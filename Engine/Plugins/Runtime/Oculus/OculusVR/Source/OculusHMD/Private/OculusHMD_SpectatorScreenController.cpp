// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_SpectatorScreenController.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOculusRiftSpectatorScreenController
//-------------------------------------------------------------------------------------------------

FOculusHMD_SpectatorScreenController::FOculusHMD_SpectatorScreenController(FOculusHMD* InOculusHMD)
	: FDefaultSpectatorScreenController(InOculusHMD)
	, OculusHMD(InOculusHMD)
{
}

void FOculusHMD_SpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef RenderTexture, FVector2D WindowSize) const
{
	if (OculusHMD->GetCustomPresent_Internal())
	{
		FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, RenderTexture, WindowSize);
	}
}

void FOculusHMD_SpectatorScreenController::RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();
	FSettings* Settings = OculusHMD->GetSettings_RenderThread();
	FIntRect DestRect(0, 0, TargetTexture->GetSizeX() / 2, TargetTexture->GetSizeY());
	for (int i = 0; i < 2; ++i)
	{
		OculusHMD->CopyTexture_RenderThread(RHICmdList, EyeTexture, Settings->EyeRenderViewport[i], TargetTexture, DestRect, false);
		DestRect.Min.X += TargetTexture->GetSizeX() / 2;
		DestRect.Max.X += TargetTexture->GetSizeX() / 2;
	}
}

void FOculusHMD_SpectatorScreenController::RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();
	FCustomPresent* CustomPresent = OculusHMD->GetCustomPresent_Internal();
	FTexture2DRHIRef MirrorTexture = CustomPresent->GetMirrorTexture();
	if (MirrorTexture)
	{
		FIntRect SrcRect(0, 0, MirrorTexture->GetSizeX(), MirrorTexture->GetSizeY());
		FIntRect DestRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
		OculusHMD->CopyTexture_RenderThread(RHICmdList, MirrorTexture, SrcRect, TargetTexture, DestRect, false);
	}
}

void FOculusHMD_SpectatorScreenController::RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();
	FSettings* Settings = OculusHMD->GetSettings_RenderThread();
	const FIntRect SrcRect= Settings->EyeRenderViewport[0];
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	OculusHMD->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false);
}

} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS