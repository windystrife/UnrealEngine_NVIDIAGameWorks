// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Interface_PostProcessVolume.generated.h"

struct FPostProcessSettings;

struct FPostProcessVolumeProperties
{
	const FPostProcessSettings* Settings;
	float Priority;
	float BlendRadius;
	float BlendWeight;
	bool bIsEnabled;
	bool bIsUnbound;
};

/** Interface for general PostProcessVolume access **/
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInterface_PostProcessVolume : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IInterface_PostProcessVolume
{
	GENERATED_IINTERFACE_BODY()

	ENGINE_API virtual bool EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint) = 0;
	ENGINE_API virtual FPostProcessVolumeProperties GetProperties() const = 0;
};
