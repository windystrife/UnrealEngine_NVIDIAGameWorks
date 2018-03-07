// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"

#define WITH_STEAM_CONTROLLER		((PLATFORM_LINUX || PLATFORM_WINDOWS) && STEAMSDK_FOUND)

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class ISteamControllerPlugin : public IInputDeviceModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ISteamControllerPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< ISteamControllerPlugin >( "SteamController" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SteamController" );
	}
};

