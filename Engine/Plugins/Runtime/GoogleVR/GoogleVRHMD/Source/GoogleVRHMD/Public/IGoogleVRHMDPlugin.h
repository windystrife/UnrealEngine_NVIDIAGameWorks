// Copyright 2017 Google Inc.

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

#define GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS (PLATFORM_ANDROID)
#define GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS (PLATFORM_IOS)
#define GOOGLEVRHMD_SUPPORTED_PLATFORMS (GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS || GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS)
#define GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS (!GOOGLEVRHMD_SUPPORTED_PLATFORMS && WITH_EDITOR)

/**
 * The public interface to this module.
 */
class IGoogleVRHMDPlugin : public IHeadMountedDisplayModule
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGoogleVRHMDPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IGoogleVRHMDPlugin >( "GoogleVRHMD" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GoogleVRHMD" );
	}
};
