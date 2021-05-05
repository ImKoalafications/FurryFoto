// Fill out your copyright notice in the Description page of Project Settings.

#include "LostCauseSaveSystemLibrary.h"
//#include "LostCauseSaveSystem.h"
#include "LostCauseSaveGame.h"
#include "PersistentObjectInterface.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "LCTimerHelper.h"
#include "LoadGameAction.h"


void ULostCauseSaveSystemLibrary::LoadPlayerData(ULostCauseSaveGame* SaveGame, APlayerController* Controller, UObject* WorldContextObject)
{
	if (Controller != NULL && SaveGame != NULL)
	{
		if (SaveGame->PlayerSaveData.ActorName == "None") return;

		APawn* PlayerPawn = Controller->GetPawn();

		if (PlayerPawn != NULL)
		{
			//we spawn a dummy pawn while we destroy and respawn the one saved.
			Controller->UnPossess();
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FTransform TempTransform = FTransform(FVector(0.f, 0.f, 99999.f));
			ADefaultPawn* NewPawn = WorldContextObject->GetWorld()->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), TempTransform, SP);
			Controller->Possess(NewPawn);

			PlayerPawn->K2_DestroyActor();

			// We load the player related objects
			if (SaveGame->GetSavedObjectsNum(true) > 0)
			{
				TArray<AActor*> PersistentActors;
				TArray<UObject*> MapObjects;
				TArray<UObject*> PlayerObjects;

				GetPersistentObjects(PlayerObjects, MapObjects, PersistentActors, WorldContextObject);

				//Only the objects that exist in the save game are destroyed
				for (int i = 0; i < PlayerObjects.Num(); i++)
				{
					if (SaveGame->ContainsObjectSaveData(FName(*PlayerObjects[i]->GetName()), true))
					{
						if (PlayerObjects[i]->IsValidLowLevel())
						{
							if (PlayerObjects[i]->IsRooted()) MapObjects[i]->RemoveFromRoot();

							PlayerObjects[i]->ConditionalBeginDestroy();
							PlayerObjects[i]->MarkPendingKill();
							PlayerObjects[i] = NULL;
						}
					}
				}
			}
			GEngine->ForceGarbageCollection(true); //UE 4.18
			//Controller->GetWorld()->ForceGarbageCollection(true); //UE 4.17 and before


			ULCTimerHelper* TimerHelper = Cast<ULCTimerHelper>(UGameplayStatics::SpawnObject(ULCTimerHelper::StaticClass(), WorldContextObject));

			if (TimerHelper != NULL)
			{
				TimerHelper->AddToRoot();
				TimerHelper->StartPlayerLoadTimer(SaveGame, Controller, WorldContextObject);
				TimerHelper = NULL;
			}
		}
	}
}

