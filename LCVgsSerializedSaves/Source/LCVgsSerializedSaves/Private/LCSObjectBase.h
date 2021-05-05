// (c) 2017 Lost Cause Videogames - Written by Leonardo F. Juane

#pragma once

#include "LostCauseSaveSystem.h"
#include "EngineMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LCSObjectBase.generated.h"

/**
 * A Base class fro serializable UObjects
 */
UCLASS(BlueprintType, Blueprintable)
class ULCSObjectBase : public UObject
{
	GENERATED_BODY()
	
public:

	/** Get the world this Object resides in */
	virtual class UWorld* GetWorld() const override;

	/** Sets a new UWorld for this object. Only used if the Outer for this object does not have access to it */
	virtual void SetWorld(class UWorld* NewWorld);

protected:

	/** The world this object resides in */
	UPROPERTY(Transient)
	class UWorld* World;

	/** Gets the current world this Object resides in. Blueprint Callable */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LCS Object", meta = (DisplayName = "Get World"))
	UWorld* GetWorld_BP() const;
};
