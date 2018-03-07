// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DPICustomScalingRule.generated.h"

/**
 * Custom Scaling Rules for Slate and UMG Widgets can be implemented by sub-classing from this class
 * and setting this rule to be used in your project settings.
 */
UCLASS(Abstract)
class ENGINE_API UDPICustomScalingRule : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Return the scale to use given the size of the viewport.
	 * @param Size The size of the viewport.
	 * @return The Scale to apply to the entire UI.
	 */
	virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const;
};
