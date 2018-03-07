// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FlexAssetPreviewComponent.generated.h"

/* Component to render flex particles in the static mesh editor. */
UCLASS(MinimalAPI)
class UFlexAssetPreviewComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End UPrimitiveComponent interface.

	class UFlexAsset* FlexAsset;
};
