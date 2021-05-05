// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "LostCauseSaveSystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <EngineGlobals.h>
#include <Runtime/Engine/Classes/Engine/Engine.h>
#include "LCSObjectBase.h"
#include "EngineMinimal.h"
#include "LostCauseSaveSystemLibrary.generated.h"

/**
* Proxy archive for saving persistent objects data
*/
struct FLostCauseSaveGameArchive : public FObjectAndNameAsStringProxyArchive
{
	FLostCauseSaveGameArchive(FArchive& InInnerArchive)
		: FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};

/**
* Actor record used to store and read the data from the save file
*/
USTRUCT(BlueprintType)
struct FActorSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Actor Record")
	UClass* ActorClass;

	UPROPERTY(BlueprintReadWrite, Category = "Actor Record")
	FTransform ActorTransform;

	UPROPERTY(BlueprintReadWrite, Category = "Actor Record")
	FName ActorName = "None";

	UPROPERTY(BlueprintReadWrite, Category = "Actor Record")
	TArray<uint8> ActorData;

	bool bIsSet = false;
};

/** 
* Object record used to store and read data from the save file
*/
USTRUCT(BlueprintType)
struct FObjectSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Object Record")
	UClass* ObjectClass;

	UPROPERTY(BlueprintReadWrite, Category = "Object Record")
	FName ObjectName;

	UPROPERTY(BlueprintReadWrite, Category = "Object Record")
	TArray<uint8> ObjectData;

	UPROPERTY(BlueprintReadWrite, Category = "Object Record")
	bool AddedToRoot;

	UPROPERTY(BlueprintReadOnly, Category = "Object Record")
	UObject* Outer;
};

/**
* This structure alocates all the data for a certain map. This allows to save
* all the data for a single player in a single file.
*/
USTRUCT(BlueprintType)
struct FLCS_LevelData
{
	GENERATED_USTRUCT_BODY()

	/** Name of the map saved in this structure */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	FName LevelName = "";

	/** Names of the actors saved for this map
	*	It is used so actors added after the first save are not deleted without being saved */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FName> SavedActors;

	/** Names of the UObjects saved for this map 
	*	Only objects that are not linked to the player will be saved for the map */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FName> SavedObjects;

	/** The actual serialized persistent actors data for this map */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FActorSaveData> ActorsSaveData;

	/** The actual serialized persistent objects data for this map */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FObjectSaveData> ObjectsSaveData;

	/** Checks if this map save contains an actor */
	bool ContainsActor(AActor* Actor)
	{
		if (Actor == NULL) return false;

		bool Result = false;
		FString SavedActorName;
		FString TempActorName;

		for (int32 i = 0; i < SavedActors.Num(); i++)
		{
			SavedActorName = SavedActors[i].ToString();

			if (SavedActorName == Actor->GetName())
			{
				Result = true;
				break;
			}
		}
		return Result;
	}

	bool ContainsObject(UObject* Object)
	{
		if (Object == NULL) return false;

		if (Object->GetClass()->IsChildOf(AActor::StaticClass())) return ContainsActor(Cast<AActor>(Object));

		bool Result = false;
		FString SavedObjectName;
		FString TempObjectName;

		for (int32 i = 0; i < SavedObjects.Num(); i++)
		{
			SavedObjectName = SavedObjects[i].ToString();

			if (SavedObjectName == Object->GetName())
			{
				Result = true;
				break;
			}
		}
		return Result;
	}
};

USTRUCT(BlueprintType)
struct FObjectPropertiesContainer
{

	GENERATED_USTRUCT_BODY()

protected:

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Default")
	TMap<FString, UObject*> ObjectReferences;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Default")
	TMap<FName, FString> ObjectNames;

public:

	int32 Num()
	{
		return ObjectReferences.Num();
	}

	int32 NumNames()
	{
		return ObjectNames.Num();
	}

	void Add(FString VariableName, UObject* Object)
	{
		ObjectReferences.Add(VariableName, Object);
	}

	void Add(FName ObjectName, FString VariableName)
	{
		ObjectNames.Add(ObjectName, VariableName);
	}

	void Emplace(FString VariableName, UObject* Object)
	{
		ObjectReferences.Emplace(VariableName, Object);
	}

	void Emplace(FName ObjectName, FString VariableName)
	{
		ObjectNames.Emplace(ObjectName, VariableName);
	}

	bool ContainsObject(FString VariableName)
	{
		return ObjectReferences.Contains(VariableName);
	}

	bool ContainsName(FName ObjectName)
	{
		return ObjectNames.Contains(ObjectName);
	}

	void EmptyObjects()
	{
		ObjectReferences.Empty();
	}

	void EmptyNames()
	{
		ObjectNames.Empty();
	}

	void Remove(FString VariableName)
	{
		ObjectReferences.Remove(VariableName);
	}

	void Remove(FName ObjectName)
	{
		ObjectNames.Remove(ObjectName);
	}

	UObject* FindObject(FString Key)
	{
		return ObjectReferences.FindRef(Key);
	}

