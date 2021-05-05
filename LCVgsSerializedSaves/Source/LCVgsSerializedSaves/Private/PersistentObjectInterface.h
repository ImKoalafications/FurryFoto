// Lost Cause Videogames (c) 2017

#pragma once

#include "LostCauseSaveSystem.h"
#include "PersistentObjectInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPersistentObjectInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 
 */
class LCVGSSERIALIZEDSAVES_API IPersistentObjectInterface
{
	GENERATED_IINTERFACE_BODY()

	// This interface has no usage besides identifying actors to be saved, aditional functionality might be addded if desired.
public:
		
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Persistent Object Interface")
	void ObjectLoadComplete();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Persistent Object Interface")
	void ObjectSaveComplete();

	/** Override this function on any object that will be tied to the player so it will return true
	*	Very useful for any data that needs to be accesible independently of the map the player is in*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Persistent Object Interface")
	bool IsObjectLinkedToPlayer() const;
};
