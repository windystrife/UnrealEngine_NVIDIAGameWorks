// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/NetworkSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"

UNetworkSettings::UNetworkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Network");
}

static FName NetworkConsoleVariableFName(TEXT("ConsoleVariable"));

void UNetworkSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void UNetworkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif // #if WITH_EDITOR

