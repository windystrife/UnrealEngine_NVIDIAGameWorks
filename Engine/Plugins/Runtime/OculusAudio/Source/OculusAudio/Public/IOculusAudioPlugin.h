// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IAudioExtensionPlugin.h"

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IOculusAudioPlugin : public IAudioPlugin
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOculusAudioPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IOculusAudioPlugin >("OculusAudio");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "OculusAudio" );
	}

	/** We are overriding spatialization. */
	virtual bool ImplementsSpatialization() const override { return true; }
};

