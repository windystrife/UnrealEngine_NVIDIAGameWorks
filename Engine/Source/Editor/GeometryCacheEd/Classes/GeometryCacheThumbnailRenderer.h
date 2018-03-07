// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
*
* This thumbnail renderer displays a given GeometryCache
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "GeometryCacheThumbnailRenderer.generated.h"

class FCanvas;
class FGeometryCacheThumbnailScene;
class FRenderTarget;

UCLASS(config = Editor, MinimalAPI)
class UGeometryCacheThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// Begin UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation
private:
	FGeometryCacheThumbnailScene* ThumbnailScene;
};
