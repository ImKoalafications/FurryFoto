// Fill out your copyright notice in the Description page of Project Settings.

#include "LCTimerHelper.h"
#include "LostCauseSaveSystemLibrary.h"
#include "LostCauseSaveGame.h"




// Sets default values
ULCTimerHelper::ULCTimerHelper()
{
 	
}

void ULCTimerHelper::StartLoadTimer(ULostCauseSaveGame *NewSaveGame, UObject* WorldContextObject)
{
	if (NewSaveGame != NULL)
	{
		SaveGame = NewSaveGame;
		WorldContext = WorldContextObject;
		if (WorldContext->GetWorld() != NULL)
		{
			WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ULCTimerHelper::FinishGameLoad, 0.2f);
		}
		else GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("StartLoadtimer() : World context object is not valid"));
	}
}

void ULCTimerHelper::StartPlayerLoadTimer(ULostCauseSaveGame *NewSaveGame, APlayerController* NewController, UObject* WorldContextObject)
{
	if (NewSaveGame != NULL && NewController != NULL)
	{
		SaveGame = NewSaveGame;
		Controller = NewController;
		WorldContext = WorldContextObject;

		if (WorldContext->GetWorld() != NULL)
		{
			WorldContext->GetWorld()->GetTimerManager().SetTimer(PlayerTimerHandle, this, &ULCTimerHelper::FinishPlayerLoad, 0.2f);
		}
		else GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("StartPlayerLoadTimer() : World context object is not valid"));
	} else GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("StartPlayerLoadTimer() : Either SaveGame or Controller are null"));
}

void ULCTimerHelper::FinishGameLoad()
{
	ULostCauseSaveSystemLibrary::FinishLoad(SaveGame, WorldContext);
	CleanUp();
}

void ULCTimerHelper::FinishPlayerLoad()
{
	ULostCauseSaveSystemLibrary::FinishPlayerObjectsLoad(SaveGame, Controller, WorldContext);
	CleanUp();
}

void ULCTimerHelper::CleanUp()
{
	SaveGame = NULL;
	Controller = NULL;
	WorldContext = NULL;
	RemoveFromRoot();
}
