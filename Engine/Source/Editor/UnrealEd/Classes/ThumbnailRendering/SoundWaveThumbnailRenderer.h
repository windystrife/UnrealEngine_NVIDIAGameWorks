// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer generates a render of a waveform
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "SoundWaveThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS()
class USoundWaveThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	virtual bool AllowsRealtimeThumbnails(UObject* Object) const override;
	// End UThumbnailRenderer Object
};

