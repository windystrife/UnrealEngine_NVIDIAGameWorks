//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "PhononCommon.h"
#include "phonon.h"

class UOcclusionPluginSourceSettingsBase;

namespace SteamAudio
{
	struct FDirectSoundSource
	{
		FDirectSoundSource();

		FCriticalSection CriticalSection;
		IPLDirectSoundPath DirectSoundPath;
		IPLhandle DirectSoundEffect;
		EIplDirectOcclusionMethod DirectOcclusionMethod;
		EIplDirectOcclusionMode DirectOcclusionMode;
		IPLAudioBuffer InBuffer;
		IPLAudioBuffer OutBuffer;
		IPLVector3 Position;
		float Radius;
		bool bDirectAttenuation;
		bool bAirAbsorption;
		bool bNeedsUpdate;
	};

	/************************************************************************/
	/* FPhononOcclusion                                                     */
	/* Scene-dependent audio occlusion plugin. Receives updates from        */
	/* an FPhononPluginManager on the game thread on player position and    */
	/* geometry, and performs geometry-aware filtering of the direct path   */
	/* of an audio source.                                                  */
	/************************************************************************/
	class FPhononOcclusion : public IAudioOcclusion
	{
	public:
		FPhononOcclusion();
		~FPhononOcclusion();

		//~ Begin IAudioOcclusion
		virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
		virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings) override;
		virtual void OnReleaseSource(const uint32 SourceId) override;
		virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
		//~ End IAudioOcclusion

		// Receives updates on listener positions from the game thread using this function call.
		void UpdateDirectSoundSources(const FVector& ListenerPosition, const FVector& ListenerForward, const FVector& ListenerUp);

		// Sets up handle to the environmental handle owned by the FPhononPluginManager.
		void SetEnvironmentalRenderer(IPLhandle EnvironmentalRenderer);

		// Sets up the handle to the Critical Section owned by the FPhononPluginManager.
		void SetCriticalSectionHandle(FCriticalSection* CriticalSectionHandle);

	private:
		//Critical section. Scope locked so that environment is not modified by the Plugin Manager during audio processing.
		FCriticalSection* EnvironmentCriticalSectionHandle;
		
		//Handle to FPhononPluginManager's Environmental Renderer.
		IPLhandle EnvironmentalRenderer;

		//Cached Audio Formats:
		IPLAudioFormat InputAudioFormat;
		IPLAudioFormat OutputAudioFormat;

		//Cached array of direct sound sources to be occluded.
		TArray<FDirectSoundSource> DirectSoundSources;
	};
}
