// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IGoogleVRHMDPlugin.h"

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
#include "TickableObjectRenderThread.h"
#include "gvr.h"

class IRendererModule;
class UTexture2D;
class FGoogleVRHMD;
class FGoogleVRHMDCustomPresent;

class FGoogleVRSplash : public TSharedFromThis<FGoogleVRSplash>
{
public:

	FGoogleVRSplash(FGoogleVRHMD* InGVRHMD);
	~FGoogleVRSplash();

	void Init();
	void Show();
	void Hide();
	bool IsShown() { return bIsShown; }

	void Tick(float DeltaTime);
	bool IsTickable() const;
	void RenderStereoSplashScreen(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture);
	void ForceRerenderSplashScreen();

private:

	class FGoogleVRSplashTicker : public FTickableObjectRenderThread, public TSharedFromThis<FGoogleVRSplashTicker>
	{
	public:
		FGoogleVRSplashTicker(FGoogleVRSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FGoogleVRSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FGoogleVRSplash* pSplash;
	};

	void OnPreLoadMap(const FString&);
	void OnPostLoadMap(UWorld*);

	void AllocateSplashScreenRenderTarget();
	void SubmitBlackFrame();

	bool LoadDefaultSplashTexturePath();
	bool LoadTexture();
	void UnloadTexture();
	void UpdateSplashScreenEyeOffset();

public:
	bool bEnableSplashScreen;
	UTexture2D* SplashTexture;

	FString SplashTexturePath;
	FVector2D SplashTextureUVOffset;
	FVector2D SplashTextureUVSize;
	float RenderDistanceInMeter;
	float RenderScale;
	// View angle is used to reduce the async reprojection artifact
	// The splash screen will be hidden when the head rotated beyond half of the view angle
	// from its original orientation.
	float ViewAngleInDegree;

private:

	FGoogleVRHMD* GVRHMD;
	IRendererModule* RendererModule;
	FGoogleVRHMDCustomPresent* GVRCustomPresent;

	bool bInitialized;
	bool bIsShown;
	bool bSplashScreenRendered;
	FVector2D SplashScreenEyeOffset;
	TSharedPtr<FGoogleVRSplashTicker> RenderThreadTicker;

	gvr_mat4f SplashScreenRenderingHeadPose;
	FRotator SplashScreenRenderingOrientation;
};
#endif //GOOGLEVRHMD_SUPPORTED_PLATFORMS