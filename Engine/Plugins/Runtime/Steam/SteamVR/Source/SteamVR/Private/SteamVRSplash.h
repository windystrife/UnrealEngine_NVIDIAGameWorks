// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ISteamVRPlugin.h"
#include "TickableObjectRenderThread.h"

#if STEAMVR_SUPPORTED_PLATFORMS

class FSteamSplashTicker : public FTickableObjectRenderThread, public TSharedFromThis<FSteamSplashTicker>
{
public:
	FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD);

	// Registration functions for map load calls
	void RegisterForMapLoad();
	void UnregisterForMapLoad();

	// Map load delegates
	void OnPreLoadMap(const FString&);
	void OnPostLoadMap(UWorld*);

	// FTickableObjectRenderThread overrides
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override ;
	virtual bool IsTickable() const override;

private:
	class FSteamVRHMD* SteamVRHMD;
};

#endif
