// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "ISpectatorScreenController.h"
#include "HeadMountedDisplayTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FHeadMountedDisplayBase;

/** 
 *	Default implementation of spectator screen controller.
 *
 */
class HEADMOUNTEDDISPLAY_API FDefaultSpectatorScreenController : public ISpectatorScreenController, public TSharedFromThis<FDefaultSpectatorScreenController, ESPMode::ThreadSafe>
{
public:
	FDefaultSpectatorScreenController(FHeadMountedDisplayBase* InHMDDevice);
	virtual ~FDefaultSpectatorScreenController() {}

	// ISpectatorScreenController
	virtual ESpectatorScreenMode GetSpectatorScreenMode() const override;
	virtual void SetSpectatorScreenMode(ESpectatorScreenMode Mode) override;
	virtual void SetSpectatorScreenTexture(UTexture* InTexture) override;
	virtual UTexture* GetSpectatorScreenTexture() const override;
	virtual void SetSpectatorScreenModeTexturePlusEyeLayout(const FSpectatorScreenModeTexturePlusEyeLayout& Layout) override;

	FSpectatorScreenRenderDelegate* GetSpectatorScreenRenderDelegate_RenderThread();


	// Implementation methods called by HMD
	virtual void BeginRenderViewFamily();
	virtual void UpdateSpectatorScreenMode_RenderThread();
	virtual void RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef RenderTarget, FVector2D WindowSize) const;

protected:
	friend struct FRHISetSpectatorScreenTexture;
	virtual void SetSpectatorScreenTextureRenderCommand(UTexture* SrcTexture);
	virtual void SetSpectatorScreenTexture_RenderThread(FTexture2DRHIRef& InTexture);

	friend struct FRHISetSpectatorScreenModeTexturePlusEyeLayout;
	virtual void SetSpectatorScreenModeTexturePlusEyeLayoutRenderCommand(const FSpectatorScreenModeTexturePlusEyeLayout& Layout);
	virtual void SetSpectatorScreenModeTexturePlusEyeLayout_RenderThread(const FSpectatorScreenModeTexturePlusEyeLayout& Layout);

	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture);

	virtual void RenderSpectatorModeSingleEyeLetterboxed(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeMirrorAndTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);
	virtual void RenderSpectatorModeSingleEyeCroppedToFill(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize);

	virtual FRHITexture2D* GetFallbackRHITexture() const;

	mutable FCriticalSection NewSpectatorScreenModeLock;
	ESpectatorScreenMode NewSpectatorScreenMode = ESpectatorScreenMode::SingleEyeCroppedToFill;
	TWeakObjectPtr<UTexture> SpectatorScreenTexture;

	ESpectatorScreenMode SpectatorScreenMode_RenderThread = ESpectatorScreenMode::Disabled;
	FTexture2DRHIRef SpectatorScreenTexture_RenderThread;
	FSpectatorScreenModeTexturePlusEyeLayout SpectatorScreenModeTexturePlusEyeLayout_RenderThread;
	FSpectatorScreenRenderDelegate SpectatorScreenDelegate_RenderThread;

	class HEADMOUNTEDDISPLAY_API Helpers
	{
	public:
		static FIntRect GetEyeCroppedToFitRect(FVector2D EyeCenterPoint, const FIntRect& EyeRect, const FIntRect& TargetRect);
		static FIntRect GetLetterboxedDestRect(const FIntRect& SrcRect, const FIntRect& TargetRect);
	};

private:
	FHeadMountedDisplayBase* HMDDevice;
};