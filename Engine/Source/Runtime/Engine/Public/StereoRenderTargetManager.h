// Copyright 1998 - 2017 Epic Games, Inc.All Rights Reserved.

/*=============================================================================
StereoRenderTargetManager.h: Abstract interface returned from IStereoRendering to support rendering into a texture
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

/** 
 * The IStereoRenderTargetManager can be returned from IStereoRendering::GetRenderTargetManager() implementations.
 * Implement this interface if a stereo rendering device requires all output to be rendered into separate render targets and/or to customize how
 * separate render targets are allocated.
 */
class IStereoRenderTargetManager
{
public:
	/** 
	 * Whether a separate render target should be used or not.
	 * In case the stereo rendering implementation does not require special handling of separate render targets 
	 * at all, it can leave out implementing this interface completely and simply let the default implementation 
	 * of IStereoRendering::GetRenderTargetManager() return nullptr.
	 */
	virtual bool ShouldUseSeparateRenderTarget() const = 0;

	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 *
	 * @param bUseSeparateRenderTarget	Set to true if a separate render target will be used. Can potentiallt be true even if ShouldUseSeparateRenderTarget() returned false earlier.
	 * @param Viewport					The Viewport instance calling this method.
	 * @param ViewportWidget			(optional) The Viewport widget containing the view. Can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) = 0;

	/**
	 * Returns true, if render target texture must be re-calculated.
	 */
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) = 0;

	/**
	* Returns true, if render target texture must be re-calculated.
	*/
	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<struct IPooledRenderTarget>& DepthTarget) { return false; }

	/**
	 * Returns number of required buffered frames.
	 */
	virtual uint32 GetNumberOfBufferedFrames() const { return 1; }

	/**
	 * Allocates a render target texture.
	 * The default implementation always return false to indicate that the default texture allocation should be used instead.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) { return false; }

	/**
	 * Allocates a depth texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) { return false; }
};