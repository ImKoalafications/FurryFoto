//Copyright 2017 Lost Cause Videogames. Code Written by Leonardo Federico Juane
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/LatentActionManager.h"
#include "LatentActions.h"
#include "LostCauseSaveSystemLibrary.h"
#include "LostCauseSaveGame.h"

UENUM(BlueprintType)
enum class EGameLoadingState : uint8
{
	INITIALIZINGSAVE			UMETA(DisplayName = "Initializing"),
	RETRIEVINGSAVE			UMETA(DisplayName = "Retrieving Save..."),
	CREATINGSAVE			UMETA(DisplayName = "Creating Save..."),
	LOADINGPLAYERDATA		UMETA(DisplayName = "Loading Player Data..."),
	LOADINGACTORDATA		UMETA(DisplayName = "Loading Actors Data..."),
	COMPLETED				UMETA(DisplayName = "Completed")
};

//Load Game Action
//This Action loads the game asyncrhonously to avoid crashes and excesive overhead
class FLoadGameAction : public FPendingLatentAction
{

public:

	float WaitingTime;
	float ElapsedTime;
	bool bStepComplete;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	EGameLoadingState LoadingState;
	FName PlayerName;
	APlayerController* PlayerController;

	UPROPERTY()
	ULostCauseSaveGame* SaveGame;

	UPROPERTY()
	UObject* WorldContext;

	FLoadGameAction(const FLatentActionInfo& LatentInfo, FName NewPlayerName,ULostCauseSaveGame* NewSaveGame, APlayerController* NewPC, UObject* WorldContextObject)
		: ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
		, PlayerName(NewPlayerName)
		, PlayerController(NewPC)
		, SaveGame(NewSaveGame)
		, WorldContext(WorldContextObject)
	{
		LoadingState = EGameLoadingState::INITIALIZINGSAVE;
		WaitingTime = 0.1f;
		ElapsedTime = 0.0f;
		bStepComplete = false;
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		switch (LoadingState)
		{
			case EGameLoadingState::INITIALIZINGSAVE:
			{
				SaveGame = ULostCauseSaveSystemLibrary::GetLCSaveGame(PlayerName, WorldContext);

				if (SaveGame != NULL)
				{
					SaveGame->bPreventPointerPopulation = true;
					LoadingState = EGameLoadingState::RETRIEVINGSAVE;
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("Latent Action Save Game is not valid"));
					LoadingState = EGameLoadingState::COMPLETED;
				}
			}
			break;

			case EGameLoadingState::RETRIEVINGSAVE:
			{
				ElapsedTime += Response.ElapsedTime();
				if (ElapsedTime >= WaitingTime) LoadingState = EGameLoadingState::LOADINGACTORDATA;
			}
			break;

			case EGameLoadingState::LOADINGACTORDATA:
			{
				if (PlayerController != NULL)
				{
					APawn* PlayerPawn = PlayerController->GetPawn();
					if (PlayerPawn != NULL) PlayerPawn->SetActorLocation(FVector(0, 0, 490000));
				}
				ULostCauseSaveSystemLibrary::LoadPersistentActors(SaveGame, WorldContext);
				LoadingState = EGameLoadingState::LOADINGPLAYERDATA;
			}
			break;

			case EGameLoadingState::LOADINGPLAYERDATA:
			{
				ULostCauseSaveSystemLibrary::LoadPlayerData(SaveGame, PlayerController, WorldContext);
				LoadingState = EGameLoadingState::COMPLETED;
			}
			break;
		}

		Response.FinishAndTriggerIf(LoadingState == EGameLoadingState::COMPLETED, ExecutionFunction, OutputLink, CallbackTarget);
	}


#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override
	{
		const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameLoadingState"), true);
		if (!EnumPtr) return FString("Invalid State");

		return EnumPtr->GetNameByValue((int64)LoadingState).ToString();
	}
#endif
};