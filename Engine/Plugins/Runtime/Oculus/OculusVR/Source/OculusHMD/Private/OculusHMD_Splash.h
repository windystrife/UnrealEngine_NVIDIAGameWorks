// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_GameFrame.h"
#include "OculusHMD_Layer.h"
#include "TickableObjectRenderThread.h"
#include "OculusHMDTypes.h"

namespace OculusHMD
{

class FOculusHMD;

//-------------------------------------------------------------------------------------------------
// FSplashLayer
//-------------------------------------------------------------------------------------------------

struct FSplashLayer
{
	FOculusSplashDesc Desc;
	FLayerPtr Layer;

public:
	FSplashLayer(const FOculusSplashDesc& InDesc) : Desc(InDesc) {}
	FSplashLayer(const FSplashLayer& InSplashLayer) : Desc(InSplashLayer.Desc), Layer(InSplashLayer.Layer) {}
};


//-------------------------------------------------------------------------------------------------
// FSplash
//-------------------------------------------------------------------------------------------------

class FSplash : public TSharedFromThis<FSplash>
{
protected:
	class FTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTicker>
	{
	public:
		FTicker(FSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick_RenderThread(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FSplash* pSplash;
	};

public:
	FSplash(FOculusHMD* InPlugin);
	virtual ~FSplash();

	void Tick_RenderThread(float DeltaTime);
	bool IsTickable() const { return bTickable; }
	bool IsShown() const;

	void Startup();
	void PreShutdown();
	void Shutdown();

	bool IsLoadingStarted() const { return bLoadingStarted; }
	bool IsLoadingCompleted() const { return bLoadingCompleted; }

	void OnLoadingBegins();
	void OnLoadingEnds();

	bool AddSplash(const FOculusSplashDesc&);
	void ClearSplashes();
	bool GetSplash(unsigned index, FOculusSplashDesc& OutDesc);

	void SetAutoShow(bool bInAuto) { bAutoShow = bInAuto; }
	bool IsAutoShow() const { return bAutoShow; }

	void SetLoadingIconMode(bool bInLoadingIconMode) { bLoadingIconMode = bInLoadingIconMode; }
	bool IsLoadingIconMode() const { return bLoadingIconMode; }

	enum EShowFlags
	{
		ShowManually = (1 << 0),
		ShowAutomatically = (1 << 1),
	};

	void Show(uint32 InShowFlags = ShowManually);
	void Hide(uint32 InShowFlags = ShowManually);

	// delegate method, called when loading begins
	void OnPreLoadMap(const FString&) { OnLoadingBegins(); }

	// delegate method, called when loading ends
	void OnPostLoadMap(UWorld*) { OnLoadingEnds(); }


protected:
	void OnShow();
	void OnHide();
	bool PushFrame();
	void UnloadTextures();
	void LoadTexture(FSplashLayer& InSplashLayer);
	void UnloadTexture(FSplashLayer& InSplashLayer);

	void RenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList, double TimeInSeconds);

protected:
	FOculusHMD* OculusHMD;
	FCustomPresent* CustomPresent;
	TSharedPtr<FTicker> Ticker;
	FCriticalSection RenderThreadLock;
	FSettingsPtr Settings;
	FGameFramePtr Frame;
	TArray<FSplashLayer> SplashLayers;
	uint32 NextLayerId;
	FLayerPtr BlackLayer;
	TArray<FLayerPtr> Layers_RenderThread;
	TArray<FLayerPtr> Layers_RHIThread;

	// All these flags are only modified from the Game thread
	bool bInitialized;
	bool bTickable;
	bool bLoadingStarted;
	bool bLoadingCompleted;
	bool bLoadingIconMode; // this splash screen is a simple loading icon (if supported)
	bool bAutoShow; // whether or not show splash screen automatically (when LoadMap is called)
	bool bIsBlack;

	float SystemDisplayInterval;
	uint32 ShowFlags;
	double LastTimeInSeconds;
};

typedef TSharedPtr<FSplash> FSplashPtr;


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS