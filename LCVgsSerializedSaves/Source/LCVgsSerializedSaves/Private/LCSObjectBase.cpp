// Fill out your copyright notice in the Description page of Project Settings.

#include "LCSObjectBase.h"

UWorld* ULCSObjectBase::GetWorld() const
{
	UWorld* Result = nullptr;
	bool OuterWorldNotValid = false;

	if (GetOuter() != nullptr)
	{
		if (GetOuter()->GetWorld() != nullptr) Result = GetOuter()->GetWorld();
		else OuterWorldNotValid = true;
	}
	else OuterWorldNotValid = true;

	if (OuterWorldNotValid)
	{
		Result = World;
	}

	return Result;
}

void ULCSObjectBase::SetWorld(UWorld* NewWorld)
{
	if (NewWorld) World = NewWorld;
}

UWorld * ULCSObjectBase::GetWorld_BP() const
{
	return GetWorld();
}
