// Fill out your copyright notice in the Description page of Project Settings.

#include "LostCauseSaveGame.h"
//#include "LostCauseSaveSystem.h"
#include "PersistentObjectInterface.h"
#include <Runtime/Engine/Classes/Engine/Engine.h>



int32 ULostCauseSaveGame::ContainsMapSaveData(FName LevelName) const
{
	int32 Result = -1;
	FLCS_LevelData CurrentLevelData;

	for (int i = 0; i < SavedMaps.Num(); i++)
	{
		CurrentLevelData = SavedMaps[i];

		if (CurrentLevelData.LevelName == LevelName)
		{
			Result = i;
			break;
		}
	}

	return Result;
}

FLCS_LevelData ULostCauseSaveGame::GetMapSaveData(int32 Index) const
{
	FLCS_LevelData ResultData;

	if (SavedMaps.Num() - 1 >= Index && Index > -1)
	{
		ResultData = SavedMaps[Index];
	}

	return ResultData;
}

int32 ULostCauseSaveGame::ContainsObjectSaveData(FName ObjectName, bool IsAutosavedObject = false)
{
	int32 Result = -1;
	FObjectSaveData CurrentObjectData;

	TArray<FObjectSaveData> SearchArray;
	GetSearchArray(SearchArray, IsAutosavedObject);

	for (int i = 0; i < SearchArray.Num(); i++)
	{
		CurrentObjectData = SearchArray[i];

		if (CurrentObjectData.ObjectName == ObjectName)
		{
			Result = i;
			break;
		}
	}

	return Result;
}

FObjectSaveData ULostCauseSaveGame::GetObjectSaveData(int32 Index, bool IsAutosavedObject = false)
{
	FObjectSaveData ObjectSaveData;

	TArray<FObjectSaveData> SearchArray;
	GetSearchArray(SearchArray, IsAutosavedObject);

	if (SearchArray.IsValidIndex(Index))
	{
		ObjectSaveData = SearchArray[Index];
	}
	return ObjectSaveData;
}

void ULostCauseSaveGame::AddMapSaveData(FLCS_LevelData MapSaveData)
{
	int32 LevelDataIndex = ContainsMapSaveData(MapSaveData.LevelName);
	if (LevelDataIndex > -1)
	{
		SavedMaps[LevelDataIndex] = MapSaveData;
	}
	else
	{
		SavedMaps.Add(MapSaveData);
	}
}

void ULostCauseSaveGame::AddObjectSaveData(FObjectSaveData ObjectSaveData, bool IsAutosavedObject = false)
{
	int32 ObjectDataIndex= ContainsObjectSaveData(ObjectSaveData.ObjectName, IsAutosavedObject);
	TArray<FObjectSaveData> SearchArray;
	GetSearchArray(SearchArray, IsAutosavedObject);

	if (ObjectDataIndex > -1)
	{
		SearchArray[ObjectDataIndex] = ObjectSaveData;
	}
	else
	{
		SearchArray.Add(ObjectSaveData);
	}

	if (IsAutosavedObject == true) AutoSavedObjects = SearchArray;
	else SavedObjects = SearchArray;
}

void ULostCauseSaveGame::ClearAutosavedObjects()
{
	AutoSavedObjects.Empty();
}

int32 ULostCauseSaveGame::GetSavedObjectsNum(bool AreAutoSavedObjects)
{
	TArray<FObjectSaveData> SearchArray;
	GetSearchArray(SearchArray, AreAutoSavedObjects);

	return SearchArray.Num();
}

TArray<FObjectSaveData> ULostCauseSaveGame::GetSavedObjectsData(bool AreAutoSavedObjects = false)
{
	TArray<FObjectSaveData> SearchArray;
	GetSearchArray(SearchArray, AreAutoSavedObjects);
	return SearchArray;
}

