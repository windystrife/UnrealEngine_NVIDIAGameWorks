// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"
#include "AudioMixerBuffer.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerBus.h"
#include "DSP/OnePole.h"
#include "DSP/Filter.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/ParamInterpolator.h"
#include "IAudioExtensionPlugin.h"
#include "Containers/Queue.h"


namespace Audio
{
	class FMixerSubmix;
	class FMixerDevice;
	class FMixerSourceVoice;

	/** Struct defining a source voice buffer. */
	struct FMixerSourceVoiceBuffer
	{
		/** PCM float data. */
		TArray<float> AudioData;

		/** The amount of samples of audio data in the float buffer array. */
		uint32 Samples;

		/** How many times this buffer will loop. */
		int32 LoopCount;

		/** If this buffer is from real-time decoding and needs to make callbacks for more data. */
		uint32 bRealTimeBuffer : 1;

		FMixerSourceVoiceBuffer()
		{
			FMemory::Memzero(this, sizeof(*this));
		}
	};

	typedef TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> FMixerSourceBufferPtr;
	typedef TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixPtr;

	// Task used to store pending release/decode data
	struct FPendingReleaseData
	{
		FSoundBuffer* Buffer;
		IAudioTask* Task;

		FPendingReleaseData()
			: Buffer(nullptr)
			, Task(nullptr)
		{}
	};

	class ISourceBufferQueueListener
	{
	public:
		// Called when the current buffer is finished and a new one needs to be queued
		virtual void OnSourceBufferEnd() = 0;

		// Called when the buffer queue listener is released. Allows cleaning up any resources from render thread.
		virtual void OnRelease(TArray<FPendingReleaseData*>& OutPendingReleaseData) = 0;
	};

	struct FMixerSourceSubmixSend
	{
		// The submix ptr
		FMixerSubmixPtr Submix;

		// The amount of audio that is to be mixed into this submix
		float SendLevel;

		// Whather or not this is the primary send (i.e. first in the send chain)
		bool bIsMainSend;
	};

	// Struct holding mappings of bus ids (unique ids) to send level
	struct FMixerBusSend
	{
		uint32 BusId;
		float SendLevel;
	};

	struct FMixerSourceVoiceInitParams
	{
		ISourceBufferQueueListener* BufferQueueListener;
		TArray<FMixerSourceSubmixSend> SubmixSends;
		TArray<FMixerBusSend> BusSends;
		uint32 BusId;
		float BusDuration;
		uint32 SourceEffectChainId;
		TArray<FSourceEffectChainEntry> SourceEffectChain;
		FMixerSourceVoice* SourceVoice;
		int32 NumInputChannels;
		int32 NumInputFrames;
		FString DebugName;
		USpatializationPluginSourceSettingsBase* SpatializationPluginSettings;
		UOcclusionPluginSourceSettingsBase* OcclusionPluginSettings;
		UReverbPluginSourceSettingsBase* ReverbPluginSettings;
		FName AudioComponentUserID;
		bool bPlayEffectChainTails;
		bool bUseHRTFSpatialization;
		bool bIsDebugMode;
		bool bOutputToBusOnly;

		FMixerSourceVoiceInitParams()
			: BufferQueueListener(nullptr)
			, BusId(INDEX_NONE)
			, BusDuration(0.0f)
			, SourceEffectChainId(INDEX_NONE)
			, SourceVoice(nullptr)
			, NumInputChannels(0)
			, NumInputFrames(0)
			, SpatializationPluginSettings(nullptr)
			, OcclusionPluginSettings(nullptr)
			, ReverbPluginSettings(nullptr)
			, bPlayEffectChainTails(false)
			, bUseHRTFSpatialization(false)
			, bIsDebugMode(false)
			, bOutputToBusOnly(false)
		{}
	};

	class FSourceChannelMap
	{
	public:
		FSourceChannelMap() {}

		FORCEINLINE void Reset()
		{
			ChannelValues.Reset();
		}

