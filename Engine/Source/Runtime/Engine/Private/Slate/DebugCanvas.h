// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "CanvasTypes.h"

class FRHICommandListImmediate;

typedef TSharedPtr<FCanvas, ESPMode::ThreadSafe> FCanvasPtr;

/**
 * Custom Slate drawer to render a debug canvas on top of a Slate window
 */
class FDebugCanvasDrawer : public ICustomSlateElement
{
public:
	FDebugCanvasDrawer();
	~FDebugCanvasDrawer();

	/** @return The debug canvas that the game thread can use */
	FCanvas* GetGameThreadDebugCanvas();

	/**
	 * Sets up the canvas for rendering
	 */
	void BeginRenderingCanvas( const FIntRect& InCanvasRect );

	/**
	 * Creates a new debug canvas and enqueues the previous one for deletion
	 */
	void InitDebugCanvas(UWorld* InWorld);

	/** 
	* Releases rendering resources
	*/
	void ReleaseResources();

private:
	/**
	 * ICustomSlateElement interface 
	 */
	virtual void DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer) override;

	/**
	 * Deletes the rendering thread canvas 
	 */
	void DeleteRenderThreadCanvas();

	/**
	 * Gets the render thread canvas 
	 */
	FCanvasPtr GetRenderThreadCanvas();

	/**
	 * Set the canvas that can be used by the render thread
	 */
	void SetRenderThreadCanvas(const FIntRect& InCanvasRect, FCanvasPtr& Canvas);

	/**
	* Release the internal layer texture
	*/
	void ReleaseTexture();

private:
	/** The canvas that can be used by the game thread */
	FCanvasPtr GameThreadCanvas;
	/** The canvas that can be used by the render thread */
	FCanvasPtr RenderThreadCanvas;
	/** Render target that the canvas renders to */
	class FSlateCanvasRenderTarget* RenderTarget;
	/** Rendertarget used in case of self textured canvas */
	TRefCountPtr<IPooledRenderTarget> LayerTexture;
	/** HMD layer ID */
	uint32 LayerID;
	/** true if the RenderThreadCanvas rendered elements last frame */
	bool bCanvasRenderedLastFrame;
};