void ULostCauseSaveGame::GetSearchArray(TArray<FObjectSaveData> &SearchArray, bool IsAutosavedObject)
{
	if (IsAutosavedObject == true) SearchArray = AutoSavedObjects;
	else SearchArray = SavedObjects;
}

void ULostCauseSaveGame::RestoreUObjectReferencesIn(UObject * Object)
{
	if(!CheckObjectInputValidity(Object, false)) return;

	FString PropertyOwner = Object->GetName();
	//if (IsPlayerPawn(Object)) PropertyOwner = PlayerName.ToString();
	//else PropertyOwner = Object->GetName();

	FObjectPropertiesContainer PropContainer;

	//If there is no container for this object we exit. This object has never been saved before.
	if (!ObjectReferences.Contains(PropertyOwner)) { /*FailMsg(2, false)*/; return; }

	PropContainer = ObjectReferences.FindRef(PropertyOwner);

	TArray<FString> PropNames = PropContainer.GetKeys();
	//if the container holds no keys for this object, we exit.
	if (PropNames.Num() == 0) { FailMsg(3, false); return; }

	int32 RestoredProps = 0;
	UObjectProperty* CurrentProp;
	for (int32 i = 0; i < PropNames.Num(); i++)
	{
		CurrentProp = FindField<UObjectProperty>(Object->GetClass(), FName(*PropNames[i]));
		if (CurrentProp)
		{
			UObject* TargetPtr = PropContainer.FindObject(PropNames[i]);
			if (!TargetPtr) { /*FailMsg(4, false);*/ continue; }
			CurrentProp->SetObjectPropertyValue_InContainer(Object, TargetPtr);
			RestoredProps++;
		}
	}
}

void ULostCauseSaveGame::SaveUObjectReferencesIn(UObject * Object)
{
	if (!CheckObjectInputValidity(Object, true)) return;

	FString PropertyOwner = Object->GetName();
	//if (IsPlayerPawn(Object)) PropertyOwner = PlayerName.ToString();
	//else PropertyOwner = Object->GetName();

	FObjectPropertiesContainer PropContainer;

	//We iterate over the object's properties so we can gather the references to UObjects
	//TArray<UObjectProperty> Properties;
	int32 FoundProperties = 0;
	for (TFieldIterator<UObjectProperty> It(Object->GetClass()); It; ++It)
	{
		UObjectProperty* CurrentProperty = *It;
		
		// skip properties without any setup data and non save game uproperties	
		if (CurrentProperty->HasAnyPropertyFlags(CPF_Transient) ||
			!CurrentProperty->HasAnyPropertyFlags(CPF_SaveGame))
		{
			//FailMsg(5, true);
			continue;
		}

		//Property Adress
		UObject* ObjPtr = CurrentProperty->GetObjectPropertyValue_InContainer(Object);

		//If the pointer to save is trully valid
		if (ObjPtr != NULL)
		{
			//We ensure ourselves this is not an AActor reference nor a UActorComponent reference
			if (ObjPtr->IsA(AActor::StaticClass()) || ObjPtr->IsA(UActorComponent::StaticClass())) continue;

			//We only save the name of the referenced object since the real pointers will be useless on next load.
			FString PropertyName = CurrentProperty->GetName();
			PropContainer.Add(ObjPtr->GetFName(),PropertyName);

			FoundProperties++;
		}
		//else FailMsg(6,true);
	}
	//If we have properties to save we emplace the data, replacing everything that might already exist.
	if (FoundProperties > 0)
	{
		ObjectReferences.Emplace(PropertyOwner, PropContainer);
	}
	//If we found nothing we check if there was something saved and delete it, as there should be no data saved for this object.
	else if (ObjectReferences.Contains(PropertyOwner)) ObjectReferences.Remove(PropertyOwner);
}

