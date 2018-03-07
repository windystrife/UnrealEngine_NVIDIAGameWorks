// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/BlendableInterface.h"
#include "LightPropagationVolumeSettings.h"
#include "LightPropagationVolumeBlendable.generated.h"

// BlueprintType to make the object spawnable in blueprint
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class ULightPropagationVolumeBlendable : public UObject, public IBlendableInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(interp, Category=LightPropagationVolumeBlendable, meta=(ShowOnlyInnerProperties))
	struct FLightPropagationVolumeSettings Settings;

	/** 0:no effect, 1:full effect */
	UPROPERTY(interp, Category=LightPropagationVolumeBlendable, BlueprintReadWrite, meta=(UIMin = "0.0", UIMax = "1.0"))
	float BlendWeight;
	
	// Begin interface IBlendableInterface
	virtual void OverrideBlendableSettings(class FSceneView& View, float InWeight) const override;
	// End interface IBlendableInterface
};



