// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultXRCamera.h"

class FGoogleARCoreHMD;

class FGoogleARCoreXRCamera : public FDefaultXRCamera
{

public:
	FGoogleARCoreXRCamera(const FAutoRegister&, FGoogleARCoreHMD& InTangoSystem, int32 InDeviceID);

	//~ FDefaultXRCamera
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderMobileBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override;
	//~ FDefaultXRCamera

private:
	FGoogleARCoreHMD& TangoSystem;
};