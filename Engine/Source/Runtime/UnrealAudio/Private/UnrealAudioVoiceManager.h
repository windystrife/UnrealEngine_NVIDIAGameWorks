// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "Containers/Queue.h"
#include "UnrealAudioPitchManager.h"
#include "UnrealAudioVolumeManager.h"
#include "UnrealAudioVoiceMixer.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	* FVoiceSettings
	* Voice settings struct
	*/
	struct FVoiceManagerSettings
	{
		/** The min pitch of a voice. */
		float MinPitch;

		/** The max pitch of a voice. */
		float MaxPitch;

		/** The max voice count of the voice manager. */
		int32 MaxVoiceCount;

		/** The max virtual voice count of the voice manager. */
		int32 MaxVirtualVoiceCount;

		/** The number of sound file decoders to use to decode audio data for voice playback. */
		uint32 NumDecoders;

		/** Settings structure for sound file decoding. */
		FSoundFileDecoderSettings DecoderSettings;

		/** How fast the control data is updating (i.e. update speed of audio system thread). */
		float ControlUpdateRateSeconds;
	};

	/**
	* FVoiceInitializationData
	* Struct used to initialize data for a single voice entry from main thread to the audio thread.
	*/
	struct FVoiceInitializationData
	{
		TSharedPtr<ISoundFile> SoundFile;
		FEmitterHandle EmitterHandle;
		float BaselineVolumeScale;
		float DynamicVolumeScale;
		float DynamicVolumeTime;
		float BaselinePitchScale;
		float DynamicPitchScale;
		float DynamicPitchTime;
		float PriorityWeight;
		float DurationSeconds;
		uint32 VoiceFlags;
	};

	/**
	* FVoiceManager
	* Manages voice handles, messaging, and updating.
	*/
	class FVoiceManager
	{
	public:

		/************************************************************************/
		/* Main Thread Functions												*/
		/************************************************************************/

		FVoiceManager(FUnrealAudioModule* InAudioModule);
		~FVoiceManager();

		void Init(const FVoiceManagerSettings& Settings);

		// Commands to audio thread...
		EVoiceError::Type PlayVoice(FVoice* Voice, const FVoiceInitializationData* InitData, FVoiceHandle& OutVoiceHandle);
		EVoiceError::Type PauseVoice(const FVoiceHandle& VoiceHandle, float InFadeTimeSec);
		EVoiceError::Type StopVoice(const FVoiceHandle& VoiceHandle, float InFadeTimeSec);
		EVoiceError::Type SetVolumeScale(uint32 VoiceDataIndex, float InVolumeScale, float InFadeTimeSec);
		EVoiceError::Type SetPitchScale(uint32 VoiceDataIndex, float InPitchScale, float InFadeTimeSec);

		// Commands from audio thread
		void NotifyVoiceDone(const FCommand& Command);
		void NotifyVoiceReal(const FCommand& Command);
		void NotifyVoiceVirtual(const FCommand& Command);
		void NotifyVoiceSuspend(const FCommand& Command);

		// Getters
		EVoiceError::Type GetNumPlayingVoices(int32& OutNumPlayingVoices) const;
		EVoiceError::Type GetMaxNumberOfPlayingVoices(int32& OutMaxNumPlayingVoices) const;
		EVoiceError::Type GetNumVirtualVoices(int32& OutNumVirtualVoices) const;
		EVoiceError::Type GetMaxNumberOfVirtualVoices(int32& OutMaxNumPlayingVoices) const;
		EVoiceError::Type GetVolumeScale(uint32 VoiceDataIndex, float& OutVolumeScale) const;
		EVoiceError::Type GetVolumeAttenuation(uint32 VoiceDataIndex, float& OutVolumeAttenuation) const;
		EVoiceError::Type GetVolumeFade(uint32 VoiceDataIndex, float& OutVolumeFade) const;
		EVoiceError::Type GetVolumeProduct(uint32 VoiceDataIndex, float& OutVolumeProduct) const;
		EVoiceError::Type GetPitchScale(uint32 VoiceDataIndex, float& OutPitchScale) const;
		EVoiceError::Type GetPitchProduct(uint32 VoiceDataIndex, float& OutPitchProduct) const;

		bool IsValidVoice(const FVoiceHandle& VoiceHandle) const;

		/************************************************************************/
		/* Audio Thread Functions												*/
		/************************************************************************/

		void PlayVoice(const FCommand& Command);
		void PauseVoice(const FCommand& Command);
		void StopVoice(const FCommand& Command);
		void SetVolumeScale(const FCommand& Command);
		void SetPitchScale(const FCommand& Command);
		void Update();

		/**
		 * Mixes the active audio-generating voices together, called from audio device thread.
		 *
		 * @param [in,out]	CallbackInfo	Information describing the audio device callback.
		 */

		void Mix(struct FCallbackInfo& CallbackInfo);

	private:

		void UpdateStates();
		uint32 GetVoiceDataIndex(const FVoiceHandle& VoiceHandle) const;
		void ComputeInitialVolumeValues(const FVoiceInitializationData* VoiceInitData, float& OutVolumeProduct, float& OutVolumeAttenuation, float& OutWeightedPriority) const;

		// Main thread data
		TArray<FVoice*> VoiceObjects;
		FEntityManager EntityManager;

		FUnrealAudioModule* AudioModule;
		FVoiceManagerSettings Settings;

		// Audio thread data

		// Handle index to voice pointer
		TMap<uint32, uint32> HandleToIndex;

		// VoiceDataIndex
		TArray<FVoiceHandle> VoiceHandles;
		TArray<EVoicePlayingState::Type> PlayingStates;
		TArray<EVoiceState::Type> States;
		TArray<TSharedPtr<ISoundFile>> SoundFiles;
		FPitchManager PitchManager;
		FVolumeManager VolumeManager;
		TArray<uint32> Flags;
		TQueue<uint32> FreeDataIndicies;
		
		FVoiceMixer VoiceMixer;
		int32 NumVirtualVoices;
		int32 NumRealVoices;
	};

}

#endif // #if ENABLE_UNREAL_AUDIO
