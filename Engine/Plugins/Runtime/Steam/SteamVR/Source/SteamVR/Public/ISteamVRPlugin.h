// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#define STEAMVR_SUPPORTED_PLATFORMS (PLATFORM_MAC || (PLATFORM_LINUX && PLATFORM_CPU_X86_FAMILY && PLATFORM_64BITS) || (PLATFORM_WINDOWS && WINVER > 0x0502))

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"
#include "IHeadMountedDisplay.h"
#if STEAMVR_SUPPORTED_PLATFORMS
#include "openvr.h"
#endif // STEAMVR_SUPPORTED_PLATFORMS

/** Up to 8 motion controller devices supported (two VR motion controllers per Unreal controller, one for either the left or right hand.) */
#define MAX_STEAMVR_CONTROLLER_PAIRS 4

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class ISteamVRPlugin : public IHeadMountedDisplayModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ISteamVRPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< ISteamVRPlugin >( "SteamVR" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SteamVR" );
	}

#if STEAMVR_SUPPORTED_PLATFORMS
	/**
	 * Get the IVRSystem* that was previously set by the HMD implemenentation.
	 *
	 * @return The pointer if the HMD has been initialized, otherwise nullptr 
	 */
	virtual vr::IVRSystem* GetVRSystem() const=0;

	/**
	 * Resets the plugin, in case of early VR system shutdown
	 */
	virtual void Reset() = 0;

#endif // STEAMVR_SUPPORTED_PLATFORMS

private:
};

