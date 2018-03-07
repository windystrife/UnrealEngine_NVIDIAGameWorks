// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SteamVRSplash.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#if STEAMVR_SUPPORTED_PLATFORMS

FSteamSplashTicker::FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD)
	: FTickableObjectRenderThread(false, true)
	, SteamVRHMD(InSteamVRHMD)
{}

void FSteamSplashTicker::RegisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FSteamSplashTicker::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FSteamSplashTicker::OnPostLoadMap);
}

void FSteamSplashTicker::UnregisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
};

void FSteamSplashTicker::OnPreLoadMap(const FString&)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterAsyncTick,
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Register();
		});
}

void FSteamSplashTicker::OnPostLoadMap(UWorld*)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterAsyncTick, 
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Unregister();
		});
}

void FSteamSplashTicker::Tick(float DeltaTime)
{
	if (SteamVRHMD->pBridge && SteamVRHMD->VRCompositor && SteamVRHMD->bSplashIsShown)
	{
		SteamVRHMD->pBridge->FinishRendering();
		SteamVRHMD->VRCompositor->PostPresentHandoff();
	}
}
TStatId FSteamSplashTicker::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSplashTicker, STATGROUP_Tickables);
}

bool FSteamSplashTicker::IsTickable() const
{
	return true;
}

#endif