void ULostCauseSaveSystemLibrary::FinishPlayerObjectsLoad(ULostCauseSaveGame * SaveGame, APlayerController* Controller, UObject * WorldContextObject)
{
	if (SaveGame != NULL && WorldContextObject != NULL && Controller != NULL)
	{
		Controller->UnPossess();
		APawn* CurrPawn = Controller->GetPawn();
		//We load the player data
		FMemoryReader PMemoryReader(SaveGame->PlayerSaveData.ActorData, true);
		FLostCauseSaveGameArchive Arch(PMemoryReader);

		FTransform NewTransform = FTransform();
		if (FName(*UGameplayStatics::GetCurrentLevelName(WorldContextObject->GetWorld(), true)) == SaveGame->LastSavedLevelName)
		{
			NewTransform = SaveGame->PlayerSaveData.ActorTransform;
		}
		else
		{
			AGameModeBase* GameMode = UGameplayStatics::GetGameMode(WorldContextObject);
			if (GameMode != NULL)
			{
				AActor* PlayerStart = GameMode->ChoosePlayerStart(Controller);
				if (PlayerStart != NULL) NewTransform = PlayerStart->GetTransform();
			}
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Controller;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.Name = SaveGame->PlayerSaveData.ActorName;

		APawn* NewPawn = WorldContextObject->GetWorld()->SpawnActor<APawn>(SaveGame->PlayerSaveData.ActorClass, NewTransform, SpawnParameters);


		if (NewPawn != NULL)
		{
			NewPawn->Serialize(Arch);
			Controller->Possess(NewPawn);
			Controller->SetControlRotation(NewPawn->GetActorRotation());
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("LoadPlayerData: NewPawn is not valid and cannot be possessed"));
		}

		if(CurrPawn) CurrPawn->SetLifeSpan(0.1);
		//

		TArray<UObject*> RestoredObjects;
		if (SaveGame->GetSavedObjectsNum(true) > 0)
		{
			TArray<FObjectSaveData> SavedObjects = SaveGame->GetSavedObjectsData(true);
			for (int i = 0; i < SavedObjects.Num(); i++)
			{

				UObject* NewOuter = NULL;
				if (SavedObjects[i].Outer != NULL) NewOuter = SavedObjects[i].Outer;
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString("My Outer was not Saved or it was NULL! Player controller will be my new outer"));
					NewOuter = Controller;
				}

				UObject* NewObj = NewObject<UObject>(NewOuter, SavedObjects[i].ObjectClass, SavedObjects[i].ObjectName);

				FMemoryReader MemoryReader(SavedObjects[i].ObjectData, true);

				FLostCauseSaveGameArchive Ar(MemoryReader);
				NewObj->Serialize(Ar);
				
				//We add the object to the root set if it was rooted at the moment of saving so the user doesnt have to do it by itself.
				if (SavedObjects[i].AddedToRoot == true) NewObj->AddToRoot();

				RestoredObjects.Add(NewObj);
			}
		} else GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString("There are no player object saved"));

		SaveGame->PlayerPawn = Controller->GetPawn();
		SaveGame->PlayerObjects = RestoredObjects;
		if (!SaveGame->bPreventPointerPopulation)
		{
			SaveGame->PopulateReferenceMap(WorldContextObject);
			RestoreUObjectReferencesForPlayer(SaveGame);
		}
	}
}

void ULostCauseSaveSystemLibrary::LoadGameByObject(ULostCauseSaveGame* SaveGame, APlayerController* PlayerController, UObject* WorldContextObject)
{
	if (PlayerController == NULL || !SaveGame) return;

	//Sometimes Objects interact with players instantly after spawned. By moving the player
	//to a safe location before loading we prevent this issue
	if (PlayerController != NULL)
	{
		APawn* PlayerPawn = PlayerController->GetPawn();
		if (PlayerPawn != NULL) PlayerPawn->SetActorLocation(PlayerPawn->GetActorLocation() + FVector(0, 0, 99999));
	}

	SaveGame->bPreventPointerPopulation = true;
	LoadPersistentActors(SaveGame, WorldContextObject);
	LoadPlayerData(SaveGame, PlayerController, WorldContextObject);
}

ULostCauseSaveGame * ULostCauseSaveSystemLibrary::SaveLCSaveGame(FName PlayerName, APlayerController * PlayerController, UObject* WorldContextObject)
{
	if (PlayerName == "None" || PlayerName == "" || PlayerController == NULL) return NULL;

	ULostCauseSaveGame* SaveGame = GetLCSaveGame(PlayerName, WorldContextObject);
	if (SaveGame != NULL) DirectSaveLCSaveGame(SaveGame, PlayerController, WorldContextObject);
	return SaveGame;
}

void ULostCauseSaveSystemLibrary::DirectSaveLCSaveGame(ULostCauseSaveGame * SaveGame, APlayerController * PlayerController, UObject* WorldContextObject)
{
	if (SaveGame == NULL || PlayerController == NULL) return;
	APawn* PlayerPawn = PlayerController->GetPawn();

	//Preapares the data for saving
	StartSaveForPlayerData(PlayerPawn, SaveGame, WorldContextObject);
	StartSaveForPersistentActors(SaveGame, WorldContextObject);

	//saves the data to the hard drive
	FlushSaveGameData(SaveGame);
}

ULostCauseSaveGame * ULostCauseSaveSystemLibrary::DirectSavePersistentActors(FName PlayerName, UObject* WorldContextObject)
{
	if (PlayerName == "" || PlayerName == "None") return NULL;

	ULostCauseSaveGame* SaveGame = GetLCSaveGame(PlayerName, WorldContextObject);

	StartSaveForPersistentActors(SaveGame, WorldContextObject);
	FlushSaveGameData(SaveGame);

	return SaveGame;
}

