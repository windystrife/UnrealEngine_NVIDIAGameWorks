// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UnrealAudioTypes.h"
#include "Containers/Queue.h"
#include "UnrealAudioVoiceInternal.h"
#include "UnrealAudioDecode.h"
#include "UnrealAudioSampleRateConverter.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	struct FVoiceMixerVoiceData
	{
		uint32 VoiceManagerIndex;
		float VolumeProduct;
		float PitchProduct;
		TSharedPtr<ISoundFile> SoundFile;
	};

	struct FVoiceParamData
	{
		float Current;
		float Target;
		float Delta;
		uint32 FrameCount;

		FVoiceParamData()
			: Current(0.0f)
			, Target(0.0f)
			, Delta(0.0f)
			, FrameCount(0)
		{}

		void Init(float Value)
		{
			Current = Value;
			Target = Value;
			Delta = 0.0f;
			FrameCount = 0;
		}

	};

	struct FVoiceDataEntry
	{
		uint32 NumChannels;
		uint32 FrameRate;
		double CurrentFrame;
		bool IsActive;
		FVoiceParamData Volume;
		FVoiceParamData Pitch;
		FSampleRateConverter SampleRateConverter;

		FVoiceDataEntry()
			: NumChannels(0)
			, FrameRate(0)
			, CurrentFrame(0.0)
			, IsActive(false)
		{}

	};

	namespace EAudioDeviceThreadMixCommand
	{
		enum Type
		{
			NONE = 0,

			VOICE_INIT_ENTRY,
			VOICE_RELEASE_ENTRY,
			VOICE_SET_VOLUME_PRODUCT,
			VOICE_SET_PITCH_PRODUCT,
			VOICE_SET_LISTENER_RELATIVE_ANGLE,

		};
	}

	namespace EAudioSystemThreadMixCommand
	{
		enum Type
		{
			NONE = 0,

			VOICE_DONE,
		};
	}

	class FVoiceMixer
	{
	public:
		FVoiceMixer(FUnrealAudioModule* InAudioModule, class FVoiceManager* InVoiceManager);
		~FVoiceMixer();

		void Init(const struct FVoiceManagerSettings& Settings);

		int32 InitEntry(const FVoiceMixerVoiceData& Data);
		void ReleaseEntry(uint32 VoiceIndex);

		void UpdateDeviceThread(struct FCallbackInfo& CallbackInfo);

		void UpdateAudioSystem();

		uint32 GetNumActiveVoices() const;

	private:
		void PumpDeviceThreadMessages();
		void FillDecodedBuffer(uint32 VoiceIndex, TArray<float>& InDecodeBuffer);

		void InitAudioSystemThreadId();
		void InitDeviceThreadId();
		void CheckAudioSystemThread();
		void CheckDeviceThread();

		FUnrealAudioModule* AudioModule;

		// Parent voice manager object
		FVoiceManager* VoiceManager;

		// Asynchronous decoders of audio file data
		TArray<FSoundFileDecoder> SoundFileDecoders;
		uint32 NumVoicesPerDecoder;

		uint32 MaxVoices;
		FThreadSafeCounter NumActiveVoices;
		float ControlUpdateRateSeconds;

		// Scratch buffer we are going to mix audio into from decoders
		TArray<float> DecodeBuffer;

		TQueue<int32> FreeVoiceIndices;

		TCommandQueue<FCommand> DeviceThreadCommandQueue;

		TCommandQueue<FCommand> AudioSystemCommandQueue;

		TArray<FVoiceDataEntry> VoiceData;


#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		uint32 AudioThreadId;
		uint32 DeviceThreadId;
#endif // #if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING

	};


}

#endif // #if ENABLE_UNREAL_AUDIO
