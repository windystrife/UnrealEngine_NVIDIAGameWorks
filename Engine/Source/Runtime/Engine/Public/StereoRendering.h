// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StereoRendering.h: Abstract stereoscopic rendering interface
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

class IStereoLayers;
class IStereoRenderTargetManager;

/**
* Stereoscopic rendering passes.  FULL implies stereoscopic rendering isn't enabled for this pass
*/
enum EStereoscopicPass
{
	eSSP_FULL,
	eSSP_LEFT_EYE,
	eSSP_RIGHT_EYE,
	eSSP_MONOSCOPIC_EYE
};

class IStereoRendering
{
public:
	/** 
	 * Whether or not stereo rendering is on this frame.
	 */
	virtual bool IsStereoEnabled() const = 0;

	/** 
	 * Whether or not stereo rendering is on on next frame. Useful to determine if some preparation work
	 * should be done before stereo got enabled in next frame. 
	 */
	virtual bool IsStereoEnabledOnNextFrame() const { return IsStereoEnabled(); }

	/** 
	 * Switches stereo rendering on / off. Returns current state of stereo.
	 * @return Returns current state of stereo (true / false).
	 */
	virtual bool EnableStereo(bool stereo = true) = 0;

	/**
	 * Adjusts the viewport rectangle for stereo, based on which eye pass is being rendered.
	 */
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const = 0;

	/**
	 * Gets the percentage bounds of the safe region to draw in.  This allows things like stat rendering to appear within the readable portion of the stereo view.
	 * @return	The centered percentage of the view that is safe to draw readable text in
	 */
	virtual FVector2D GetTextSafeRegionBounds() const { return FVector2D(0.75f, 0.75f); }

	/**
	 * Calculates the offset for the camera position, given the specified position, rotation, and world scale
	 */
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) = 0;

	/**
	 * Gets a projection matrix for the device, given the specified eye setup
	 */
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const = 0;

	/**
	 * Sets view-specific params (such as view projection matrix) for the canvas.
	 */
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) = 0;

	// Are we outputting so a Spectator Screen now.
	virtual bool IsSpectatorScreenActive() const { return false; }

	// Renders texture into a backbuffer. Could be empty if no rendertarget texture is used, or if direct-rendering 
	// through RHI bridge is implemented. 
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const {}

	/**
	 * Returns orthographic projection , used from Canvas::DrawItem.
	 */
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
	{
		OrthoProjection[0] = OrthoProjection[1] = FMatrix::Identity;
		OrthoProjection[1] = FTranslationMatrix(FVector(OrthoProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5, 0, 0));
	}

	/**
	 * Returns currently active custom present. 
	 */
	virtual FRHICustomPresent* GetCustomPresent() { return nullptr; }

	/**
	 * Returns currently active render target manager.
	 */
	virtual IStereoRenderTargetManager* GetRenderTargetManager() { return nullptr; }

	/**
	* Returns an IStereoLayers implementation, if one is present
	*/
	virtual IStereoLayers* GetStereoLayers () { return nullptr; }

};
