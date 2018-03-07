// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "DefaultSpectatorScreenController.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOculusHMD_SpectatorScreenController
//-------------------------------------------------------------------------------------------------

class FOculusHMD_SpectatorScreenController : public FDefaultSpectatorScreenController
{
public:
	FOculusHMD_SpectatorScreenController(class FOculusHMD* InOculusHMD);

	virtual void RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef RenderTarget, FVector2D WindowSize) const override;
	virtual void RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize) override;
	virtual void RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize) override;
	virtual void RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize) override;

private:
	FOculusHMD* OculusHMD;
};


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS