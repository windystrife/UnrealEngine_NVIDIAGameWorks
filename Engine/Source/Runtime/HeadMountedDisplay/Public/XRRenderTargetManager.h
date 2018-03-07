// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "StereoRenderTargetManager.h"

/** 
 * Common IStereoRenderTargetManager implementation that can be used by HMD implementations in order to get default implementations for most methods.
 */
class HEADMOUNTEDDISPLAY_API FXRRenderTargetManager : public IStereoRenderTargetManager
{
public:
	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 *
	 * @param bUseSeparateRenderTarget	Set to true if a separate render target will be used. Can potentiallt be true even if ShouldUseSeparateRenderTarget() returned false earlier.
	 * @param Viewport					The Viewport instance calling this method.
	 * @param ViewportWidget			(optional) The Viewport widget containing the view. Can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override;

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 * This implementation calculates the size based on the current value of r.ScreenPercentage.
	 */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;

	/**
	* Returns true, if render target texture must be re-calculated.
	*/
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;

protected:
	/** 
	 * Optional method called when the ViewportWidget is not null
	 */
	virtual void UpdateViewportWidget(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget) {}

	/** 
	 * Optional method for custom present specific actions called at the end of UpdateViewport
	 */
	virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) {}

};

