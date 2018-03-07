// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "StereoLayerManager.h"
#include "SceneViewExtension.h"
class FHeadMountedDisplayBase;

/** 
 *	Default implementation of stereo layers for platforms that require emulating layer support.
 *
 *	FHeadmountedDisplayBase subclasses will use this implementation by default unless overridden.
 */
class HEADMOUNTEDDISPLAY_API FDefaultStereoLayers : public FSimpleLayerManager, public FSceneViewExtensionBase
{
public:
	FDefaultStereoLayers(const FAutoRegister& AutoRegister, FHeadMountedDisplayBase* InHMDDevice);

	virtual void UpdateSplashScreen() override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override;

protected:
	
	/**
	 * Invoked by FHeadMountedDisplayBase to update the HMD position during the late update.
	 */
	void UpdateHmdTransform(const FTransform& InHmdTransform)
	{
		HmdTransform = InHmdTransform;
	}

	struct FLayerRenderParams
	{
		FIntRect Viewport;
		FMatrix RenderMatrices[3];
	};

	void StereoLayerRender(FRHICommandListImmediate& RHICmdList, const TArray<uint32> & LayersToRender, const FLayerRenderParams& RenderParams) const;

	FHeadMountedDisplayBase* HMDDevice;
	FTransform HmdTransform;

	TArray<FLayerDesc> RenderThreadLayers;
	TArray<uint32> SortedSceneLayers;
	TArray<uint32> SortedOverlayLayers;

	friend class FHeadMountedDisplayBase;
};