ULostCauseSaveGame* ULostCauseSaveSystemLibrary::DirectSavePlayerData(FName PlayerName, APlayerController * PlayerController, UObject* WorldContextObject)
{
	if (PlayerName == "" || PlayerName == "None" || PlayerController == NULL) return NULL;

	ULostCauseSaveGame* SaveGame = GetLCSaveGame(PlayerName, WorldContextObject);

	StartSaveForPlayerData(PlayerController->GetPawn(), SaveGame, WorldContextObject);
	FlushSaveGameData(SaveGame);

	return SaveGame;
}

bool ULostCauseSaveSystemLibrary::LoadLastKnownMapForPlayer(FName PlayerName, UObject* WorldContextObject, FString Options)
{
	if (PlayerName == "None" || PlayerName == "") return false;
	bool Result = false;

	ULostCauseSaveGame* SaveGame = GetLCSaveGame(PlayerName, WorldContextObject);
	if (SaveGame != NULL)
	{
		if (SaveGame->ContainsMapSaveData(SaveGame->LastSavedLevelName))
		{
			Result = true;
			UGameplayStatics::OpenLevel(GWorld, SaveGame->LastSavedLevelName);
		}
		else Result = false;
	}
	
	return Result;
}

TArray<uint8> ULostCauseSaveSystemLibrary::SerializeObject_ToBytes(UObject * Object)
{

	TArray<uint8> Result;

	FMemoryWriter MemoryWriter(Result, true);
	FLostCauseSaveGameArchive Arch(MemoryWriter);
	Object->Serialize(Arch);

	return Result;
}

void ULostCauseSaveSystemLibrary::SerializeObject_FromBytes(UObject * Object, TArray<uint8> Bytes)
{
	if (Object != NULL)
	{
		FMemoryReader MemoryReader(Bytes, true);
		FLostCauseSaveGameArchive Arch(MemoryReader);
		Object->Serialize(Arch);
	}
}

TArray<uint8> ULostCauseSaveSystemLibrary::SerializeClass_ToBytes(TSubclassOf<UObject> Class)
{
	TArray<uint8> Result;
	
	FMemoryWriter MemoryWriter(Result, true);
	FLostCauseSaveGameArchive Arch(MemoryWriter);
	Class->Serialize(Arch);

	return Result;
}

void ULostCauseSaveSystemLibrary::SerializeClass_FromBytes(TSubclassOf<UObject> Class, TArray<uint8> Bytes)
{
	if (Class != NULL)
	{
		FMemoryReader MemoryReader(Bytes, true);
		FLostCauseSaveGameArchive Arch(MemoryReader);
		Class->Serialize(Arch);
	}
}

TArray<uint8> ULostCauseSaveSystemLibrary::SerializeActor_ToBytes(AActor* Actor)
{
	TArray<uint8> Result;
	FMemoryWriter MemoryWriter(Result, true);
	FLostCauseSaveGameArchive Arch(MemoryWriter);
	Actor->Serialize(Arch);
	return Result;
}

void ULostCauseSaveSystemLibrary::SerializeActor_FromBytes(AActor* Actor, TArray<uint8> Bytes)
{
	if (Actor != NULL)
	{
		FMemoryReader MemoryReader(Bytes, true);
		FLostCauseSaveGameArchive Arch(MemoryReader);
		Actor->Serialize(Arch);
	}
}

TArray<FString> ULostCauseSaveSystemLibrary::GetMapNamesFromDirectory(FString Location)
{
	TArray<FString> Result;
	UObjectLibrary* ObjectLibrary = UObjectLibrary::CreateLibrary(UWorld::StaticClass(), false, true);

	ObjectLibrary->LoadAssetDataFromPath(*Location);
	TArray<FAssetData> AssetsData;
	ObjectLibrary->GetAssetDataList(AssetsData);

	for (int32 i = 0; i < AssetsData.Num(); i++)
	{
		Result.Add(AssetsData[i].AssetName.ToString());
	}

	return Result;
}

