// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Visual.h"
#include "Engine/UserInterfaceSettings.h"

/////////////////////////////////////////////////////
// UVisual

UVisual::UVisual(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVisual::ReleaseSlateResources(bool bReleaseChildren)
{
}

void UVisual::BeginDestroy()
{
	Super::BeginDestroy();

	const bool bReleaseChildren = false;
	ReleaseSlateResources(bReleaseChildren);
}

bool UVisual::NeedsLoadForServer() const
{
	const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	check(UISettings);
	return UISettings->bLoadWidgetsOnDedicatedServer;
}