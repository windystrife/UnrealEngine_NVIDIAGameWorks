// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHeadMountedDisplay.h"
#include "XRTrackingSystemBase.h"
#include "DefaultSpectatorScreenController.h"

/**
 * Default implementation for various IHeadMountedDisplay methods.
 * You can extend this class instead of IHeadMountedDisplay directly when implementing support for new HMD devices.
 */

class HEADMOUNTEDDISPLAY_API FHeadMountedDisplayBase : public FXRTrackingSystemBase, public IHeadMountedDisplay, public IStereoRendering
{

public:
	virtual ~FHeadMountedDisplayBase() {}

	/**
	 * Record analytics - To add custom information logged with the analytics, override PopulateAnalyticsAttributes
	 */
	virtual void RecordAnalytics() override;

	/** 
	 * Default IXRTrackingSystem implementation
	 */
	virtual bool IsHeadTrackingAllowed() const override;

	/** 
	 * Default stereo layer implementation
	 */
	virtual IStereoLayers* GetStereoLayers() override;

	virtual bool GetHMDDistortionEnabled() const override;
	virtual void BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual void BeginRendering_GameThread() override;

	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual bool IsSpectatorScreenActive() const override;

	virtual class ISpectatorScreenController* GetSpectatorScreenController() override;
	virtual class ISpectatorScreenController const* GetSpectatorScreenController() const override;

	// Spectator Screen Hooks into specific implementations
	// Get the point on the left eye render target which the viewers eye is aimed directly at when looking straight forward. 0,0 is top left.
	virtual FVector2D GetEyeCenterPoint_RenderThread(EStereoscopicPass Eye) const;
	// Get the rectangle of the HMD rendertarget for the left eye which seems undistorted enough to be cropped and displayed on the spectator screen.
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const { return FIntRect(0, 0, 1, 1); }
	// Helper to copy one render target into another for spectator screen display
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack) const {}

protected:
	/**
	 * Called by RecordAnalytics when creating the analytics event sent during HMD initialization.
	 *
	 * Return false to disable recording the analytics event	
	 */
	virtual bool PopulateAnalyticsAttributes(TArray<struct FAnalyticsEventAttribute>& EventAttributes);

	/** 
	 * Implement this method to provide an alternate render target for head locked stereo layer rendering, when using the default Stereo Layers implementation.
	 * 
	 * Return a FTexture2DRHIRef pointing to a texture that can be composed on top of each eye without applying reprojection to it.
	 * Return nullptr to render head locked stereo layers into the same render target as other layer types, in which case InOutViewport must not be modified.
	 */
	virtual FTexture2DRHIRef GetOverlayLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& InOutViewport) { return nullptr; }

	/**
	 * Implement this method to override the render target for scene based stereo layers.
	 * Return nullptr to render stereo layers into the normal render target passed to the stereo layers scene view extension, in which case OutViewport must not be modified.
	 */
	virtual FTexture2DRHIRef GetSceneLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& InOutViewport) { return nullptr; }

	mutable TSharedPtr<class FDefaultStereoLayers, ESPMode::ThreadSafe> DefaultStereoLayers;
	
	friend class FDefaultStereoLayers;

	TUniquePtr<FDefaultSpectatorScreenController> SpectatorScreenController;
};