		FORCEINLINE void SetChannelMap(const TArray<float>& ChannelMap, const int32 InNumInterpFrames)
		{
			if (ChannelValues.Num() != ChannelMap.Num())
			{
				ChannelValues.Reset();
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues.Add(FParam());
					ChannelValues[i].SetValue(ChannelMap[i], InNumInterpFrames);
				}
			}
			else
			{
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues[i].SetValue(ChannelMap[i], InNumInterpFrames);
				}
			}
		}

		FORCEINLINE void UpdateChannelMap()
		{
			const int32 NumChannelValues = ChannelValues.Num();
			for (int32 i = 0; i < NumChannelValues; ++i)
			{
				ChannelValues[i].Update();
			}
		}

		FORCEINLINE void ResetInterpolation()
		{
			const int32 NumChannelValues = ChannelValues.Num();
			for (int32 i = 0; i < NumChannelValues; ++i)
			{
				ChannelValues[i].Reset();
			}
		}

		FORCEINLINE float GetChannelValue(int ChannelIndex)
		{
			return ChannelValues[ChannelIndex].GetValue();
		}

		FORCEINLINE void PadZeroes(const int32 ToSize, const int32 InNumInterpFrames)
		{
			int32 CurrentSize = ChannelValues.Num();
			for (int32 i = CurrentSize; i < ToSize; ++i)
			{
				ChannelValues.Add(FParam());
				ChannelValues[i].SetValue(0.0f, InNumInterpFrames);
			}
		}

	private:
		TArray<FParam> ChannelValues;
	};

	struct FSourceManagerInitParams
	{
		// Total number of sources to use in the source manager
		int32 NumSources;

		// Number of worker threads to use for the source manager.
		int32 NumSourceWorkers;
	};

	class FMixerSourceManager
	{
	public:
		FMixerSourceManager(FMixerDevice* InMixerDevice);
		~FMixerSourceManager();

		void Init(const FSourceManagerInitParams& InitParams);
		void Update();

		bool GetFreeSourceId(int32& OutSourceId);
		int32 GetNumActiveSources() const;
		int32 GetNumActiveBuses() const;

		void ReleaseSourceId(const int32 SourceId);
		void InitSource(const int32 SourceId, const FMixerSourceVoiceInitParams& InitParams);

		void Play(const int32 SourceId);
		void Stop(const int32 SourceId);
		void Pause(const int32 SourceId);
		void SetPitch(const int32 SourceId, const float Pitch);
		void SetVolume(const int32 SourceId, const float Volume);
		void SetDistanceAttenuation(const int32 SourceId, const float DistanceAttenuation);
		void SetSpatializationParams(const int32 SourceId, const FSpatializationParams& InParams);
		void SetChannelMap(const int32 SourceId, const TArray<float>& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly);
		void SetLPFFrequency(const int32 SourceId, const float Frequency);
		void SetHPFFrequency(const int32 SourceId, const float Frequency);

		void SubmitBuffer(const int32 SourceId, FMixerSourceBufferPtr InSourceVoiceBuffer, const bool bSubmitSynchronously);

		int64 GetNumFramesPlayed(const int32 SourceId) const;
		bool IsDone(const int32 SourceId) const;
		bool IsEffectTailsDone(const int32 SourceId) const;
		bool NeedsSpeakerMap(const int32 SourceId) const;
		void ComputeNextBlockOfSamples();
		void MixOutputBuffers(const int32 SourceId, AlignedFloatBuffer& OutWetBuffer, const float SendLevel) const;

		void SetSubmixSendInfo(const int32 SourceId, const FMixerSourceSubmixSend& SubmixSend);

		void UpdateDeviceChannelCount(const int32 InNumOutputChannels);

		void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails);

		const float* GetPreDistanceAttenuationBuffer(const int32 SourceId) const;
		const float* GetPreviousBusBuffer(const int32 SourceId) const;
		int32 GetNumChannels(const int32 SourceId) const;
		int32 GetNumOutputFrames() const { return NumOutputFrames; }
		bool IsBus(const int32 SourceId) const;
		void PumpCommandQueue();
		void UpdatePendingReleaseData(bool bForceWait = false);

	private:

		void ReleaseSource(const int32 SourceId);
		void BuildSourceEffectChain(const int32 SourceId, FSoundEffectSourceInitData& InitData, const TArray<FSourceEffectChainEntry>& SourceEffectChain);
		void ResetSourceEffectChain(const int32 SourceId);
		void ReadSourceFrame(const int32 SourceId);

		void GenerateSourceAudio(const bool bGenerateBuses);
		void GenerateSourceAudio(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ComputeSourceBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputePostSourceEffectBufferForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputeOutputBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ComputeBuses();
		void UpdateBuses();

		void AudioMixerThreadCommand(TFunction<void()> InFunction);

		static const int32 NUM_BYTES_PER_SAMPLE = 2;

		// Private class which perform source buffer processing in a worker task
		class FAudioMixerSourceWorker : public FNonAbandonableTask
		{
			FMixerSourceManager* SourceManager;
			int32 StartSourceId;
			int32 EndSourceId;
			bool bGenerateBuses;

		public:
			FAudioMixerSourceWorker(FMixerSourceManager* InSourceManager, const int32 InStartSourceId, const int32 InEndSourceId)
				: SourceManager(InSourceManager)
				, StartSourceId(InStartSourceId)
				, EndSourceId(InEndSourceId)
				, bGenerateBuses(false)
			{
			}

			void SetGenerateBuses(bool bInGenerateBuses)
			{
				bGenerateBuses = bInGenerateBuses;
			}

			void DoWork()
			{
				SourceManager->GenerateSourceAudio(bGenerateBuses, StartSourceId, EndSourceId);
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FAudioMixerSourceWorker, STATGROUP_ThreadPoolAsyncTasks);
			}
		};

		FMixerDevice* MixerDevice;

		// Cached ptr to an optional spatialization plugin
		TAudioSpatializationPtr SpatializationPlugin;

		// Array of pointers to game thread audio source objects
		TArray<FMixerSourceVoice*> MixerSources;

		// A command queue to execute commands from audio thread (or game thread) to audio mixer device thread.
		struct FCommands
		{
			TQueue<TFunction<void()>> SourceCommandQueue;
		};

		FCommands CommandBuffers[2];
		FThreadSafeCounter AudioThreadCommandBufferIndex;
		FThreadSafeCounter RenderThreadCommandBufferIndex;

		TArray<int32> DebugSoloSources;

		struct FSourceInfo
		{
			FSourceInfo() {}
			~FSourceInfo() {}

			// Raw PCM buffer data
			TQueue<FMixerSourceBufferPtr> BufferQueue;
			ISourceBufferQueueListener* BufferQueueListener;

			// Data used for rendering sources
			FMixerSourceBufferPtr CurrentPCMBuffer;
			int32 CurrentAudioChunkNumFrames;

			// The post-attenuation source buffer, used to send audio to submixes
			TArray<float> SourceBuffer;
			TArray<float> PreDistanceAttenuationBuffer;

			TArray<float> CurrentFrameValues;
			TArray<float> NextFrameValues;
			float CurrentFrameAlpha;
			int32 CurrentFrameIndex;
			int64 NumFramesPlayed;

			TArray<FMixerSourceSubmixSend> SubmixSends;

			// What bus Id this source is, if it is a bus. This is INDEX_NONE for sources which are not buses.
			uint32 BusId;

			// Number of samples to count for bus
			int64 BusDurationFrames;

			// What buses this source is sending its audio to. Used to remove this source from the bus send list.
			TArray<uint32> BusSends;

			// Interpolated source params
			FParam PitchSourceParam;
			FParam VolumeSourceParam;
			FParam DistanceAttenuationSourceParam;
			FParam LPFCutoffFrequencyParam;
			FParam HPFCutoffFrequencyParam;

			// One-Pole LPFs and HPFs per source
			Audio::FOnePoleLPFBank LowPassFilter;
			Audio::FOnePoleFilter HighPassFilter;

			// Source effect instances
			uint32 SourceEffectChainId;
			TArray<FSoundEffectSource*> SourceEffects;
			TArray<USoundEffectSourcePreset*> SourceEffectPresets;
			bool bEffectTailsDone;
			FSoundEffectSourceInputData SourceEffectInputData;
			FSoundEffectSourceOutputData SourceEffectOutputData;

			FAudioPluginSourceOutputData AudioPluginOutputData;

			// A DSP object which tracks the amplitude envelope of a source.
			Audio::FEnvelopeFollower SourceEnvelopeFollower;
			float SourceEnvelopeValue;

			FSourceChannelMap ChannelMapParam;
			FSpatializationParams SpatParams;
			TArray<float> ScratchChannelMap;

			// Output data, after computing a block of sample data, this is read back from mixers
			TArray<float> ReverbPluginOutputBuffer;
			TArray<float>* PostEffectBuffers;
			TArray<float> OutputBuffer;

			// State management
			bool bIs3D;
			bool bIsCenterChannelOnly;
			bool bIsActive;
			bool bIsPlaying;
			bool bIsPaused;
			bool bHasStarted;
			bool bIsBusy;
			bool bUseHRTFSpatializer;
			bool bUseOcclusionPlugin;
			bool bUseReverbPlugin;
			bool bIsDone;
			bool bIsLastBuffer;
			bool bOutputToBusOnly;

			bool bIsDebugMode;
			FString DebugName;

			// Source format info
			int32 NumInputChannels;
			int32 NumPostEffectChannels;
			int32 NumInputFrames;
		};

		void ApplyDistanceAttenuation(FSourceInfo& InSourceInfo, int32 NumSamples);
		void ComputePluginAudio(FSourceInfo& InSourceInfo, int32 SourceId, int32 NumSamples);

		// Array of source infos.
		TArray<FSourceInfo> SourceInfos;

		// Array of active source ids
		TArray<int32> ActiveSourceIds;

		// Map of bus object Id's to bus data. 
		TMap<uint32, FMixerBus> Buses;

		// Async task workers for processing sources in parallel
		TArray<FAsyncTask<FAudioMixerSourceWorker>*> SourceWorkers;

		// Array of task data waiting to finished. Processed on audio render thread.
		TArray<FPendingReleaseData*> PendingReleaseData;

		// General information about sources in source manager accessible from game thread
		struct FGameThreadInfo
		{
			TArray<int32> FreeSourceIndices;
			TArray<bool> bIsBusy;
			TArray<FThreadSafeBool> bIsDone;
			TArray<FThreadSafeBool> bEffectTailsDone;
			TArray <bool> bNeedsSpeakerMap;
			TArray<bool> bIsDebugMode;
		} GameThreadInfo;

		int32 NumActiveSources;
		int32 NumTotalSources;
		int32 NumOutputFrames;
		int32 NumOutputSamples;
		int32 NumSourceWorkers;

		uint8 bInitialized : 1;
		uint8 bUsingSpatializationPlugin : 1;

		friend class FMixerSourceVoice;
		// Set to true when the audio source manager should pump the command queue
		FThreadSafeBool bPumpQueue;
	};
}
