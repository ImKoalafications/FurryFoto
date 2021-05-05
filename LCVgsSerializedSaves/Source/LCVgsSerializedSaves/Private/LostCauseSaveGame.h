// Lost Cause Videogames (c) 2017.

#pragma once

#include "LostCauseSaveSystem.h"
#include "GameFramework/SaveGame.h"
#include "LostCauseSaveSystemLibrary.h"
#include "LostCauseSaveGame.generated.h"



/**
 * LOST CAUSE SAVE GAME 
 */
UCLASS()
class LCVGSSERIALIZEDSAVES_API ULostCauseSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Name of the player owning this save */
	UPROPERTY(BlueprintReadWrite, Category = "LC Save Game")
	FName PlayerName;

	/** The name for this save game file */
	UPROPERTY(BlueprintReadWrite, Category = "LC Save Game")
	FString SaveSlotName;

	UPROPERTY(BlueprintReadWrite, Category = "LC Save Game")
	int32 UserIndex;

	/** Is this an old save for the game? */
	UPROPERTY(BlueprintReadWrite, Category = "LC Save Game")
	bool bOldSave;

	/** Data specific to the player pawn */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	FActorSaveData PlayerSaveData;

	/** Last saved level for this player */
	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	FName LastSavedLevelName;

	/** Searches in the array for this map, if it exists returns its corresponding index
	* if it does not exists returns -1 */
	int32 ContainsMapSaveData(FName LevelName) const;

	/** Gets the Saved map data at the input index. If the map does not exist it will return an empty structure*/
	FLCS_LevelData GetMapSaveData(int32 Index) const;

	/** Searches the ObjectSaveData array for the selected object by name. Returns its index if it exists or -1 if it does not */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LC Save Game")
	int32 ContainsObjectSaveData(FName ObjectName, bool IsAutosavedObject);

	/** Gets the saved ObjectSaveData at the input index. If the index is invalid it will return an empty structure */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LC Save Game")
	FObjectSaveData GetObjectSaveData(int32 Index, bool IsAutosavedObject);

	/** Adds the input FLCS_LevelData to the save array. It checks if it exists before adding it to prevent duplicates */
	void AddMapSaveData(FLCS_LevelData MapSaveData);

	/** Adds the input FObjectSaveData to the save array */
	UFUNCTION(BlueprintCallable, Category = "LC Save Game")
	void AddObjectSaveData(FObjectSaveData ObjectSaveData, bool IsAutosavedObject);

	/** Clears the Auto saved objects */
	void ClearAutosavedObjects();

	/** Checks the size of the solicited Saved Objects array */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LC Save Game")
	int32 GetSavedObjectsNum(bool AreAutoSavedObjects);

	/** Returns an array of all the saved objects in this save file  */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LC Save Game")
	TArray<FObjectSaveData> GetSavedObjectsData(bool AreAutoSavedObjects);

	/** FOR INTERNAL USE ONLY!
		Prevents the save game from populating the UObject pointer array. */
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, Category = "LC Save Game")
	bool bPreventPointerPopulation;


protected:

	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FLCS_LevelData> SavedMaps;

	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FObjectSaveData> SavedObjects;

	UPROPERTY(BlueprintReadOnly, Category = "LC Save Game")
	TArray<FObjectSaveData> AutoSavedObjects;

	void GetSearchArray(TArray<FObjectSaveData> &SearchArray, bool IsAutosavedObject);

	//DO NOT MODIFY THIS
	//All this section handles the auto referencing of UObjects inside Persistent Objects

public:

	/** FOR INTERNAL USE ONLY!
	*	Holds the Player UOjects references for them to be restored after map references are loaded
	*	In case a direct save or a complete save was called */
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, Category = "LC Save Game")
	TArray<UObject*> PlayerObjects;

	/** FOR INTERNAL USE ONLY!
	*	Holds the Player Pawn. Only for SinglePlayer, for multiplayer games NEVER save the player pawn
	*	In case a direct save or a complete save was called */
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, Category = "LC Save Game")
	APawn* PlayerPawn;

	/** Restores the saved UObject references into the persistent object 
	*	Fails silently if the input uObject is not a Persistent Object*/
	void RestoreUObjectReferencesIn(UObject* Object);

	/** Saves the UObject references inside the persistent object 
	*	This function will fail silently if the input UObject is not a Persistent Object*/
	void SaveUObjectReferencesIn(UObject* Object);

	/** Populates the map of pointers so they can actually be reassigned.
	*	This is a quite complex operation and should only be performed when needed. */
	void PopulateReferenceMap(UObject* WorldContextObject);

protected:

	UPROPERTY()
	TMap<FString, FObjectPropertiesContainer> ObjectReferences;

	void FailMsg(int32 ErrorNum, bool IsSaving);
	bool CheckObjectInputValidity(UObject* Object, bool IsSaving);
	bool IsPlayerPawn(UObject* Object);

};
