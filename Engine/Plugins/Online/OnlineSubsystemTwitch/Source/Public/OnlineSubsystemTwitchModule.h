// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"

/**
 * Module used for talking with an Twitch service via Http requests
 */
class FOnlineSubsystemTwitchModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	TUniquePtr<class FOnlineFactoryTwitch> TwitchFactory;

public:

	/**
	 * Constructor
	 */
	FOnlineSubsystemTwitchModule() = default;

	/**
	 * Destructor
	 */
	virtual ~FOnlineSubsystemTwitchModule() = default;

	// IModuleInterface

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	/**
	 * Override this to set whether your module is allowed to be unloaded on the fly
	 *
	 * @return	Whether the module supports shutdown separate from the rest of the engine.
	 */
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	/**
	 * Override this to set whether your module would like cleanup on application shutdown
	 *
	 * @return	Whether the module supports shutdown on application exit
	 */
	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};
