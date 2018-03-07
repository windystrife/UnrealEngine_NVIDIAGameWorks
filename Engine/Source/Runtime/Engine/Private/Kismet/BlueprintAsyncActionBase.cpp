// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintAsyncActionBase.h"

//////////////////////////////////////////////////////////////////////////
// UBlueprintAsyncActionBase

UBlueprintAsyncActionBase::UBlueprintAsyncActionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetFlags(RF_StrongRefOnFrame);
	}
}

void UBlueprintAsyncActionBase::Activate()
{
}

void UBlueprintAsyncActionBase::SetReadyToDestroy()
{
	ClearFlags(RF_StrongRefOnFrame);
}
