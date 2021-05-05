// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LostCauseSaveSystem.h"
#include "EngineMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LCTimerHelper.generated.h"

UCLASS()
class ULCTimerHelper : public UObject
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ULCTimerHelper();

	//Starts the timer for the second part of the loading process. Do not call this function manually!
	void StartLoadTimer(class ULostCauseSaveGame* NewSaveGame, UObject* WorldContextObject);

	void StartPlayerLoadTimer(ULostCauseSaveGame* NewSaveGame, APlayerController* NewController, UObject* WorldContextObject);

protected:
	
	//Calls the finish load function in the library
	void FinishGameLoad();
	void FinishPlayerLoad();

	void CleanUp();

	FTimerHandle TimerHandle;
	FTimerHandle PlayerTimerHandle;

	UPROPERTY()
	ULostCauseSaveGame* SaveGame;
	APlayerController* Controller;

	UPROPERTY()
	UObject* WorldContext;
};
