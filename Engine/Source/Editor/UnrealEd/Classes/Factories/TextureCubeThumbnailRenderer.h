// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays the first face of the cube map
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "TextureCubeThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS()
class UTextureCubeThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	//~ Begin UThumbnailRenderer Interface
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	//~ End UThumbnailRenderer Interface
};

