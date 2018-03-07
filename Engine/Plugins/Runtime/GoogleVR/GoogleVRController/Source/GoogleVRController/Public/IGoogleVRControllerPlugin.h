// Copyright 2017 Google Inc.

#pragma once

#include "ModuleManager.h"
#include "IInputDeviceModule.h"

#define GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS (PLATFORM_ANDROID)
#define GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS (WITH_EDITOR)
#define GOOGLEVRCONTROLLER_SUPPORTED_INSTANT_PREVIEW_PLATFORMS (WITH_EDITOR)
#define GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS (GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS || GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS || GOOGLEVRCONTROLLER_SUPPORTED_INSTANT_PREVIEW_PLATFORMS)

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IGoogleVRControllerPlugin : public IInputDeviceModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGoogleVRControllerPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IGoogleVRControllerPlugin >( "GoogleVRController" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GoogleVRController" );
	}
};
