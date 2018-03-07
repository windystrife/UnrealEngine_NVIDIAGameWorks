// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This is an abstract base class that is used to define the interface that
 * UnrealEd will use when rendering a given object's thumbnail. The editor
 * only calls the virtual rendering function.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS(abstract, MinimalAPI)
class UThumbnailRenderer : public UObject
{
	GENERATED_UCLASS_BODY()


public:
	/**
	 * Returns true if the renderer is capable of producing a thumbnail for the specified asset.
	 *
	 * @param Object the asset to attempt to render
	 */
	virtual bool CanVisualizeAsset(UObject* Object) { return true; }

	/**
	 * Calculates the size the thumbnail would be at the specified zoom level
	 *
	 * @param Object the object the thumbnail is of
	 * @param Zoom the current multiplier of size
	 * @param OutWidth the var that gets the width of the thumbnail
	 * @param OutHeight the var that gets the height
	 */
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const PURE_VIRTUAL(UThumbnailRenderer::GetThumbnailSize,);

	/**
	 * Draws a thumbnail for the object that was specified.
	 *
	 * @param Object the object to draw the thumbnail for
	 * @param X the X coordinate to start drawing at
	 * @param Y the Y coordinate to start drawing at
	 * @param Width the width of the thumbnail to draw
	 * @param Height the height of the thumbnail to draw
	 * @param Viewport the viewport being drawn in
	 * @param Canvas the render interface to draw with
	 */
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas) PURE_VIRTUAL(UThumbnailRenderer::Draw,);

	/**
	 * Checks to see if the specified asset supports realtime thumbnails, which will cause them to always be rerendered to reflect any changes
	 * made to the asset. If this is false, thumbnails should render once and then not update again.
	 * For most renderers, this should remain as true.
	 *
	 * @param	Object	The asset to draw the thumbnail for
	 *
	 * @return True if the thumbnail needs to always be redrawn, false if it can be just drawn once and then reused.
	 */
	virtual bool AllowsRealtimeThumbnails(UObject* Object) const { return true; }
};

