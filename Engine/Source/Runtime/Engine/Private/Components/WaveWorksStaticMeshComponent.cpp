// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WaveWorksStaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "WaveWorksStaticMeshComponent"

UWaveWorksStaticMeshComponent::UWaveWorksStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WaveWorksAsset(nullptr)
{
}

#undef LOCTEXT_NAMESPACE