TArray<UClass*> ULostCauseSaveSystemLibrary::GetBlueprintClassesFromDirectory(UClass * ClassToSearch, FString Location)
{
	TArray<UClass*> Result;
	if (ClassToSearch == NULL) return Result;

	UObjectLibrary* ObjectLibrary = UObjectLibrary::CreateLibrary(ClassToSearch, true, true);
	ObjectLibrary->AddToRoot();
	ObjectLibrary->LoadBlueprintsFromPath(*Location);

	TArray<UBlueprintGeneratedClass*> ClassArray;
	ObjectLibrary->GetObjects<UBlueprintGeneratedClass>(ClassArray);

	for (int32 i = 0; i < ClassArray.Num(); i++)
	{
		Result.Add(ClassArray[i]);
	}

	return Result;
}

FActorSaveData ULostCauseSaveSystemLibrary::SerializeActorForSave(AActor* Actor)
{
	FActorSaveData ActorSaveData;
	if (Actor != NULL)
	{
		ActorSaveData.ActorClass = Actor->GetClass();
		ActorSaveData.ActorName = FName(*Actor->GetName());
		ActorSaveData.ActorTransform = Actor->GetTransform();

		FMemoryWriter MemoryWriter(ActorSaveData.ActorData, true);

		//Uses a wrapper archive that converts FNames and UObjects*'s to strings that can ve read back in
		FLostCauseSaveGameArchive Arch(MemoryWriter);

		//Serializes the Actor
		Actor->Serialize(Arch);
	}
	return ActorSaveData;
}

FObjectSaveData ULostCauseSaveSystemLibrary::SerializeObjectForSave(UObject * Object)
{
	FObjectSaveData ObjectSaveData;
	if (Object != NULL)
	{
		ObjectSaveData.ObjectClass = Object->GetClass();
		ObjectSaveData.ObjectName = FName(*Object->GetName());
		ObjectSaveData.AddedToRoot = Object->IsRooted();
		ObjectSaveData.Outer = Object->GetOuter();

		FMemoryWriter MemoryWriter(ObjectSaveData.ObjectData, true);
		FLostCauseSaveGameArchive Arch(MemoryWriter);

		Object->Serialize(Arch);
	}
	return ObjectSaveData;
}

void ULostCauseSaveSystemLibrary::RestoreUObjectReferencesForPlayer(ULostCauseSaveGame* SaveGame)
{
	if (SaveGame != NULL)
	{
		SaveGame->RestoreUObjectReferencesIn(SaveGame->PlayerPawn);

		UObject* CurrObj = NULL;
		for (int i = 0; i < SaveGame->PlayerObjects.Num(); i++)
		{
			CurrObj = SaveGame->PlayerObjects[i];
			SaveGame->RestoreUObjectReferencesIn(CurrObj);
			IPersistentObjectInterface::Execute_ObjectLoadComplete(CurrObj);
		}
	}
}

TArray<uint8> ULostCauseSaveSystemLibrary::SerializeStructure_ToBytes(UStructProperty * Structure)
{
	TArray<uint8> Bytes;

	if (Structure != NULL)
	{
		FBufferArchive Archive(true);
		Structure->Struct->SerializeBin(Archive, Structure->Struct);
	}


	return Bytes;
}

void ULostCauseSaveSystemLibrary::GetPersistentObjects(TArray<UObject*> &PlayerLinkedObjects, TArray<UObject*> &MapObjects, TArray<AActor*> &MapActors, UObject* WorldContextObject)
{
	UGameplayStatics::GetAllActorsWithInterface(WorldContextObject, UPersistentObjectInterface::StaticClass(), MapActors);

	TArray<UObject*> TemPlayerObjects;
	TArray<UObject*> TempMapObjects;
	UObject* CurrentObject;

	for (TObjectIterator<UObject> Iterator; Iterator; ++Iterator)
	{
		CurrentObject = *Iterator;
		if (!CurrentObject->GetClass()->ImplementsInterface(UPersistentObjectInterface::StaticClass())
			|| CurrentObject->GetClass()->IsChildOf(AActor::StaticClass())) continue;
		
		if (IPersistentObjectInterface::Execute_IsObjectLinkedToPlayer(*Iterator) == true) TemPlayerObjects.Add(*Iterator);
		else TempMapObjects.Add(*Iterator);
	}

	MapObjects = TempMapObjects;
	PlayerLinkedObjects = TemPlayerObjects;
}

