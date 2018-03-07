//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Components/SceneComponent.h"
#include "PhononProbeComponent.generated.h"

UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision))
class STEAMAUDIO_API UPhononProbeComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FVector> ProbeLocations;
};
