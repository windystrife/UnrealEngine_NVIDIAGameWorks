// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * This thumbnail renderer displays the static mesh used by this foliage type
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "FoliageType_ISMThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS(CustomConstructor, Config=Editor)
class UFoliageType_ISMThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()
	
	UFoliageType_ISMThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	:	Super(ObjectInitializer)
	,	ThumbnailScene(nullptr)
	{}

	// UThumbnailRenderer implementation
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	virtual bool CanVisualizeAsset(UObject* Object) override;
	// UObject implementation
	virtual void BeginDestroy() override;

private:
	class FStaticMeshThumbnailScene* ThumbnailScene;
};
