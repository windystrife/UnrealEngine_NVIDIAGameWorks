// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*
***************************************************************
FDestructibleMeshThumbnailScene
***************************************************************
*/

#pragma once

#include "ThumbnailHelpers.h"

class APEXDESTRUCTIONEDITOR_API FDestructibleMeshThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FDestructibleMeshThumbnailScene();

	/** Sets the skeletal mesh to use in the next GetView() */
	void SetDestructibleMesh(class UDestructibleMesh* InMesh);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The skeletal mesh actor used to display all skeletal mesh thumbnails */
	class ADestructibleActor* PreviewActor;
};