ULostCauseSaveGame* ULostCauseSaveSystemLibrary::LoadGameByPlayerName(UObject * WorldContextObject, FLatentActionInfo LatentInfo, FName PlayerName, APlayerController* PlayerController)
{
	if (PlayerController == NULL || WorldContextObject == NULL)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("Player Controller and/or World context are not valid"));
		return nullptr;
	}

	if (PlayerName == "None" || PlayerName == "")
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString("PlayerName: " + PlayerName.ToString() + " is not valid!"));
		return nullptr;
	}

	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("LoadGameByPlayerName(): Function Called correctly"));

	//ULostCauseSaveGame* SaveGame = GetLCSaveGame(PlayerName, WorldContextObject);

	//if (!SaveGame) return NULL;

	FLoadGameAction* LoadGameAction = NULL;

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	if (World)
	{
		FLatentActionManager& LAM = World->GetLatentActionManager();
		if(LAM.FindExistingAction<FLoadGameAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
		{
			LoadGameAction = new FLoadGameAction(LatentInfo, PlayerName, nullptr, PlayerController, WorldContextObject);
			LAM.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, LoadGameAction);
		} else GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("LAM: FLoadGameAction alreadt exists"));

	} else GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("Tere is no world context"));

	if (LoadGameAction) return LoadGameAction->SaveGame;
	else return nullptr;
}

void ULostCauseSaveSystemLibrary::StartSaveForPersistentActors(ULostCauseSaveGame* SaveGame, UObject* WorldContextObject)
{
	TArray<AActor*> PersistentActors;
	TArray<UObject*> PlayerObjects; //This array will not be used. There's room for optimization, but will require further testing.
	TArray<UObject*> MapObjects;
	
	GetPersistentObjects(PlayerObjects, MapObjects, PersistentActors, WorldContextObject);

	// If there is saved data for this map we delete it so it can be repopulated with the new one
	FName LevelName = FName(*UGameplayStatics::GetCurrentLevelName(GWorld, true));
	FLCS_LevelData LevelData = SaveGame->GetMapSaveData(SaveGame->ContainsMapSaveData(LevelName));

	if (LevelData.LevelName == "None")
	{
		LevelData.LevelName = LevelName;
	}
	else
	{
		LevelData.ActorsSaveData.Empty();
		LevelData.SavedActors.Empty();
		LevelData.ObjectsSaveData.Empty();
		LevelData.SavedObjects.Empty();
	}
	

	if ((PersistentActors.IsValidIndex(0) || MapObjects.IsValidIndex(0)) && (SaveGame != NULL))
	{
		//Adds objects to save file
		if (MapObjects.IsValidIndex(0))
		{
			for (int i = 0; i < MapObjects.Num(); i++)
			{
				if (MapObjects[i] != NULL)
				{
					LevelData.ObjectsSaveData.Add(SerializeObjectForSave(MapObjects[i]));
					LevelData.SavedObjects.Add(FName(*MapObjects[i]->GetName()));
					IPersistentObjectInterface::Execute_ObjectSaveComplete(MapObjects[i]);
				}
			}
		}

		//Adds actors to save file
		if (PersistentActors.IsValidIndex(0))
		{
			for (int i = 0; i < PersistentActors.Num(); i++)
			{
				if (PersistentActors[i] != NULL)
				{
					LevelData.ActorsSaveData.Add(SerializeActorForSave(PersistentActors[i]));
					LevelData.SavedActors.Add(FName(*PersistentActors[i]->GetName()));
					IPersistentObjectInterface::Execute_ObjectSaveComplete(PersistentActors[i]);
				}
			}
		}

		SaveGame->AddMapSaveData(LevelData);
	}
}

void ULostCauseSaveSystemLibrary::FlushSaveGameData(ULostCauseSaveGame * SaveGame)
{
	//We need a player name so we can actually create the corresponding save file
	//the creation of the save game is handled by another function prior to the calling of this one
	if (SaveGame != NULL)
	{
		UGameplayStatics::SaveGameToSlot(SaveGame, SaveGame->SaveSlotName, SaveGame->UserIndex);
	}
}