void ULostCauseSaveGame::FailMsg(int32 ErrorNum, bool IsSaving = false)
{
	FString FunctionName;
	if (IsSaving) FunctionName = FString("SaveUObjectReferencesIn(): ");
	else FunctionName = FString("RestoreUObjectReferencesIn(): ");

	float DisplayTime = 10.0f;

	switch(ErrorNum)
	{
	case 0:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName+"Input Object is not Valid"));
		break;
	case 1:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName+"Input Object is not a Persistent Object nor a Player Controlled Pawn"));
		break;
	case 2:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName+"There is no container for the input Object!"));
		break;
	case 3:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName + "PropNames.Num() == 0!"));
		break;
	case 4:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName + "TargetPtr is not valid!"));
		break;
	case 5:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName + "Property Ignored"));
		break;
	case 6:
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString(FunctionName + "ObjPtr is not Valid"));
		break;
	}
}

bool ULostCauseSaveGame::CheckObjectInputValidity(UObject * Object, bool IsSaving)
{
	if (!Object) { /*FailMsg(0, false);*/ return false; }
	if (!Cast<IPersistentObjectInterface>(Object))
	{
		bool bFail = true;
		/** We check if not a persistent object, if we are passing a pawn.*/
		if (IsPlayerPawn(Object)) bFail = false;

		if (bFail)
		{
			//FailMsg(1, false);
			return false;
		}
		else return true;
	}
	else return true;
}

bool ULostCauseSaveGame::IsPlayerPawn(UObject * Object)
{
	APawn* PP = Cast<APawn>(Object);
	if (PP)
	{
		APlayerController* PC = Cast<APlayerController>(PP->GetController());
		if (PC) return true;
		else return false;
	}
	else return false;
}

void ULostCauseSaveGame::PopulateReferenceMap(UObject* WorldContextObject)
{
	//To avoid repreating this operation all the time we only do it
	//if we actually saved object references.
	if (bPreventPointerPopulation ||
		ObjectReferences.Num() == 0) return;

	TMap<FName, FString> OwnersMap;

	TArray<FString> OwnerList;
	ObjectReferences.GenerateKeyArray(OwnerList);

	//We populate the OwnersList so we can find the owners of the variables swiftly
	//Instead of reiterating over and over again
	FString CurrOwner;
	for (int32 i = 0; i < OwnerList.Num(); i++)
	{
		CurrOwner = OwnerList[i];
		TMap<FName, FString> OwnSubMap;

		TArray<FName> Names = ObjectReferences.FindRef(CurrOwner).GetNames();

		for (int32 n = 0; n < Names.Num(); n++)
		{
			OwnSubMap.Add(Names[n], CurrOwner);
		}

		if (OwnSubMap.Num() > 0) OwnersMap.Append(OwnSubMap);
	}

	//We can now proceed to gather the actual persistent object pointers.
	TArray<UObject*> PlayerSObjects;
	TArray<UObject*> MapObjects;
	TArray<AActor*> MapActors; //UNUSED
	
	ULostCauseSaveSystemLibrary::GetPersistentObjects(PlayerSObjects, MapObjects, MapActors, WorldContextObject);

	TArray<UObject*> ObjectPointers;
	ObjectPointers.Append(PlayerSObjects);
	ObjectPointers.Append(MapObjects);

	if (ObjectPointers.Num() == 0) return;

	FName CurrName;
	for (int32 i = 0; i < ObjectPointers.Num(); i++)
	{
		CurrName = ObjectPointers[i]->GetFName();

		if (OwnersMap.Contains(CurrName))
		{
			CurrOwner = OwnersMap.FindRef(CurrName);

			FObjectPropertiesContainer PropContainer = ObjectReferences.FindRef(CurrOwner);

			if (PropContainer.ContainsName(CurrName))
			{
				FString CurrentVariable = PropContainer.FindName(CurrName);
				PropContainer.Emplace(CurrentVariable, ObjectPointers[i]);

				//we replace the previous prop container since it is no longer up to date.
				ObjectReferences.Emplace(CurrOwner, PropContainer); 

			} //

		} // if

	}// for
}
