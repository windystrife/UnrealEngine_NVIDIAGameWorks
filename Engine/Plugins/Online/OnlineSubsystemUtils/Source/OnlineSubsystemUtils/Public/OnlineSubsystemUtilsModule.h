// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IOnlineSubsystemUtils;

/**
 * Online subsystem utils module class
 * Misc functionality where dependency on the engine code is allowed (OnlineSubsystem is not allowed to require engine dependencies)
 */
class FOnlineSubsystemUtilsModule : public IModuleInterface
{
public:

	FOnlineSubsystemUtilsModule() {}
	virtual ~FOnlineSubsystemUtilsModule() {}

	/** @return the singleton utility interface */
	IOnlineSubsystemUtils* GetUtils() const { return SubsystemUtils; }

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}


private:

	IOnlineSubsystemUtils* SubsystemUtils;
};