ULostCauseSaveGame * ULostCauseSaveSystemLibrary::GetLCSaveGame(FName PlayerName, UObject* WorldContextObject)
{
	FString SlotName = FString(PlayerName.ToString());

	//we always load from player index 0, as every player should have only one save
	ULostCauseSaveGame* OutSaveGame = Cast<ULostCauseSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	//if the file does not exist, we will create a new one
	if (OutSaveGame == NULL)
	{
		OutSaveGame = Cast<ULostCauseSaveGame>(UGameplayStatics::CreateSaveGameObject(ULostCauseSaveGame::StaticClass()));
		OutSaveGame->SaveSlotName = SlotName;
		OutSaveGame->PlayerName = PlayerName;
		OutSaveGame->UserIndex = 0;
		OutSaveGame->bOldSave = false;
	}

	return OutSaveGame;
}

void ULostCauseSaveSystemLibrary::LoadPersistentActorsByPlayerName(FName PlayerName, UObject* WorldContextObject)
{
	LoadPersistentActors(GetLCSaveGame(PlayerName, WorldContextObject), WorldContextObject);
}

void ULostCauseSaveSystemLibrary::StartSaveForPlayerData(APawn * PlayerPawn, ULostCauseSaveGame* SaveGame, UObject* WorldContextObject)
{
	if (PlayerPawn != NULL && SaveGame != NULL)
	{
		FActorSaveData PlayerSaveData = SerializeActorForSave(PlayerPawn);
		SaveGame->SaveUObjectReferencesIn(PlayerPawn);
		SaveGame->PlayerSaveData = PlayerSaveData;
		SaveGame->LastSavedLevelName = FName(*UGameplayStatics::GetCurrentLevelName(GWorld, true));

		//Auto saves player linked objects
		TArray<AActor*> PersistentActors;	//UNUSED IN THIS FUNCTION
		TArray<UObject*> MapObjects;		//UNUSED IN THIS FUNCTION
		TArray<UObject*> PlayerObjects;

		GetPersistentObjects(PlayerObjects, MapObjects, PersistentActors, WorldContextObject);

		//If there are previously saved objects we delete them so they will be resaved.
		if (SaveGame->GetSavedObjectsNum(true) > 0) SaveGame->ClearAutosavedObjects();

		//If we catched any player linked objects
		if (PlayerObjects.IsValidIndex(0))
		{
			for (int i = 0; i < PlayerObjects.Num(); i++)
			{
				SaveGame->AddObjectSaveData(SerializeObjectForSave(PlayerObjects[i]), true);
				IPersistentObjectInterface::Execute_ObjectSaveComplete(PlayerObjects[i]);
			}

		}
	}
}

void ULostCauseSaveSystemLibrary::LoadPersistentActors(ULostCauseSaveGame * SaveGame, UObject* WorldContextObject)
{
	if (SaveGame != NULL)
	{
		//we check if there is data for this map
		FName LevelName = FName(*UGameplayStatics::GetCurrentLevelName(GWorld, true));
		FLCS_LevelData LevelData = SaveGame->GetMapSaveData(SaveGame->ContainsMapSaveData(LevelName));

		if (LevelData.LevelName != "None")
		{
			TArray<AActor*> PersistentActors;
			TArray<UObject*> MapObjects;
			TArray<UObject*> PlayerObjects;

			GetPersistentObjects(PlayerObjects, MapObjects, PersistentActors, WorldContextObject);

			//Only the objects that exist in the save game are destroyed
			for (int i = 0; i < MapObjects.Num(); i++)
			{
				if (LevelData.ContainsObject(MapObjects[i]))
				{
					if (MapObjects[i]->IsValidLowLevel())
					{
						if (MapObjects[i]->IsRooted()) MapObjects[i]->RemoveFromRoot();

						MapObjects[i]->ConditionalBeginDestroy();
						MapObjects[i]->MarkPendingKill();
						MapObjects[i] = NULL;
					}
				}
			}

			//Only the actors that exist in the save are destroyed
			for (int i = 0; i < PersistentActors.Num(); i++)
			{
				if (LevelData.ContainsActor(PersistentActors[i]))
				{
					PersistentActors[i]->K2_DestroyActor();
					PersistentActors[i] = NULL;
				}
			}
			GEngine->ForceGarbageCollection(true); //UE 4.18
			//GWorld->ForceGarbageCollection(true); //UE 4.17 and before

			ULCTimerHelper* TimerHelper = Cast<ULCTimerHelper>(UGameplayStatics::SpawnObject(ULCTimerHelper::StaticClass(), WorldContextObject));

			if (TimerHelper != NULL)
			{
				TimerHelper->AddToRoot();
				TimerHelper->StartLoadTimer(SaveGame, WorldContextObject);
			}
		}
	} 
}

