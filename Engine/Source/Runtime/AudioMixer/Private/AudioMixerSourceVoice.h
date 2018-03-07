// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixerBuffer.h"
#include "AudioMixerSourceManager.h"

namespace Audio
{
	struct FMixerSourceVoiceBuffer;
	struct FMixerSourceVoiceFilterParams;
	struct FMixerSourceVoiceInitParams;
	class FMixerDevice;
	class FMixerSubmix;
	class FMixerSource;
	class FMixerSourceManager;
	class ISourceBufferQueueListener;


	class FMixerSourceVoice
	{
	public:
		FMixerSourceVoice();
		~FMixerSourceVoice();

		// Resets the source voice state
		void Reset(FMixerDevice* InMixerDevice);

		// Initializes the mixer source voice
		bool Init(const FMixerSourceVoiceInitParams& InFormat);

		// Releases the source voice back to the source buffer pool
		void Release();

		// Queues the given buffer data to the internal queue of audio buffers.
		void SubmitBuffer(FMixerSourceBufferPtr InSourceVoiceBuffer, const bool bOnRenderThread);

		// Returns the number of buffers that are currently queued to be played.
		int32 GetNumBuffersQueued() const;

		// Sets the source voice pitch value.
		void SetPitch(const float InPitch);

		// Sets the source voice volume value.
		void SetVolume(const float InVolume);

		// Sets the source voice distance attenuation.
		void SetDistanceAttenuation(const float InDistanceAttenuation);
		
		// Sets the source voice's LPF filter frequency.
		void SetLPFFrequency(const float InFrequency);

		// Sets the source voice's HPF filter frequency.
		void SetHPFFrequency(const float InFrequency);

		// Sets the source voice's channel map (2d or 3d).
		void SetChannelMap(TArray<float>& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly);

		// Sets params used by HRTF spatializer
		void SetSpatializationParams(const FSpatializationParams& InParams);

		// Starts the source voice generating audio output into it's submix.
		void Play();

		// Pauses the source voice (i.e. stops generating output but keeps its state as "active and playing". Can be restarted.)
		void Pause();

		// Stops the source voice (no longer playing or active, can't be restarted.)
		void Stop();

		// Queries if the voice is playing
		bool IsPlaying() const;

		// Queries if the voice is paused
		bool IsPaused() const;

		// Queries if the source voice is active.
		bool IsActive() const;

		// Queries if the source voice has finished playing all its audio.
		bool IsDone() const;

		// Queries if the source ffect tails have finished
		bool IsSourceEffectTailsDone() const;

		// Whether or not the device changed and needs another speaker map sent
		bool NeedsSpeakerMap() const;

		// Retrieves the total number of samples played.
		int64 GetNumFramesPlayed() const;

		// Mixes the dry and wet buffer audio into the given buffers.
		void MixOutputBuffers(AlignedFloatBuffer& OutWetBuffer, const float SendLevel) const;

		// Sets the submix send levels
		void SetSubmixSendInfo(FMixerSubmixPtr Submix, const float SendLevel);

		// Called when the source is a bus and needs to mix other sources together to generate output
		void OnMixBus(FMixerSourceBufferPtr OutMixerSourceBuffer);

	private:

		friend class FMixerSourceManager;

		FMixerSourceManager* SourceManager;
		TMap<uint32, FMixerSourceSubmixSend> SubmixSends;
		FMixerDevice* MixerDevice;
		TArray<float> ChannelMap;
		FThreadSafeCounter NumBuffersQueued;
		float Pitch;
		float Volume;
		float DistanceAttenuation;
		float Distance;
		float LPFFrequency;
		float HPFFrequency;
		int32 SourceId;
		uint16 bIsPlaying : 1;
		uint16 bIsPaused : 1;
		uint16 bIsActive : 1;
		uint16 bOutputToBusOnly : 1;
		uint16 bIsBus : 1;
	};

}