	FString FindName(FName Key)
	{
		return ObjectNames.FindRef(Key);
	}

	TArray<FString> GetKeys()
	{
		TArray<FString> OutKeys;
		ObjectReferences.GenerateKeyArray(OutKeys);
		return OutKeys;
	}

	TArray<UObject*> GetObjects()
	{
		TArray<UObject*> OutObjects;
		ObjectReferences.GenerateValueArray(OutObjects);
		return OutObjects;
	}

	TArray<FName> GetNames()
	{
		TArray<FName> OutNames;
		ObjectNames.GenerateKeyArray(OutNames);
		return OutNames;
	}
};


/**
 * 
 */
UCLASS()
class LCVGSSERIALIZEDSAVES_API ULostCauseSaveSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/** Starts the saving process. Se system goes like this:
	*	1. Look for every actor implementing the Persistent Actor interface
	*	2. Serialize the data for each one of the found objects 
	*	3. Add the data to the save file 
	*	FlushSaveData must be called manually after this function.
	*	Each map and user generates a unique structure that is saved into a file with the player name */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject))
	static void StartSaveForPersistentActors(ULostCauseSaveGame* SaveGame, UObject* WorldContextObject);
	
	/** Writes the save game data to the hard drive */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System")
	static void FlushSaveGameData(ULostCauseSaveGame* SaveGame);

	/** Gets a reference to the LostCauseSaveGame object for the desired player
	*	Loads it from file if it exists, if does not a new one is created */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject))
	static ULostCauseSaveGame* GetLCSaveGame(FName PlayerName, UObject* WorldContextObject);

	/** Loads every Persistent Actor for this map if there is something to load. Be aware that any unsaved data in those actors will be lost or set to its default values. */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject, DisplayName = "Load Persistent Objects"))
	static void LoadPersistentActors(ULostCauseSaveGame* SaveGame, UObject* WorldContextObject);

	/** The same as LoandPersistentActors, except it will use the player name */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject, DisplayName = "Load Persistent Objects by Player Name"))
	static void LoadPersistentActorsByPlayerName(FName PlayerName, UObject* WorldContextObject);

	/** Serializes the player data to be saved */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject))
	static void StartSaveForPlayerData(APawn* PlayerPawn, ULostCauseSaveGame* SaveGame, UObject* WorldContextObject);

	/** Loads the data for the pawn controlled by a designated controller
	* Including its class.
    */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject))
	static void LoadPlayerData(ULostCauseSaveGame* SaveGame, APlayerController* Controller, UObject* WorldContextObject);

	static void FinishPlayerObjectsLoad(ULostCauseSaveGame* SaveGame, APlayerController* Controller, UObject* WorldContextObject);

	// SIMPLIFIED FUNCTIONS ===================================================

	/** Directly loads everything (Player and Persistent Actors)
	 *  Requiresa VALID reference to a Lost Cause Save Game Object. 
	 *  Create it with Get LCSAveGame.
	 *  If using UE 1.17 or later wait at least one frame before passing the object reference into this function. 
	 *	No flush is requiered
	 *  Retunrs a reference to the save game object */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject))
	static void LoadGameByObject(ULostCauseSaveGame* SaveGame, APlayerController* PlayerController, UObject* WorldContextObject);

	/** Directly saves everything (Player and Persistent Actors)
	 *  Everything is managed by this function
	 *	No flush is requiered
	 *  @Param PlayerName The name for the save file
	 *  A reference to the save game object is returned */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject))
	static ULostCauseSaveGame* SaveLCSaveGame(FName PlayerName, APlayerController* PlayerController, UObject* WorldContextObject);

	/** Directly saves everything (Player and Persistent Actors) by using a Lost Cause Save Game Reference
	*  Everything is managed by this function 
	*  No Flush is requiered
	*  @Param SaveGame must be a valid LostCauseSaveGame reference or it will not work! */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject))
	static void DirectSaveLCSaveGame(ULostCauseSaveGame* SaveGame, APlayerController* PlayerController, UObject* WorldContextObject);

	/** Directly saves The persistent actors data.
	*	No flush is requiered */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject, DisplayName = "Direct Save Persistent Objects"))
	static ULostCauseSaveGame* DirectSavePersistentActors(FName PlayerName, UObject* WorldContextObject);

	/** Directly saves the player data.
	*	No flush is requiered */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject))
	static ULostCauseSaveGame* DirectSavePlayerData(FName PlayerName, APlayerController* PlayerController, UObject* WorldContextObject);

	/** Loads the last map saved for the player. Returns false if the map does not exist.
	 *	This function will only load the map, it will not load any other data. 
	 *	Call LoadLCSGame or any other relevant function for loading after the level opens.
	 *	@ Params:
	 *	@ PlayerName: The name of the player you want to load.
	 *	@ Options: The option for your map, if suported by your game mode, if not leave blank.
	 *	FOR SINGLE PLAYER ONLY */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (WorldContext = WorldContextObject))
	static bool LoadLastKnownMapForPlayer(FName PlayerName, UObject* WorldContextObject, FString Options = FString(TEXT("")));

	//Individual SERIALIZATION

	/** Serializes the data for an specific object and returns it as an array of bytes */
	UFUNCTION(BlueprintPure, Category = "Lost Cause Save System|Low Level")
	static TArray<uint8> SerializeObject_ToBytes(UObject* Object);

	/** Takes an array of bytes and serializes it into an object
	*	Use with caution! Don't try to serialize objects of incompatible classes! */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Low Level")
	static void SerializeObject_FromBytes(UObject* Object, TArray<uint8> Bytes);

	/** Serializes the data for an specific Class and returns it as an array of bytes */
	UFUNCTION(BlueprintPure, Category = "Lost Cause Save System|Low Level")
	static TArray<uint8> SerializeClass_ToBytes(TSubclassOf<UObject> Class);

	/** Takes an array of bytes and serializes it into an UClass
	*	Use with caution! Don't try to serialize incompatible Classes! */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Low Level")
	static void SerializeClass_FromBytes(TSubclassOf<UObject> Class, TArray<uint8> Bytes);

	/** Serializes the data for an specific Actor and returns it as an array of bytes */
	UFUNCTION(BlueprintPure, Category = "Lost Cause Save System|Low Level")
	static TArray<uint8> SerializeActor_ToBytes(AActor* Actor);

	/** Takes an array of bytes and serializes it into an Actor
	*	Use with caution! Don't try to serialize incompatible Actor Classes! */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Low Level")
	static void SerializeActor_FromBytes(AActor* Actor, TArray<uint8> Bytes);

	/** Returns a list of the names for all the level files in a directory.
	*	Remember to include Game when you write the directory '/Game/Maps/' 
	*	Be sure to add the directories you search in to be cooked, or it will cause crashes in your shipping builds.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lost Cause Save System|Low Level")
	static TArray<FString> GetMapNamesFromDirectory(FString Location);

	/** Get a list of Blueprint Classes from a directory (Actors and Objects). 
	*	Params:
	*	@ClassToSearch: The class of the object to be searched. Every object that is a subclass of it will also be returned.
	*	@Location: the directory to search said blueprints. Remember to include '/Game/' in your directory string. 
	*	Be sure to add the directories you search in to be cooked, or it will cause crashes in your shipping builds.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lost Cause Save System|Low Level")
	static TArray<UClass*> GetBlueprintClassesFromDirectory(UClass* ClassToSearch, FString Location);

	/** Finishes the loading process. Do not call this function directly. */
	static void FinishLoad(ULostCauseSaveGame *SaveGame, UObject* WorldContextObject);

	/** Creates an Object that inherits from LCSObjectBase. If the outer pin is left blank the World context object will be used as outer.
	*	If you intend to save this objects an outer input is mandatory. */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Low Level", meta = (WorldContext = WorldContextObject, DeterminesOutputType = Class))
	static ULCSObjectBase* CreateLCObjectOfClass(TSubclassOf<ULCSObjectBase> Class, UObject* Outer, UObject* WorldContextObject);

	/** Gets all the objects and actors that implement the PersistentObject interface
	*	This is a VERY EXPENSIVE action, refrain from using this every frame. Might cause a hitch on very big levels! */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lost Cause Save System", meta = (WorldContext = WorldContextObject))
	static void GetPersistentObjects(TArray<UObject*> &PlayerLinkedObjects, TArray<UObject*> &MapObjects, TArray<AActor*> &MapActors, UObject* WorldContextObject);

	/** Loads the game for the selected player.
	*	It handles the creation of the Save Game Object.
	*	Params:
	*	@PlayerName: The name of the player that owns the save game 
	*	@PlayerController: The controller of the player owning the save 
	*	This function is LATENT, it will not be available inside functions. For that functionality use DirectLoadGameByPlayerName (UE 4.16 and previous versions)
	*	, For UE4.17 and later use this function instead. */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Simplified", meta = (Latent, LatentInfo = "LatentInfo", HidePin = "WorldContextObject", WorldContext = WorldContextObject)) 
	static ULostCauseSaveGame* LoadGameByPlayerName(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FName PlayerName, APlayerController* PlayerController);
	
protected:

	/** Serializes and actor and gives the order to add it into the serialazed actors data array that will be flushed later */
	static FActorSaveData SerializeActorForSave(AActor* Actor);

	/** Serializes an Object to be saved. DO NOT use this function for serializing actors. Use SerializeActorsForSave(AActor) instead. */
	static FObjectSaveData SerializeObjectForSave(UObject* Object);

	/** Restores the UObject pointers for player objects and the player pawn */
	static void RestoreUObjectReferencesForPlayer(ULostCauseSaveGame* SaveGame);

public:

	//BLUEPRINT STRUCTURE SERIALIZATION

	/** Serializes a given structure to bytes */
	UFUNCTION(BlueprintCallable, Category = "Lost Cause Save System|Low Level", meta = (CustomStructureParam = "Structure"))
	static TArray<uint8> SerializeStructure_ToBytes(UStructProperty* Structure);

};
