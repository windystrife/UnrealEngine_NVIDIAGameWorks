// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"

class AGeometryCacheActor;
class UGeometryCache;

class FGeometryCacheThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FGeometryCacheThumbnailScene();

	/** Sets the static mesh to use in the next GetView() */
	void SetGeometryCache(UGeometryCache* GeometryCache);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The static mesh actor used to display all static mesh thumbnails */
	AGeometryCacheActor* PreviewActor;
};
