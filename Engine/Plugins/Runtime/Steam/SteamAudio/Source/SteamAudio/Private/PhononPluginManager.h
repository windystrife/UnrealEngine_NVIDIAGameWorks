//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SteamAudioModule.h"
#include "PhononSpatialization.h"
#include "PhononOcclusion.h"
#include "PhononReverb.h"
#include "SteamAudioEnvironment.h"
#include "AudioPluginUtilities.h"

namespace SteamAudio
{
	/************************************************************************/
	/* FPhononPluginManager                                                 */
	/* This ListenerObserver owns the Steam Audio environment, and          */
	/* dispatches information to the Steam Audio reverb and occlusion       */
	/* plugins.                                                             */
	/************************************************************************/
	class FPhononPluginManager : public IAudioPluginListener
	{
	public:
		FPhononPluginManager();
		~FPhononPluginManager();

		//~ Begin IAudioPluginListener
		virtual void OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld) override;
		virtual void OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds) override;
		virtual void OnListenerShutdown(FAudioDevice* AudioDevice) override;
		//~ End IAudioPluginListener

	private:
		bool bEnvironmentCreated;
		FEnvironment Environment;
		FPhononReverb* ReverbPtr;
		FPhononOcclusion* OcclusionPtr;

	private:
		/* Helper function for checking whether the user is using Steam Audio for spatialization, reverb, and/or occlusion: */
		static bool IsUsingSteamAudioPlugin(EAudioPlugin PluginType);
	};
}