void ULostCauseSaveSystemLibrary::FinishLoad(ULostCauseSaveGame * SaveGame, UObject* WorldContextObject)
{
	if (SaveGame != NULL)
	{
		//we check if there is data for this map
		FName LevelName = FName(*UGameplayStatics::GetCurrentLevelName(GWorld, true));
		FLCS_LevelData LevelData = SaveGame->GetMapSaveData(SaveGame->ContainsMapSaveData(LevelName));

		TArray<UObject*> LoadedObjects;

		if (LevelData.LevelName != "None")
		{
			TArray<FObjectSaveData> ObjectsSaveData = LevelData.ObjectsSaveData;
			TArray<FActorSaveData> ActorsSaveData = LevelData.ActorsSaveData;

			//Objects Spawn before actors
			for (int i = 0; i < ObjectsSaveData.Num(); i++)
			{
				UObject* NewObj = NewObject<UObject>(WorldContextObject, ObjectsSaveData[i].ObjectClass, ObjectsSaveData[i].ObjectName);

				FMemoryReader MemoryReader(ObjectsSaveData[i].ObjectData, true);
				FLostCauseSaveGameArchive Ar(MemoryReader);
				NewObj->Serialize(Ar);

				//We add the object to the root set if it was rooted at the moment of saving so the user doesnt have to do it by itself.
				if (ObjectsSaveData[i].AddedToRoot == true) NewObj->AddToRoot();

				LoadedObjects.Add(NewObj);
			}

			//Actor Spawning			
			for (int i = 0; i < ActorsSaveData.Num(); i++)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				SpawnParameters.Name = ActorsSaveData[i].ActorName;

				AActor* NewActor = GWorld->SpawnActor<AActor>(ActorsSaveData[i].ActorClass, ActorsSaveData[i].ActorTransform, SpawnParameters);

				FMemoryReader MemoryReader(ActorsSaveData[i].ActorData, true);
				FLostCauseSaveGameArchive Ar(MemoryReader);
				NewActor->Serialize(Ar);

				LoadedObjects.Add(NewActor);
			}
		}

		//We always call for repopulation when actors and map objects finish loading since its the las loading operation
		//It is a forced operation. When calling direct saves or complete saves it will prevent the player object loading from repopulating pointers.
		bool bWasPreventingPP = SaveGame->bPreventPointerPopulation;
		SaveGame->bPreventPointerPopulation = false;
		SaveGame->PopulateReferenceMap(WorldContextObject);

		if (bWasPreventingPP) RestoreUObjectReferencesForPlayer(SaveGame);

		if (LevelData.LevelName != "None")
		{
			for (int i = 0; i < LoadedObjects.Num(); i++)
			{
				SaveGame->RestoreUObjectReferencesIn(LoadedObjects[i]);
				IPersistentObjectInterface::Execute_ObjectLoadComplete(LoadedObjects[i]);
			}
		}
	}
}

ULCSObjectBase * ULostCauseSaveSystemLibrary::CreateLCObjectOfClass(TSubclassOf<ULCSObjectBase> Class, UObject * Outer, UObject * WorldContextObject)
{
	ULCSObjectBase* Result = NULL;

	if (Outer == NULL && WorldContextObject) Outer = WorldContextObject->GetWorld();

	if (Class && Outer && WorldContextObject)
	{
		Result = Cast<ULCSObjectBase>(UGameplayStatics::SpawnObject(Class, Outer));
		if (Result) Result->SetWorld(WorldContextObject->GetWorld());
	}

	return Result;
}
