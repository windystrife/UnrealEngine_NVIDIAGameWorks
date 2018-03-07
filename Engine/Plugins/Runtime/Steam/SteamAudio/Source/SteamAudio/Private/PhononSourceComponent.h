//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Components/SceneComponent.h"
#include "PhononSourceComponent.generated.h"

/**
 * Phonon Source Components can be placed alongside statically positioned Audio Components in order to bake impulse response data
 * to be applied at runtime.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent), HideCategories = (Activation, Collision, Tags, Rendering, Physics, LOD))
class STEAMAUDIO_API UPhononSourceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Any Phonon probes that lie within the baking radius will be used to produce baked impulse response data for this source location.
	UPROPERTY(EditAnywhere, Category = Baking)
	float BakingRadius = 1600.0f;

	// Users must specify a unique identifier for baked data lookup at runtime.
	UPROPERTY(EditAnywhere, Category = Baking)
	FName UniqueIdentifier;
};