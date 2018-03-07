// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "MediaPlaneFrustumComponent.generated.h"


/** 
 * A 2d material that will be rendered always facing the camera.
 */
UCLASS(MinimalAPI, ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew)
class UMediaPlaneFrustumComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()

	UMediaPlaneFrustumComponent(const FObjectInitializer& Init);

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }
};
