// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Classes/Camera/CameraModifier.h"
#include "ComposurePlayerCompositingCameraModifier.generated.h"


/**
 * Private camera manager for  UComposurePlayerCompositingTarget.
 */
UCLASS(NotBlueprintType)
class UComposurePlayerCompositingCameraModifier
	: public UCameraModifier
	, public IBlendableInterface
{
	GENERATED_UCLASS_BODY()

public:

	// Begins UCameraModifier
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	// Ends UCameraModifier

	// Begins IBlendableInterface
	void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;
	// Ends IBlendableInterface

private:
	// Current player camera manager the target is bind on.
	UPROPERTY(Transient)
	class UComposurePlayerCompositingTarget* Target;
	
	friend class UComposurePlayerCompositingTarget;
};
