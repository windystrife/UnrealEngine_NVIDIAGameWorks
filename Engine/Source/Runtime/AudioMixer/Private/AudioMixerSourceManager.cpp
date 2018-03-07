// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceManager.h"
#include "AudioMixerSource.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceVoice.h"
#include "AudioMixerSubmix.h"
#include "IAudioExtensionPlugin.h"
#include "AudioMixer.h"

DEFINE_STAT(STAT_AudioMixerHRTF);

static int32 DisableParallelSourceProcessingCvar = 1;
FAutoConsoleVariableRef CVarDisableParallelSourceProcessing(
	TEXT("au.DisableParallelSourceProcessing"),
	DisableParallelSourceProcessingCvar,
	TEXT("Disables using async tasks for processing sources.\n")
	TEXT("0: Not Disabled, 1: Disabled"),
	ECVF_Default);


#define ONEOVERSHORTMAX (3.0517578125e-5f) // 1/32768
#define ENVELOPE_TAIL_THRESHOLD (1.58489e-5f) // -96 dB

#define VALIDATE_SOURCE_MIXER_STATE 1

#if AUDIO_MIXER_ENABLE_DEBUG_MODE

// Macro which checks if the source id is in debug mode, avoids having a bunch of #ifdefs in code
#define AUDIO_MIXER_DEBUG_LOG(SourceId, Format, ...)																							\
	if (SourceInfos[SourceId].bIsDebugMode)																													\
	{																																			\
		FString CustomMessage = FString::Printf(Format, ##__VA_ARGS__);																			\
		FString LogMessage = FString::Printf(TEXT("<Debug Sound Log> [Id=%d][Name=%s]: %s"), SourceId, *SourceInfos[SourceId].DebugName, *CustomMessage);	\
		UE_LOG(LogAudioMixer, Log, TEXT("%s"), *LogMessage);																								\
	}

#else

#define AUDIO_MIXER_DEBUG_LOG(SourceId, Message)

#endif

namespace Audio
{
	/*************************************************************************
	* FMixerSourceManager
	**************************************************************************/

	FMixerSourceManager::FMixerSourceManager(FMixerDevice* InMixerDevice)
		: MixerDevice(InMixerDevice)
		, NumActiveSources(0)
		, NumTotalSources(0)
		, NumOutputFrames(0)
		, NumOutputSamples(0)
		, NumSourceWorkers(4)
		, bInitialized(false)
		, bUsingSpatializationPlugin(false)
	{
	}

	FMixerSourceManager::~FMixerSourceManager()
	{
		if (SourceWorkers.Num() > 0)
		{
			for (int32 i = 0; i < SourceWorkers.Num(); ++i)
			{
				delete SourceWorkers[i];
				SourceWorkers[i] = nullptr;
			}

			SourceWorkers.Reset();
		}
	}

	void FMixerSourceManager::Init(const FSourceManagerInitParams& InitParams)
	{
		AUDIO_MIXER_CHECK(InitParams.NumSources > 0);

		if (bInitialized || !MixerDevice)
		{
			return;
		}

		AUDIO_MIXER_CHECK(MixerDevice->GetSampleRate() > 0);

		NumTotalSources = InitParams.NumSources;

		NumOutputFrames = MixerDevice->PlatformSettings.CallbackBufferFrameSize;
		NumOutputSamples = NumOutputFrames * MixerDevice->GetNumDeviceChannels();

		MixerSources.Init(nullptr, NumTotalSources);

		SourceInfos.AddDefaulted(NumTotalSources);

		for (int32 i = 0; i < NumTotalSources; ++i)
		{
			FSourceInfo& SourceInfo = SourceInfos[i];

			SourceInfo.BufferQueueListener = nullptr;
			SourceInfo.CurrentPCMBuffer = nullptr;	
			SourceInfo.CurrentAudioChunkNumFrames = 0;
			SourceInfo.CurrentFrameAlpha = 0.0f;
			SourceInfo.CurrentFrameIndex = 0;
			SourceInfo.NumFramesPlayed = 0;
			SourceInfo.SubmixSends.Reset();
			SourceInfo.BusId = INDEX_NONE;
			SourceInfo.BusDurationFrames = INDEX_NONE;
			SourceInfo.BusSends.Reset();

			SourceInfo.SourceEffectChainId = INDEX_NONE;

			SourceInfo.SourceEnvelopeFollower = Audio::FEnvelopeFollower(MixerDevice->SampleRate, 10, 100, Audio::EPeakMode::Peak);
			SourceInfo.SourceEnvelopeValue = 0.0f;
			SourceInfo.bEffectTailsDone = false;
			SourceInfo.PostEffectBuffers = nullptr;
		
			SourceInfo.bIs3D = false;
			SourceInfo.bIsCenterChannelOnly = false;
			SourceInfo.bIsActive = false;
			SourceInfo.bIsPlaying = false;
			SourceInfo.bIsPaused = false;
			SourceInfo.bIsDone = false;
			SourceInfo.bIsLastBuffer = false;
			SourceInfo.bIsBusy = false;
			SourceInfo.bUseHRTFSpatializer = false;
			SourceInfo.bUseOcclusionPlugin = false;
			SourceInfo.bUseReverbPlugin = false;
			SourceInfo.bHasStarted = false;
			SourceInfo.bIsDebugMode = false;
			SourceInfo.bOutputToBusOnly = false;

			SourceInfo.NumInputChannels = 0;
			SourceInfo.NumPostEffectChannels = 0;
			SourceInfo.NumInputFrames = 0;
		}
		
		GameThreadInfo.bIsBusy.AddDefaulted(NumTotalSources);
		GameThreadInfo.bIsDone.AddDefaulted(NumTotalSources);
		GameThreadInfo.bEffectTailsDone.AddDefaulted(NumTotalSources);
		GameThreadInfo.bNeedsSpeakerMap.AddDefaulted(NumTotalSources);
		GameThreadInfo.bIsDebugMode.AddDefaulted(NumTotalSources);
		GameThreadInfo.FreeSourceIndices.Reset(NumTotalSources);
		for (int32 i = NumTotalSources - 1; i >= 0; --i)
		{
			GameThreadInfo.FreeSourceIndices.Add(i);
		}

		// Initialize the source buffer memory usage to max source scratch buffers (num frames times max source channels)
		for (int32 SourceId = 0; SourceId < NumTotalSources; ++SourceId)
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			SourceInfo.SourceBuffer.Reset(NumOutputFrames * 8);
			SourceInfo.PreDistanceAttenuationBuffer.Reset(NumOutputFrames * 8);
			SourceInfo.AudioPluginOutputData.AudioBuffer.Reset(NumOutputFrames * 2);
		}

		// Setup the source workers
		SourceWorkers.Reset();
		if (NumSourceWorkers > 0)
		{
			const int32 NumSourcesPerWorker = NumTotalSources / NumSourceWorkers;
			int32 StartId = 0;
			int32 EndId = 0;
			while (EndId < NumTotalSources)
			{
				EndId = FMath::Min(StartId + NumSourcesPerWorker, NumTotalSources);
				SourceWorkers.Add(new FAsyncTask<FAudioMixerSourceWorker>(this, StartId, EndId));
				StartId = EndId;
			}
		}
		NumSourceWorkers = SourceWorkers.Num();

		// Cache the spatialization plugin
		SpatializationPlugin = MixerDevice->SpatializationPluginInterface;
		if (SpatializationPlugin.IsValid())
		{
			bUsingSpatializationPlugin = true;
		}

		bInitialized = true;
		bPumpQueue = false;
	}

	void FMixerSourceManager::Update()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

#if VALIDATE_SOURCE_MIXER_STATE
		for (int32 i = 0; i < NumTotalSources; ++i)
		{
			if (!GameThreadInfo.bIsBusy[i])
			{
				// Make sure that our bIsFree and FreeSourceIndices are correct
				AUDIO_MIXER_CHECK(GameThreadInfo.FreeSourceIndices.Contains(i) == true);
			}
		}
#endif

		int32 CurrentRenderIndex = RenderThreadCommandBufferIndex.GetValue();
		int32 CurrentGameIndex = AudioThreadCommandBufferIndex.GetValue();
		check(CurrentGameIndex == 0 || CurrentGameIndex == 1);
		check(CurrentRenderIndex == 0 || CurrentRenderIndex == 1);

		// If these values are the same, that means the audio render thread has finished the last buffer queue so is ready for the next block
		if (CurrentRenderIndex == CurrentGameIndex)
		{
			// This flags the audio render thread to be able to pump the next batch of commands
			// And will allow the audio thread to write to a new command slot
			const int32 NextIndex = !CurrentGameIndex;

			// Make sure we've actually emptied the command queue from the render thread before writing to it
			check(CommandBuffers[NextIndex].SourceCommandQueue.IsEmpty());
			AudioThreadCommandBufferIndex.Set(NextIndex);
			bPumpQueue = true;
		}
	}

	void FMixerSourceManager::ReleaseSource(const int32 SourceId)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(bInitialized);
		AUDIO_MIXER_CHECK(MixerSources[SourceId] != nullptr);

		AUDIO_MIXER_DEBUG_LOG(SourceId, TEXT("Is releasing"));
		
		FSourceInfo& SourceInfo = SourceInfos[SourceId];

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		if (SourceInfo.bIsDebugMode)
		{
			DebugSoloSources.Remove(SourceId);
		}
#endif
		// Remove from list of active bus or source ids depending on what type of source this is
		if (SourceInfo.BusId != INDEX_NONE)
		{
			// Remove this bus from the registry of bus instances
			FMixerBus* Bus = Buses.Find(SourceInfo.BusId);
			AUDIO_MIXER_CHECK(Bus);

			// Remove this source from the list of bus instances.
			if (Bus->RemoveInstanceId(SourceId))
			{
				Buses.Remove(SourceInfo.BusId);
			}
		}
		else
		{
			ActiveSourceIds.Remove(SourceId);
		}

		// Remove this source's send list from the bus data registry
		for (uint32 BusId : SourceInfo.BusSends)
		{
			// we should have a bus registration entry still since the send hasn't been cleaned up yet
			FMixerBus* Bus = Buses.Find(BusId);
			AUDIO_MIXER_CHECK(Bus);

			if (Bus->RemoveBusSend(SourceId))
			{
				Buses.Remove(BusId);
			}
		}

		// Call OnRelease on the BufferQueueListener to give it a chance 
		// to release any resources it owns on the audio render thread
		if (SourceInfo.BufferQueueListener)
		{
			SourceInfo.BufferQueueListener->OnRelease(PendingReleaseData);
			SourceInfo.BufferQueueListener = nullptr;
		}

		// Remove the mixer source from its submix sends
		for (FMixerSourceSubmixSend& SubmixSendItem : SourceInfo.SubmixSends)
		{
			SubmixSendItem.Submix->RemoveSourceVoice(MixerSources[SourceId]);
		}
		SourceInfo.SubmixSends.Reset();

		// Notify plugin effects
		if (SourceInfo.bUseHRTFSpatializer)
		{
			AUDIO_MIXER_CHECK(bUsingSpatializationPlugin);
			SpatializationPlugin->OnReleaseSource(SourceId);
		}

		if (SourceInfo.bUseOcclusionPlugin)
		{
			MixerDevice->OcclusionInterface->OnReleaseSource(SourceId);
		}

		if (SourceInfo.bUseReverbPlugin)
		{
			MixerDevice->ReverbPluginInterface->OnReleaseSource(SourceId);
		}

		// Delete the source effects
		SourceInfo.SourceEffectChainId = INDEX_NONE;
		ResetSourceEffectChain(SourceId);

		SourceInfo.BusId = INDEX_NONE;
		SourceInfo.BusDurationFrames = INDEX_NONE;
		SourceInfo.BusSends.Reset();

		SourceInfo.SourceEnvelopeFollower.Reset();
		SourceInfo.bEffectTailsDone = true;

		// Release the source voice back to the mixer device. This is pooled.
		MixerDevice->ReleaseMixerSourceVoice(MixerSources[SourceId]);
		MixerSources[SourceId] = nullptr;

		// Reset all state and data
		SourceInfo.PitchSourceParam.Init();
		SourceInfo.VolumeSourceParam.Init();
		SourceInfo.DistanceAttenuationSourceParam.Init();
		SourceInfo.LPFCutoffFrequencyParam.Init();

		SourceInfo.LowPassFilter.Reset();
		SourceInfo.HighPassFilter.Reset();
		SourceInfo.ChannelMapParam.Reset();
		SourceInfo.BufferQueue.Empty();
		SourceInfo.CurrentPCMBuffer = nullptr;
		SourceInfo.CurrentAudioChunkNumFrames = 0;
		SourceInfo.SourceBuffer.Reset();
		SourceInfo.PreDistanceAttenuationBuffer.Reset();
		SourceInfo.AudioPluginOutputData.AudioBuffer.Reset();
		SourceInfo.CurrentFrameValues.Reset();
		SourceInfo.NextFrameValues.Reset();
		SourceInfo.CurrentFrameAlpha = 0.0f;
		SourceInfo.CurrentFrameIndex = 0;
		SourceInfo.NumFramesPlayed = 0;
		SourceInfo.PostEffectBuffers = nullptr;
		SourceInfo.OutputBuffer.Reset();
		SourceInfo.bIs3D = false;
		SourceInfo.bIsCenterChannelOnly = false;
		SourceInfo.bIsActive = false;
		SourceInfo.bIsPlaying = false;
		SourceInfo.bIsDone = true;
		SourceInfo.bIsLastBuffer = false;
		SourceInfo.bIsPaused = false;
		SourceInfo.bIsBusy = false;
		SourceInfo.bUseHRTFSpatializer = false;
		SourceInfo.bUseOcclusionPlugin = false;
		SourceInfo.bUseReverbPlugin = false;
		SourceInfo.bHasStarted = false;
		SourceInfo.bIsDebugMode = false;
		SourceInfo.bOutputToBusOnly = false;
		SourceInfo.DebugName = FString();
		SourceInfo.NumInputChannels = 0;
		SourceInfo.NumPostEffectChannels = 0;

		GameThreadInfo.bNeedsSpeakerMap[SourceId] = false;
	}

	void FMixerSourceManager::BuildSourceEffectChain(const int32 SourceId, FSoundEffectSourceInitData& InitData, const TArray<FSourceEffectChainEntry>& InSourceEffectChain)
	{
		// Create new source effects. The memory will be owned by the source manager.
		for (const FSourceEffectChainEntry& ChainEntry : InSourceEffectChain)
		{
			// Presets can have null entries
			if (!ChainEntry.Preset)
			{
				continue;
			}

			FSoundEffectSource* NewEffect = static_cast<FSoundEffectSource*>(ChainEntry.Preset->CreateNewEffect());
			NewEffect->RegisterWithPreset(ChainEntry.Preset);

			// Get this source effect presets unique id so instances can identify their originating preset object
			const uint32 PresetUniqueId = ChainEntry.Preset->GetUniqueID();
			InitData.ParentPresetUniqueId = PresetUniqueId;

			NewEffect->Init(InitData);
			NewEffect->SetPreset(ChainEntry.Preset);
			NewEffect->SetEnabled(!ChainEntry.bBypass);

			// Add the effect instance
			FSourceInfo& SourceInfo = SourceInfos[SourceId];
			SourceInfo.SourceEffects.Add(NewEffect);

			// Add a slot entry for the preset so it can change while running. This will get sent to the running effect instance if the preset changes.
			SourceInfo.SourceEffectPresets.Add(nullptr);
		}
	}

	void FMixerSourceManager::ResetSourceEffectChain(const int32 SourceId)
	{
		FSourceInfo& SourceInfo = SourceInfos[SourceId];

		for (int32 i = 0; i < SourceInfo.SourceEffects.Num(); ++i)
		{
			SourceInfo.SourceEffects[i]->UnregisterWithPreset();
			delete SourceInfo.SourceEffects[i];
			SourceInfo.SourceEffects[i] = nullptr;
		}
		SourceInfo.SourceEffects.Reset();

		for (int32 i = 0; i < SourceInfo.SourceEffectPresets.Num(); ++i)
		{
			SourceInfo.SourceEffectPresets[i] = nullptr;
		}
		SourceInfo.SourceEffectPresets.Reset();
	}

	bool FMixerSourceManager::GetFreeSourceId(int32& OutSourceId)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (GameThreadInfo.FreeSourceIndices.Num())
		{
			OutSourceId = GameThreadInfo.FreeSourceIndices.Pop();

			AUDIO_MIXER_CHECK(OutSourceId < NumTotalSources);
			AUDIO_MIXER_CHECK(!GameThreadInfo.bIsBusy[OutSourceId]);

			AUDIO_MIXER_CHECK(!GameThreadInfo.bIsDebugMode[OutSourceId]);
			AUDIO_MIXER_CHECK(NumActiveSources < NumTotalSources);
			++NumActiveSources;

			GameThreadInfo.bIsBusy[OutSourceId] = true;
			return true;
		}
		AUDIO_MIXER_CHECK(false);
		return false;
	}

	int32 FMixerSourceManager::GetNumActiveSources() const
	{
		return NumActiveSources;
	}

	int32 FMixerSourceManager::GetNumActiveBuses() const
	{
		return Buses.Num();
	}

	void FMixerSourceManager::InitSource(const int32 SourceId, const FMixerSourceVoiceInitParams& InitParams)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK(!GameThreadInfo.bIsDebugMode[SourceId]);
		AUDIO_MIXER_CHECK(InitParams.BufferQueueListener != nullptr);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		GameThreadInfo.bIsDebugMode[SourceId] = InitParams.bIsDebugMode;
#endif 

		AudioMixerThreadCommand([this, SourceId, InitParams]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
			AUDIO_MIXER_CHECK(InitParams.SourceVoice != nullptr);

			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			SourceInfo.bIsPlaying = false;
			SourceInfo.bIsPaused = false;
			SourceInfo.bIsActive = true;
			SourceInfo.bIsBusy = true;
			SourceInfo.bIsDone = false;
			SourceInfo.bIsLastBuffer = false;
			SourceInfo.bUseHRTFSpatializer = InitParams.bUseHRTFSpatialization;

			SourceInfo.BufferQueueListener = InitParams.BufferQueueListener;
			SourceInfo.NumInputChannels = InitParams.NumInputChannels;
			SourceInfo.NumInputFrames = InitParams.NumInputFrames;

			// Initialize the number of per-source LPF filters based on input channels
			SourceInfo.LowPassFilter.Init(MixerDevice->SampleRate, InitParams.NumInputChannels);

			SourceInfo.HighPassFilter.Init(MixerDevice->SampleRate, InitParams.NumInputChannels, 0, nullptr);
			SourceInfo.HighPassFilter.SetFilterType(EFilter::Type::HighPass);

			// Create the spatialization plugin source effect
			if (InitParams.bUseHRTFSpatialization)
			{
				AUDIO_MIXER_CHECK(bUsingSpatializationPlugin);
				SpatializationPlugin->OnInitSource(SourceId, InitParams.AudioComponentUserID, InitParams.SpatializationPluginSettings);
			}

			// Create the occlusion plugin source effect
			if (InitParams.OcclusionPluginSettings != nullptr)
			{
				MixerDevice->OcclusionInterface->OnInitSource(SourceId, InitParams.AudioComponentUserID, InitParams.NumInputChannels, InitParams.OcclusionPluginSettings);
				SourceInfo.bUseOcclusionPlugin = true;
			}

			// Create the reverb plugin source effect
			if (InitParams.ReverbPluginSettings != nullptr)
			{
				MixerDevice->ReverbPluginInterface->OnInitSource(SourceId, InitParams.AudioComponentUserID, InitParams.NumInputChannels, InitParams.ReverbPluginSettings);
				SourceInfo.bUseReverbPlugin = true;
			}

			// Default all sounds to not consider effect chain tails when playing
			SourceInfo.bEffectTailsDone = true;

			// Copy the source effect chain if the channel count is 1 or 2
			if (InitParams.NumInputChannels <= 2)
			{
				// If we're told to care about effect chain tails, then we're not allowed
				// to stop playing until the effect chain tails are finished
				SourceInfo.bEffectTailsDone = !InitParams.bPlayEffectChainTails;

				FSoundEffectSourceInitData InitData;
				InitData.SampleRate = MixerDevice->SampleRate;
				InitData.NumSourceChannels = InitParams.NumInputChannels;
				InitData.AudioClock = MixerDevice->GetAudioTime();

				if (InitParams.NumInputFrames != INDEX_NONE)
				{
					InitData.SourceDuration = (float)InitParams.NumInputFrames / MixerDevice->SampleRate;
				}
				else
				{
					// Procedural sound waves have no known duration
					InitData.SourceDuration = (float)INDEX_NONE;
				}

				SourceInfo.SourceEffectChainId = InitParams.SourceEffectChainId;
				BuildSourceEffectChain(SourceId, InitData, InitParams.SourceEffectChain);

				// Whether or not to output to bus only
				SourceInfo.bOutputToBusOnly = InitParams.bOutputToBusOnly;

				// If this is a bus, add this source id to the list of active bus ids
				if (InitParams.BusId != INDEX_NONE)
				{
					// Setting this BusId will flag this source as a bus. It doesn't try to generate 
					// audio in the normal way but instead will render in a second stage, after normal source rendering.
					SourceInfo.BusId = InitParams.BusId;

					// Bus duration allows us to stop a bus after a given time
					if (InitParams.BusDuration != 0.0f)
					{
						SourceInfo.BusDurationFrames = InitParams.BusDuration * MixerDevice->GetSampleRate();
					}

					// Register this bus as an instance
					FMixerBus* Bus = Buses.Find(InitParams.BusId);
					if (Bus)
					{
						// If this bus is already registered, add this as a source id
						Bus->AddInstanceId(SourceId);
					}
					else
					{
						// If the bus is not registered, make a new entry
						FMixerBus NewBusData(this, InitParams.NumInputChannels, NumOutputFrames);

						NewBusData.AddInstanceId(SourceId);

						Buses.Add(InitParams.BusId, NewBusData);
					}
				}

				// Iterate through source's bus sends and add this source to the bus send list
				// Note: buses can also send their audio to other buses.
				for (const FMixerBusSend& BusSend : InitParams.BusSends)
				{
					// New struct to map which source (SourceId) is sending to the bus
					FBusSend NewBusSend;
					NewBusSend.SourceId = SourceId;
					NewBusSend.SendLevel = BusSend.SendLevel;

					// Get existing BusId and add the send, or create new bus registration
					FMixerBus* Bus = Buses.Find(BusSend.BusId);
					if (Bus)
					{
						Bus->AddBusSend(NewBusSend);
					}
					else
					{
						// If the bus is not registered, make a new entry
						FMixerBus NewBusData(this, InitParams.NumInputChannels, NumOutputFrames);

						// Add a send to it. This will not have a bus instance id (i.e. won't output audio), but 
						// we register the send anyway in the event that this bus does play, we'll know to send this
						// source's audio to it.
						NewBusData.AddBusSend(NewBusSend);

						Buses.Add(BusSend.BusId, NewBusData);
					}

					// Store on this source, which buses its sending its audio to
					SourceInfo.BusSends.Add(BusSend.BusId);
				}
			}

			SourceInfo.CurrentFrameValues.Init(0.0f, InitParams.NumInputChannels);
			SourceInfo.NextFrameValues.Init(0.0f, InitParams.NumInputChannels);

			AUDIO_MIXER_CHECK(MixerSources[SourceId] == nullptr);
			MixerSources[SourceId] = InitParams.SourceVoice;

			// Loop through the source's sends and add this source to those submixes with the send info

			AUDIO_MIXER_CHECK(SourceInfo.SubmixSends.Num() == 0);

			for (int32 i = 0; i < InitParams.SubmixSends.Num(); ++i)
			{
				const FMixerSourceSubmixSend& MixerSubmixSend = InitParams.SubmixSends[i];
				SourceInfo.SubmixSends.Add(MixerSubmixSend);
				MixerSubmixSend.Submix->AddOrSetSourceVoice(InitParams.SourceVoice, MixerSubmixSend.SendLevel);
			}

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
			AUDIO_MIXER_CHECK(!SourceInfo.bIsDebugMode);
			SourceInfo.bIsDebugMode = InitParams.bIsDebugMode;

			AUDIO_MIXER_CHECK(SourceInfo.DebugName.IsEmpty());
			SourceInfo.DebugName = InitParams.DebugName;
#endif 

			AUDIO_MIXER_DEBUG_LOG(SourceId, TEXT("Is initializing"));
		});
	}

	void FMixerSourceManager::ReleaseSourceId(const int32 SourceId)
	{
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AUDIO_MIXER_CHECK(NumActiveSources > 0);
		--NumActiveSources;

		GameThreadInfo.bIsBusy[SourceId] = false;

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		GameThreadInfo.bIsDebugMode[SourceId] = false;
#endif

		GameThreadInfo.FreeSourceIndices.Push(SourceId);

		AUDIO_MIXER_CHECK(GameThreadInfo.FreeSourceIndices.Contains(SourceId));

		AudioMixerThreadCommand([this, SourceId]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			ReleaseSource(SourceId);
		});
	}

	void FMixerSourceManager::Play(const int32 SourceId)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			SourceInfo.bIsPlaying = true;
			SourceInfo.bIsPaused = false;
			SourceInfo.bIsActive = true;

			AUDIO_MIXER_DEBUG_LOG(SourceId, TEXT("Is playing"));
		});
	}

	void FMixerSourceManager::Stop(const int32 SourceId)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			SourceInfo.bIsPlaying = false;
			SourceInfo.bIsPaused = false;
			SourceInfo.bIsActive = false;

			AUDIO_MIXER_DEBUG_LOG(SourceId, TEXT("Is stopping"));
		});
	}

	void FMixerSourceManager::Pause(const int32 SourceId)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			SourceInfo.bIsPaused = true;
			SourceInfo.bIsActive = false;
		});
	}

	void FMixerSourceManager::SetPitch(const int32 SourceId, const float Pitch)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);

		AudioMixerThreadCommand([this, SourceId, Pitch]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
			check(NumOutputFrames > 0);

			SourceInfos[SourceId].PitchSourceParam.SetValue(Pitch, NumOutputFrames);
		});
	}

	void FMixerSourceManager::SetVolume(const int32 SourceId, const float Volume)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, Volume]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
			check(NumOutputFrames > 0);

			SourceInfos[SourceId].VolumeSourceParam.SetValue(Volume, NumOutputFrames);
		});
	}

	void FMixerSourceManager::SetDistanceAttenuation(const int32 SourceId, const float DistanceAttenuation)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, DistanceAttenuation]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
			check(NumOutputFrames > 0);

			SourceInfos[SourceId].DistanceAttenuationSourceParam.SetValue(DistanceAttenuation, NumOutputFrames);
		});
	}

	void FMixerSourceManager::SetSpatializationParams(const int32 SourceId, const FSpatializationParams& InParams)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, InParams]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			SourceInfos[SourceId].SpatParams = InParams;
		});
	}

	void FMixerSourceManager::SetChannelMap(const int32 SourceId, const TArray<float>& ChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, ChannelMap, bInIs3D, bInIsCenterChannelOnly]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			check(NumOutputFrames > 0);

			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			// Set whether or not this is a 3d channel map and if its center channel only. Used for reseting channel maps on device change.
			SourceInfo.bIs3D = bInIs3D;
			SourceInfo.bIsCenterChannelOnly = bInIsCenterChannelOnly;

			// Fix up the channel map in case device output count changed
			const int32 NumSourceChannels = SourceInfo.bUseHRTFSpatializer ? 2 : SourceInfo.NumInputChannels;
			const int32 NumOutputChannels = MixerDevice->GetNumDeviceChannels();
			const int32 ChannelMapSize = NumSourceChannels * NumOutputChannels;

			// If this is true, then the device changed while the command was in-flight
			if (ChannelMap.Num() != ChannelMapSize)
			{
				TArray<float> NewChannelMap;

				// If 3d then just zero it out, we'll get another channel map shortly
				if (bInIs3D)
				{
					NewChannelMap.AddZeroed(ChannelMapSize);
					GameThreadInfo.bNeedsSpeakerMap[SourceId] = true;
				}
				// Otherwise, get an appropriate channel map for the new device configuration
				else
				{
					MixerDevice->Get2DChannelMap(NumSourceChannels, NumOutputChannels, bInIsCenterChannelOnly, NewChannelMap);
				}
				SourceInfo.ChannelMapParam.SetChannelMap(NewChannelMap, NumOutputFrames);
			}
			else
			{
				GameThreadInfo.bNeedsSpeakerMap[SourceId] = false;
				SourceInfo.ChannelMapParam.SetChannelMap(ChannelMap, NumOutputFrames);
			}

		});
	}

	void FMixerSourceManager::SetLPFFrequency(const int32 SourceId, const float InLPFFrequency)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, InLPFFrequency]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			SourceInfos[SourceId].LPFCutoffFrequencyParam.SetValue(InLPFFrequency, NumOutputFrames);
		});
	}

	void FMixerSourceManager::SetHPFFrequency(const int32 SourceId, const float InHPFFrequency)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, InHPFFrequency]()
		{
			AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

			SourceInfos[SourceId].HPFCutoffFrequencyParam.SetValue(InHPFFrequency, NumOutputFrames);
		});
	}

	void FMixerSourceManager::SubmitBuffer(const int32 SourceId, FMixerSourceBufferPtr InSourceVoiceBuffer, const bool bSubmitSynchronously)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(InSourceVoiceBuffer->Samples <= (uint32)InSourceVoiceBuffer->AudioData.Num());

		// If we're submitting synchronously then don't use AudioMixerThreadCommand but submit directly
		if (bSubmitSynchronously)
		{
			SourceInfos[SourceId].BufferQueue.Enqueue(InSourceVoiceBuffer);
		}
		else
		{
			AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

			// Use a thread command
			AudioMixerThreadCommand([this, SourceId, InSourceVoiceBuffer]()
			{
				AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

				SourceInfos[SourceId].BufferQueue.Enqueue(InSourceVoiceBuffer);
			});
		}
	}

	void FMixerSourceManager::SetSubmixSendInfo(const int32 SourceId, const FMixerSourceSubmixSend& InSubmixSend)
	{
		AUDIO_MIXER_CHECK(SourceId < NumTotalSources);
		AUDIO_MIXER_CHECK(GameThreadInfo.bIsBusy[SourceId]);
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		AudioMixerThreadCommand([this, SourceId, InSubmixSend]()
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			bool bIsNew = true;
			for (FMixerSourceSubmixSend& SubmixSend : SourceInfo.SubmixSends)
			{
				if (SubmixSend.Submix->GetId() == InSubmixSend.Submix->GetId())
				{
					SubmixSend.SendLevel = InSubmixSend.SendLevel;
					bIsNew = false;
					break;
				}
			}

			if (bIsNew)
			{
				SourceInfos[SourceId].SubmixSends.Add(InSubmixSend);
			}

			InSubmixSend.Submix->AddOrSetSourceVoice(MixerSources[SourceId], InSubmixSend.SendLevel);
		});
	}

	int64 FMixerSourceManager::GetNumFramesPlayed(const int32 SourceId) const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);
		return SourceInfos[SourceId].NumFramesPlayed;
	}

	bool FMixerSourceManager::IsDone(const int32 SourceId) const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);
		return GameThreadInfo.bIsDone[SourceId];
	}

	bool FMixerSourceManager::IsEffectTailsDone(const int32 SourceId) const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);
		return GameThreadInfo.bEffectTailsDone[SourceId];
	}

	bool FMixerSourceManager::NeedsSpeakerMap(const int32 SourceId) const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);
		return GameThreadInfo.bNeedsSpeakerMap[SourceId];
	}

	void FMixerSourceManager::ReadSourceFrame(const int32 SourceId)
	{
		FSourceInfo& SourceInfo = SourceInfos[SourceId];

		const int32 NumChannels = SourceInfo.NumInputChannels;

		// Check if the next frame index is out of range of the total number of frames we have in our current audio buffer
		bool bNextFrameOutOfRange = (SourceInfo.CurrentFrameIndex + 1) >= SourceInfo.CurrentAudioChunkNumFrames;
		bool bCurrentFrameOutOfRange = SourceInfo.CurrentFrameIndex >= SourceInfo.CurrentAudioChunkNumFrames;

		bool bReadCurrentFrame = true;

		// Check the boolean conditions that determine if we need to pop buffers from our queue (in PCMRT case) *OR* loop back (looping PCM data)
		while (bNextFrameOutOfRange || bCurrentFrameOutOfRange)
		{
			// If our current frame is in range, but next frame isn't, read the current frame now to avoid pops when transitioning between buffers
			if (bNextFrameOutOfRange && !bCurrentFrameOutOfRange)
			{
				// Don't need to read the current frame audio after reading new audio chunk
				bReadCurrentFrame = false;

				AUDIO_MIXER_CHECK(SourceInfo.CurrentPCMBuffer.IsValid());
				const float* AudioData = SourceInfo.CurrentPCMBuffer->AudioData.GetData();
				const int32 CurrentSampleIndex = SourceInfo.CurrentFrameIndex * NumChannels;

				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					SourceInfo.CurrentFrameValues[Channel] = AudioData[CurrentSampleIndex + Channel];
				}
			}

			// If this is our first PCM buffer, we don't need to do a callback to get more audio
			if (SourceInfo.CurrentPCMBuffer.IsValid())
			{
				if (SourceInfo.CurrentPCMBuffer->LoopCount == Audio::LOOP_FOREVER && !SourceInfo.CurrentPCMBuffer->bRealTimeBuffer)
				{
					AUDIO_MIXER_DEBUG_LOG(SourceId, TEXT("Hit Loop boundary, looping."));

					SourceInfo.CurrentFrameIndex = FMath::Max(SourceInfo.CurrentFrameIndex - SourceInfo.CurrentAudioChunkNumFrames, 0);
					break;
				}

				SourceInfo.BufferQueueListener->OnSourceBufferEnd();
			}

			// If we have audio in our queue, we're still playing
			if (!SourceInfo.BufferQueue.IsEmpty())
			{
				FMixerSourceBufferPtr NewBufferPtr;
				SourceInfo.BufferQueue.Dequeue(NewBufferPtr);
				SourceInfo.CurrentPCMBuffer = NewBufferPtr;

				AUDIO_MIXER_CHECK(MixerSources[SourceId]->NumBuffersQueued.GetValue() > 0);

				MixerSources[SourceId]->NumBuffersQueued.Decrement();

				SourceInfo.CurrentAudioChunkNumFrames = SourceInfo.CurrentPCMBuffer->Samples / NumChannels;

				// Subtract the number of frames in the current buffer from our frame index.
				// Note: if this is the first time we're playing, CurrentFrameIndex will be 0
				if (bReadCurrentFrame)
				{
					SourceInfo.CurrentFrameIndex = FMath::Max(SourceInfo.CurrentFrameIndex - SourceInfo.CurrentAudioChunkNumFrames, 0);
				}
				else
				{
					// Since we're not reading the current frame, we allow the current frame index to be negative (NextFrameIndex will then be 0)
					// This prevents dropping a frame of audio on the buffer boundary
					SourceInfo.CurrentFrameIndex = -1;
				}
			}
			else
			{
				SourceInfo.bIsLastBuffer = true;
				return;
			}

			bNextFrameOutOfRange = (SourceInfo.CurrentFrameIndex + 1) >= SourceInfo.CurrentAudioChunkNumFrames;
			bCurrentFrameOutOfRange = SourceInfo.CurrentFrameIndex >= SourceInfo.CurrentAudioChunkNumFrames;
		}

		if (SourceInfo.CurrentPCMBuffer.IsValid())
		{
			// Grab the float PCM audio data (which could be a new audio chunk from previous ReadSourceFrame call)
			const float* AudioData = SourceInfo.CurrentPCMBuffer->AudioData.GetData();
			const int32 NextSampleIndex = (SourceInfo.CurrentFrameIndex + 1)  * NumChannels;

			if (bReadCurrentFrame)
			{
				const int32 CurrentSampleIndex = SourceInfo.CurrentFrameIndex * NumChannels;
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					SourceInfo.CurrentFrameValues[Channel] = AudioData[CurrentSampleIndex + Channel];
					SourceInfo.NextFrameValues[Channel] = AudioData[NextSampleIndex + Channel];
				}
			}
			else
			{
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					SourceInfo.NextFrameValues[Channel] = AudioData[NextSampleIndex + Channel];
				}
			}
		}
	}

	void FMixerSourceManager::ComputeSourceBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd)
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioMixerSourceBuffers);

		for (int32 SourceId = SourceIdStart; SourceId < SourceIdEnd; ++SourceId)
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			if (!SourceInfo.bIsBusy || !SourceInfo.bIsPlaying || SourceInfo.bIsPaused)
			{
				continue;
			}

			// If this source is still playing at this point but technically done, zero the buffers. We haven't yet been removed by the FMixerSource owner.
			// This should be rare but could happen due to thread timing since done-ness is queried on audio thread.
			if (SourceInfo.bIsDone)
			{
				const int32 NumSamples = NumOutputFrames * SourceInfo.NumInputChannels;

				SourceInfo.PreDistanceAttenuationBuffer.Reset();
				SourceInfo.PreDistanceAttenuationBuffer.AddZeroed(NumSamples);

				SourceInfo.SourceBuffer.Reset();
				SourceInfo.SourceBuffer.AddZeroed(NumSamples);

				continue;
			}

			const bool bIsBus = SourceInfo.BusId != INDEX_NONE;
			if ((bGenerateBuses && !bIsBus) || (!bGenerateBuses && bIsBus))
			{
				continue;
			}

			// Fill array with elements all at once to avoid sequential Add() operation overhead.
			const int32 NumSamples = NumOutputFrames * SourceInfo.NumInputChannels;
			
			// Initialize both the pre-distance attenuation buffer and the source buffer
			SourceInfo.PreDistanceAttenuationBuffer.Reset();
			SourceInfo.PreDistanceAttenuationBuffer.AddZeroed(NumSamples);

			SourceInfo.SourceBuffer.Reset();
			SourceInfo.SourceBuffer.AddZeroed(NumSamples);

			float* PreDistanceAttenBufferPtr = SourceInfo.PreDistanceAttenuationBuffer.GetData();

			// if this is a bus, we just want to copy the bus audio to this source's output audio
			// Note we need to copy this since bus instances may have different audio via dynamic source effects, etc.
			if (bIsBus)
			{
				// Get the source's rendered bus data
				const FMixerBus* Bus = Buses.Find(SourceInfo.BusId);
				const float* BusBufferPtr = Bus->GetCurrentBusBuffer();

				int32 NumFramesPlayed = NumOutputFrames;
				if (SourceInfo.BusDurationFrames != INDEX_NONE)
				{
					// If we're now finishing, only copy over the real data
					if ((SourceInfo.NumFramesPlayed + NumOutputFrames) >= SourceInfo.BusDurationFrames)
					{
						NumFramesPlayed = SourceInfo.BusDurationFrames - SourceInfo.NumFramesPlayed;
						SourceInfo.bIsLastBuffer = true;				
					}
				}

				SourceInfo.NumFramesPlayed += NumFramesPlayed;

				// Simply copy into the pre distance attenuation buffer ptr
				FMemory::Memcpy(PreDistanceAttenBufferPtr, BusBufferPtr, sizeof(float) * NumFramesPlayed * SourceInfo.NumInputChannels);
			}
			// Otherwise we need to generate source audio using buffer queues and perform SRC
			else
			{
				int32 SampleIndex = 0;

				for (int32 Frame = 0; Frame < NumOutputFrames; ++Frame)
				{
					// If the source is done, then we'll just write out 0s
					if (!SourceInfo.bIsLastBuffer)
					{
						// Whether or not we need to read another sample from the source buffers
						// If we haven't yet played any frames, then we will need to read the first source samples no matter what
						bool bReadNextSample = !SourceInfo.bHasStarted;

						// Reset that we've started generating audio
						SourceInfo.bHasStarted = true;

						// Update the PrevFrameIndex value for the source based on alpha value
						while (SourceInfo.CurrentFrameAlpha >= 1.0f)
						{
							// Our inter-frame alpha lerping value is causing us to read new source frames
							bReadNextSample = true;

							// Bump up the current frame index
							SourceInfo.CurrentFrameIndex++;

							// Bump up the frames played -- this is tracking the total frames in source file played
							// CurrentFrameIndex can wrap for looping sounds so won't be accurate in that case
							SourceInfo.NumFramesPlayed++;

							SourceInfo.CurrentFrameAlpha -= 1.0f;
						}

						// If our alpha parameter caused us to jump to a new source frame, we need
						// read new samples into our prev and next frame sample data
						if (bReadNextSample)
						{
							ReadSourceFrame(SourceId);
						}
					}

					// If we've finished reading all buffer data, then just write out 0s
					if (SourceInfo.bIsLastBuffer)
					{
						for (int32 Channel = 0; Channel < SourceInfo.NumInputChannels; ++Channel)
						{
							PreDistanceAttenBufferPtr[SampleIndex++] = 0.0f;
						}
					}
					else
					{
						// perform linear SRC to get the next sample value from the decoded buffer
						for (int32 Channel = 0; Channel < SourceInfo.NumInputChannels; ++Channel)
						{
							const float CurrFrameValue = SourceInfo.CurrentFrameValues[Channel];
							const float NextFrameValue = SourceInfo.NextFrameValues[Channel];
							const float CurrentAlpha = SourceInfo.CurrentFrameAlpha;

							PreDistanceAttenBufferPtr[SampleIndex++] = FMath::Lerp(CurrFrameValue, NextFrameValue, CurrentAlpha);
						}
						const float CurrentPitchScale = SourceInfo.PitchSourceParam.Update();

						SourceInfo.CurrentFrameAlpha += CurrentPitchScale;
					}
				}

				// After processing the frames, reset the pitch param
				SourceInfo.PitchSourceParam.Reset();
			}
		}
	}

	void FMixerSourceManager::ComputeBuses()
	{
		// Loop through the bus registry and mix source audio
		for (auto& Entry : Buses)
		{
			FMixerBus& Bus = Entry.Value;
			Bus.MixBuffer();
		}
	}

	void FMixerSourceManager::UpdateBuses()
	{
		// Update the bus states post mixing. This flips the current/previous buffer indices.
		for (auto& Entry : Buses)
		{
			FMixerBus& Bus = Entry.Value;
			Bus.Update();
		}
	}

	void FMixerSourceManager::ApplyDistanceAttenuation(FSourceInfo& SourceInfo, int32 NumSamples)
	{
		float* PreDistanceAttenBufferPtr = SourceInfo.PreDistanceAttenuationBuffer.GetData();
		float* PostDistanceAttenBufferPtr = SourceInfo.SourceBuffer.GetData();

		// Interpolate the distance attenuation value to avoid discontinuities
		float CurrentDistanceAttenuationValue = SourceInfo.DistanceAttenuationSourceParam.GetValue();
		for (int32 Sample = 0; Sample < NumSamples; Sample += SourceInfo.NumInputChannels)
		{
			for (int32 Chan = 0; Chan < SourceInfo.NumInputChannels; ++Chan)
			{
				PostDistanceAttenBufferPtr[Sample + Chan] = PreDistanceAttenBufferPtr[Sample + Chan] * CurrentDistanceAttenuationValue;
			}

			CurrentDistanceAttenuationValue = SourceInfo.DistanceAttenuationSourceParam.Update();
		}
		SourceInfo.DistanceAttenuationSourceParam.Reset();
	}

	void FMixerSourceManager::ComputePluginAudio(FSourceInfo& SourceInfo, int32 SourceId, int32 NumSamples)
	{
		// Apply the distance attenuation before sending to plugins
		float* PreDistanceAttenBufferPtr = SourceInfo.PreDistanceAttenuationBuffer.GetData();
		float* PostDistanceAttenBufferPtr = SourceInfo.SourceBuffer.GetData();

		bool bShouldMixInReverb = false;
		if (SourceInfo.bUseReverbPlugin)
		{
			const FSpatializationParams* SourceSpatParams = &SourceInfo.SpatParams;

			// Move the audio buffer to the reverb plugin buffer
			FAudioPluginSourceInputData AudioPluginInputData;
			AudioPluginInputData.SourceId = SourceId;
			AudioPluginInputData.AudioBuffer = &SourceInfo.SourceBuffer;
			AudioPluginInputData.SpatializationParams = SourceSpatParams;
			AudioPluginInputData.NumChannels = SourceInfo.NumInputChannels;
			SourceInfo.AudioPluginOutputData.AudioBuffer.Reset();
			SourceInfo.AudioPluginOutputData.AudioBuffer.AddZeroed(AudioPluginInputData.AudioBuffer->Num());

			MixerDevice->ReverbPluginInterface->ProcessSourceAudio(AudioPluginInputData, SourceInfo.AudioPluginOutputData);

			// Make sure the buffer counts didn't change and are still the same size
			AUDIO_MIXER_CHECK(SourceInfo.AudioPluginOutputData.AudioBuffer.Num() == NumSamples);

			//If the reverb effect doesn't send it's audio to an external device, mix the output data back in.
			if (!MixerDevice->bReverbIsExternalSend)
			{
				// Copy the reverb-processed data back to the source buffer
				SourceInfo.ReverbPluginOutputBuffer.Reset();
				SourceInfo.ReverbPluginOutputBuffer.Append(SourceInfo.AudioPluginOutputData.AudioBuffer);
				bShouldMixInReverb = true;
			}
		}

		if (SourceInfo.bUseOcclusionPlugin)
		{
			const FSpatializationParams* SourceSpatParams = &SourceInfo.SpatParams;

			// Move the audio buffer to the occlusion plugin buffer
			FAudioPluginSourceInputData AudioPluginInputData;
			AudioPluginInputData.SourceId = SourceId;
			AudioPluginInputData.AudioBuffer = &SourceInfo.SourceBuffer;
			AudioPluginInputData.SpatializationParams = SourceSpatParams;
			AudioPluginInputData.NumChannels = SourceInfo.NumInputChannels;

			SourceInfo.AudioPluginOutputData.AudioBuffer.Reset();
			SourceInfo.AudioPluginOutputData.AudioBuffer.AddZeroed(AudioPluginInputData.AudioBuffer->Num());

			MixerDevice->OcclusionInterface->ProcessAudio(AudioPluginInputData, SourceInfo.AudioPluginOutputData);

			// Make sure the buffer counts didn't change and are still the same size
			AUDIO_MIXER_CHECK(SourceInfo.AudioPluginOutputData.AudioBuffer.Num() == NumSamples);

			// Copy the occlusion-processed data back to the source buffer and mix with the reverb plugin output buffer
			if (bShouldMixInReverb)
			{
				const float* ReverbPluginOutputBufferPtr = SourceInfo.ReverbPluginOutputBuffer.GetData();
				const float* AudioPluginOutputDataPtr = SourceInfo.AudioPluginOutputData.AudioBuffer.GetData();

				for (int32 i = 0; i < NumSamples; ++i)
				{
					PostDistanceAttenBufferPtr[i] = ReverbPluginOutputBufferPtr[i] + AudioPluginOutputDataPtr[i];
				}
			}
			else
			{
				FMemory::Memcpy(PostDistanceAttenBufferPtr, SourceInfo.AudioPluginOutputData.AudioBuffer.GetData(), sizeof(float) * NumSamples);
			}
		}
		else if (bShouldMixInReverb)
		{
			const float* ReverbPluginOutputBufferPtr = SourceInfo.ReverbPluginOutputBuffer.GetData();
			for (int32 i = 0; i < NumSamples; ++i)
			{
				PostDistanceAttenBufferPtr[i] += ReverbPluginOutputBufferPtr[i];
			}
		}

		// If the source has HRTF processing enabled, run it through the spatializer
		if (SourceInfo.bUseHRTFSpatializer)
		{
			SCOPE_CYCLE_COUNTER(STAT_AudioMixerHRTF);

			AUDIO_MIXER_CHECK(SpatializationPlugin.IsValid());
			AUDIO_MIXER_CHECK(SourceInfo.NumInputChannels == 1);

			FAudioPluginSourceInputData AudioPluginInputData;
			AudioPluginInputData.AudioBuffer = &SourceInfo.SourceBuffer;
			AudioPluginInputData.NumChannels = SourceInfo.NumInputChannels;
			AudioPluginInputData.SourceId = SourceId;
			AudioPluginInputData.SpatializationParams = &SourceInfo.SpatParams;

			if (!MixerDevice->bSpatializationIsExternalSend)
			{
				SourceInfo.AudioPluginOutputData.AudioBuffer.Reset();
				SourceInfo.AudioPluginOutputData.AudioBuffer.AddZeroed(2 * NumOutputFrames);
			}

			SpatializationPlugin->ProcessAudio(AudioPluginInputData, SourceInfo.AudioPluginOutputData);

			// If this is an external send, we treat this source audio as if it was still a mono source
			// This will allow it to traditionally pan in the ComputeOutputBuffers function and be
			// sent to submixes (e.g. reverb) panned and mixed down. Certain submixes will want this spatial 
			// information in addition to the external send. We've already bypassed adding this source
			// to a base submix (e.g. master/eq, etc)
			if (MixerDevice->bSpatializationIsExternalSend)
			{
				// Otherwise our pre- and post-effect channels are the same as the input channels
				SourceInfo.NumPostEffectChannels = SourceInfo.NumInputChannels;

				// Set the ptr to use for post-effect buffers rather than the plugin output data (since the plugin won't have output audio data)
				SourceInfo.PostEffectBuffers = &SourceInfo.SourceBuffer;
			}
			else
			{
				// Otherwise, we are now a 2-channel file and should not be spatialized using normal 3d spatialization
				SourceInfo.NumPostEffectChannels = 2;
				SourceInfo.PostEffectBuffers = &SourceInfo.AudioPluginOutputData.AudioBuffer;
			}
		}
		else
		{
			// Otherwise our pre- and post-effect channels are the same as the input channels
			SourceInfo.NumPostEffectChannels = SourceInfo.NumInputChannels;

			// Set the ptr to use for post-effect buffers
			SourceInfo.PostEffectBuffers = &SourceInfo.SourceBuffer;
		}
	}

	void FMixerSourceManager::ComputePostSourceEffectBufferForIdRange(bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd)
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioMixerSourceEffectBuffers);

		const bool bIsDebugModeEnabled = DebugSoloSources.Num() > 0;

		for (int32 SourceId = SourceIdStart; SourceId < SourceIdEnd; ++SourceId)
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			if (!SourceInfo.bIsBusy || !SourceInfo.bIsPlaying || SourceInfo.bIsPaused || (SourceInfo.bIsDone && SourceInfo.bEffectTailsDone)) 
			{
				continue;
			}

			const bool bIsBus = SourceInfo.BusId != INDEX_NONE;
			if ((bGenerateBuses && !bIsBus) || (!bGenerateBuses && bIsBus))
			{
				continue;
			}

			float* PreDistanceAttenBufferPtr = SourceInfo.PreDistanceAttenuationBuffer.GetData();
			const int32 NumSamples = SourceInfo.PreDistanceAttenuationBuffer.Num();

			float CurrentVolumeValue = SourceInfo.VolumeSourceParam.GetValue();

			for (int32 Frame = 0; Frame < NumOutputFrames; ++Frame)
			{
				const float LPFFreq = SourceInfo.LPFCutoffFrequencyParam.Update();
				const float HPFFreq = SourceInfo.HPFCutoffFrequencyParam.Update();

#if AUDIO_MIXER_ENABLE_DEBUG_MODE				
				CurrentVolumeValue = (bIsDebugModeEnabled && !SourceInfo.bIsDebugMode) ? 0.0f : SourceInfo.VolumeSourceParam.Update();

#else
				CurrentVolumeValue = SourceInfo.VolumeSourceParam.Update();
#endif

				SourceInfo.LowPassFilter.SetFrequency(LPFFreq);

				SourceInfo.HighPassFilter.SetFrequency(HPFFreq);
				SourceInfo.HighPassFilter.Update();

				int32 SampleIndex = SourceInfo.NumInputChannels * Frame;

				SourceInfo.LowPassFilter.ProcessAudio(&PreDistanceAttenBufferPtr[SampleIndex], &PreDistanceAttenBufferPtr[SampleIndex]);
				SourceInfo.HighPassFilter.ProcessAudio(&PreDistanceAttenBufferPtr[SampleIndex], &PreDistanceAttenBufferPtr[SampleIndex]);

				// Scale by current volume value (note: not including distance attenuation). TODO: do this as a SIMD operation in its own loop
				for (int32 Channel = 0; Channel < SourceInfo.NumInputChannels; ++Channel)
				{
					PreDistanceAttenBufferPtr[SampleIndex + Channel] *= CurrentVolumeValue;
				}
			}

			// Reset the volume and LPF param interpolations
			SourceInfo.LPFCutoffFrequencyParam.Reset();
			SourceInfo.HPFCutoffFrequencyParam.Reset();
			SourceInfo.VolumeSourceParam.Reset();

			// Now process the effect chain if it exists
			if (SourceInfo.SourceEffects.Num() > 0)
			{
				SourceInfo.SourceEffectInputData.CurrentVolume = CurrentVolumeValue;

				SourceInfo.SourceEffectOutputData.AudioFrame.Reset();
				SourceInfo.SourceEffectOutputData.AudioFrame.AddZeroed(SourceInfo.NumInputChannels);

				SourceInfo.SourceEffectInputData.AudioFrame.Reset();
				SourceInfo.SourceEffectInputData.AudioFrame.AddZeroed(SourceInfo.NumInputChannels);

				float* SourceEffectInputDataFramePtr = SourceInfo.SourceEffectInputData.AudioFrame.GetData();
				float* SourceEffectOutputDataFramePtr = SourceInfo.SourceEffectOutputData.AudioFrame.GetData();

				// Process the effect chain for this buffer per frame
				for (int32 Sample = 0; Sample < NumSamples; Sample += SourceInfo.NumInputChannels)
				{
					// Get the buffer input sample
					for (int32 Chan = 0; Chan < SourceInfo.NumInputChannels; ++Chan)
					{
						SourceEffectInputDataFramePtr[Chan] = PreDistanceAttenBufferPtr[Sample + Chan];
					}

					for (int32 SourceEffectIndex = 0; SourceEffectIndex < SourceInfo.SourceEffects.Num(); ++SourceEffectIndex)
					{
						if (SourceInfo.SourceEffects[SourceEffectIndex]->IsActive())
						{
							SourceInfo.SourceEffects[SourceEffectIndex]->Update();

							SourceInfo.SourceEffects[SourceEffectIndex]->ProcessAudio(SourceInfo.SourceEffectInputData, SourceInfo.SourceEffectOutputData);

							// Copy the output of the effect into the input so the next effect will get the outputs audio
							for (int32 Chan = 0; Chan < SourceInfo.NumInputChannels; ++Chan)
							{
								SourceEffectInputDataFramePtr[Chan] = SourceEffectOutputDataFramePtr[Chan];
							}
						}
					}

					// Copy audio frame back to the buffer
					for (int32 Chan = 0; Chan < SourceInfo.NumInputChannels; ++Chan)
					{
						PreDistanceAttenBufferPtr[Sample + Chan] = SourceEffectInputDataFramePtr[Chan];
					}
				}
			}

			// Compute the source envelope using pre-distance attenuation buffer
			float AverageSampleValue;
			for (int32 Sample = 0; Sample < NumSamples;)
			{
				AverageSampleValue = 0.0f;
				for (int32 Chan = 0; Chan < SourceInfo.NumInputChannels; ++Chan)
				{
					AverageSampleValue += PreDistanceAttenBufferPtr[Sample++];
				}
				AverageSampleValue /= SourceInfo.NumInputChannels;
				SourceInfo.SourceEnvelopeFollower.ProcessAudio(AverageSampleValue);
			}

			// Copy the current value of the envelope follower (block-rate value)
			SourceInfo.SourceEnvelopeValue = SourceInfo.SourceEnvelopeFollower.GetCurrentValue();

			SourceInfo.bEffectTailsDone = SourceInfo.bEffectTailsDone || SourceInfo.SourceEnvelopeValue < ENVELOPE_TAIL_THRESHOLD;

			// Only scale with distance attenuation and send to source audio to plugins if we're not in output-to-bus only mode
			if (!SourceInfo.bOutputToBusOnly)
			{
				// Apply distance attenuation
				ApplyDistanceAttenuation(SourceInfo, NumSamples);

				// Send source audio to plugins
				ComputePluginAudio(SourceInfo, SourceId, NumSamples);
			}

			// Check the source effect tails condition
			if (SourceInfo.bIsLastBuffer && SourceInfo.bEffectTailsDone)
			{
				// If we're done and our tails our done, clear everything out
				SourceInfo.BufferQueue.Empty();
				SourceInfo.CurrentFrameValues.Reset();
				SourceInfo.NextFrameValues.Reset();
				SourceInfo.CurrentPCMBuffer = nullptr;
			}
		}
	}

	void FMixerSourceManager::ComputeOutputBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd)
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioMixerSourceOutputBuffers);

		const int32 NumOutputChannels = MixerDevice->GetNumDeviceChannels();

		for (int32 SourceId = SourceIdStart; SourceId < SourceIdEnd; ++SourceId)
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			// Don't need to compute anything if the source is not playing or paused (it will remain at 0.0 volume)
			// Note that effect chains will still be able to continue to compute audio output. The source output 
			// will simply stop being read from.
			if (!SourceInfo.bIsBusy || !SourceInfo.bIsPlaying || (SourceInfo.bIsDone && SourceInfo.bEffectTailsDone))
			{
				continue;
			}

			// If we're in generate buses mode and not a bus, or vice versa, or if we're set to only output audio to buses.
			// If set to output buses, no need to do any panning for the source. The buses will do the panning.
			const bool bIsBus = SourceInfo.BusId != INDEX_NONE;
			if ((bGenerateBuses && !bIsBus) || (!bGenerateBuses && bIsBus) || SourceInfo.bOutputToBusOnly)
			{
				continue;
			}

			// Zero the buffers for all cases, this will catch the pause state. We want to zero buffers when paused.
			SourceInfo.OutputBuffer.Reset();
			SourceInfo.OutputBuffer.AddZeroed(NumOutputSamples);

			// If we're paused, then early return now
			if (SourceInfo.bIsPaused)
			{
				continue;
			}

			for (int32 Frame = 0; Frame < NumOutputFrames; ++Frame)
			{
				const int32 PostEffectChannels = SourceInfo.NumPostEffectChannels;

				float SourceSampleValue = 0.0f;

				// Make sure that our channel map is appropriate for the source channel and output channel count!
				SourceInfo.ChannelMapParam.UpdateChannelMap();

				// For each source channel, compute the output channel mapping
				for (int32 SourceChannel = 0; SourceChannel < PostEffectChannels; ++SourceChannel)
				{
					const int32 SourceSampleIndex = Frame * PostEffectChannels + SourceChannel;
					TArray<float>* Buffer = SourceInfo.PostEffectBuffers;
					SourceSampleValue = (*Buffer)[SourceSampleIndex];

					for (int32 OutputChannel = 0; OutputChannel < NumOutputChannels; ++OutputChannel)
					{
						// Look up the channel map value (maps input channels to output channels) for the source
						// This is the step that either applies the spatialization algorithm or just maps a 2d sound
						const int32 ChannelMapIndex = NumOutputChannels * SourceChannel + OutputChannel;
						const float ChannelMapValue = SourceInfo.ChannelMapParam.GetChannelValue(ChannelMapIndex);

						// If we have a non-zero sample value, write it out. Note that most 3d audio channel maps
						// for surround sound will result in 0.0 sample values so this branch should save a bunch of multiplies + adds
						if (ChannelMapValue > 0.0f)
						{
							// Scale the input source sample for this source channel value
							const float SampleValue = SourceSampleValue * ChannelMapValue;

							const int32 OutputSampleIndex = Frame * NumOutputChannels + OutputChannel;

							SourceInfo.OutputBuffer[OutputSampleIndex] += SampleValue;
						}
					}
				}
			}

			// Reset the channel map param interpolation
			SourceInfo.ChannelMapParam.ResetInterpolation();
		}
	}

	void FMixerSourceManager::GenerateSourceAudio(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd)
	{
		// Buses generate their input buffers independently
		// Get the next block of frames from the source buffers
		ComputeSourceBuffersForIdRange(bGenerateBuses, SourceIdStart, SourceIdEnd);

		// Compute the audio source buffers after their individual effect chain processing
		ComputePostSourceEffectBufferForIdRange(bGenerateBuses, SourceIdStart, SourceIdEnd);

		// Get the audio for the output buffers
		ComputeOutputBuffersForIdRange(bGenerateBuses, SourceIdStart, SourceIdEnd);
	}

	void FMixerSourceManager::GenerateSourceAudio(const bool bGenerateBuses)
	{
		// If there are no buses, don't need to do anything here
		if (bGenerateBuses && !Buses.Num())
		{
			return;
		}

		if (NumSourceWorkers > 0 && !DisableParallelSourceProcessingCvar)
		{
			AUDIO_MIXER_CHECK(SourceWorkers.Num() == NumSourceWorkers);
			for (int32 i = 0; i < SourceWorkers.Num(); ++i)
			{
				FAudioMixerSourceWorker& Worker = SourceWorkers[i]->GetTask();
				Worker.SetGenerateBuses(bGenerateBuses);

				SourceWorkers[i]->StartBackgroundTask();
			}

			for (int32 i = 0; i < SourceWorkers.Num(); ++i)
			{
				SourceWorkers[i]->EnsureCompletion();
			}
		}
		else
		{
			GenerateSourceAudio(bGenerateBuses, 0, NumTotalSources);
		}
	}

	void FMixerSourceManager::MixOutputBuffers(const int32 SourceId, AlignedFloatBuffer& OutWetBuffer, const float SendLevel) const
	{
		if (SendLevel > 0.0f)
		{
			const FSourceInfo& SourceInfo = SourceInfos[SourceId];

			// Don't need to mix into submixes if the source is paused
			if (!SourceInfo.bIsPaused && !SourceInfo.bIsDone && SourceInfo.bIsPlaying)
			{
				const float* SourceOutputBufferPtr = SourceInfo.OutputBuffer.GetData();
				const int32 OutWetBufferSize = OutWetBuffer.Num();
				float* OutWetBufferPtr = OutWetBuffer.GetData();

				for (int32 SampleIndex = 0; SampleIndex < OutWetBufferSize; ++SampleIndex)
				{
					OutWetBufferPtr[SampleIndex] += SourceOutputBufferPtr[SampleIndex] * SendLevel;
				}
			}

// TOODO: SIMD
// 			const VectorRegister SendLevelScalar = VectorLoadFloat1(SendLevel);
// 			for (int32 SampleIndex = 0; SampleIndex < OutWetBuffer.Num(); SampleIndex += 4)
// 			{
// 				VectorRegister OutputBufferRegister = VectorLoadAligned(&SourceOutputBuffer[SampleIndex]);
// 				VectorRegister Temp = VectorMultiply(OutputBufferRegister, SendLevelScalar);
// 				VectorRegister OutputWetBuffer = VectorLoadAligned(&OutWetBuffer[SampleIndex]);
// 				Temp = VectorAdd(OutputWetBuffer, Temp);
// 
// 				VectorStoreAligned(Temp, &OutWetBuffer[SampleIndex]);
// 			}
		}
	}

	void FMixerSourceManager::UpdateDeviceChannelCount(const int32 InNumOutputChannels)
	{
		NumOutputSamples = NumOutputFrames * MixerDevice->GetNumDeviceChannels();

		// Update all source's to appropriate channel maps
		for (int32 SourceId = 0; SourceId < NumTotalSources; ++SourceId)
		{
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			// Don't need to do anything if it's not active
			if (!SourceInfo.bIsActive)
			{
				continue;
			}

			SourceInfo.ScratchChannelMap.Reset();
			const int32 NumSoureChannels = SourceInfo.bUseHRTFSpatializer ? 2 : SourceInfo.NumInputChannels;

			// If this is a 3d source, then just zero out the channel map, it'll cause a temporary blip
			// but it should reset in the next tick
			if (SourceInfo.bIs3D)
			{
				GameThreadInfo.bNeedsSpeakerMap[SourceId] = true;
				SourceInfo.ScratchChannelMap.AddZeroed(NumSoureChannels * InNumOutputChannels);
			}
			// If it's a 2D sound, then just get a new channel map appropriate for the new device channel count
			else
			{
				SourceInfo.ScratchChannelMap.Reset();
				MixerDevice->Get2DChannelMap(NumSoureChannels, InNumOutputChannels, SourceInfo.bIsCenterChannelOnly, SourceInfo.ScratchChannelMap);
			}
			SourceInfo.ChannelMapParam.SetChannelMap(SourceInfo.ScratchChannelMap, NumOutputFrames);
		}
	}

	void FMixerSourceManager::UpdateSourceEffectChain(const uint32 InSourceEffectChainId, const TArray<FSourceEffectChainEntry>& InSourceEffectChain, const bool bPlayEffectChainTails)
	{
		AudioMixerThreadCommand([this, InSourceEffectChainId, InSourceEffectChain, bPlayEffectChainTails]()
		{
			FSoundEffectSourceInitData InitData;
			InitData.AudioClock = MixerDevice->GetAudioClock();
			InitData.SampleRate = MixerDevice->SampleRate;

			for (int32 SourceId = 0; SourceId < NumTotalSources; ++SourceId)
			{
				FSourceInfo& SourceInfo = SourceInfos[SourceId];

				if (SourceInfo.SourceEffectChainId == InSourceEffectChainId)
				{
					SourceInfo.bEffectTailsDone = !bPlayEffectChainTails;

					// Check to see if the chain didn't actually change
					TArray<FSoundEffectSource *>& ThisSourceEffectChain = SourceInfo.SourceEffects;
					bool bReset = false;
					if (InSourceEffectChain.Num() == ThisSourceEffectChain.Num())
					{
						for (int32 SourceEffectId = 0; SourceEffectId < ThisSourceEffectChain.Num(); ++SourceEffectId)
						{
							const FSourceEffectChainEntry& ChainEntry = InSourceEffectChain[SourceEffectId];

							FSoundEffectSource* SourceEffectInstance = ThisSourceEffectChain[SourceEffectId];
							if (!SourceEffectInstance->IsParentPreset(ChainEntry.Preset))
							{
								// As soon as one of the effects change or is not the same, then we need to rebuild the effect graph
								bReset = true;
								break;
							}

							// Otherwise just update if it's just to bypass
							SourceEffectInstance->SetEnabled(!ChainEntry.bBypass);
						}
					}
					else
					{
						bReset = true;
					}

					if (bReset)
					{
						InitData.NumSourceChannels = SourceInfo.NumInputChannels;

						if (SourceInfo.NumInputFrames != INDEX_NONE)
						{
							InitData.SourceDuration = (float)SourceInfo.NumInputFrames / InitData.SampleRate;
						}
						else
						{
							// Procedural sound waves have no known duration
							InitData.SourceDuration = (float)INDEX_NONE;
						}

						// First reset the source effect chain
						ResetSourceEffectChain(SourceId);

						// Rebuild it
						BuildSourceEffectChain(SourceId, InitData, InSourceEffectChain);
					}
				}
			}
		});
	}

	const float* FMixerSourceManager::GetPreDistanceAttenuationBuffer(const int32 SourceId) const
	{
		return SourceInfos[SourceId].PreDistanceAttenuationBuffer.GetData();
	}

	const float* FMixerSourceManager::GetPreviousBusBuffer(const int32 SourceId) const
	{
		const uint32 BusId = SourceInfos[SourceId].BusId;
		const FMixerBus* MixerBus = Buses.Find(BusId);
		return MixerBus->GetPreviousBusBuffer();
	}

	int32 FMixerSourceManager::GetNumChannels(const int32 SourceId) const
	{
		return SourceInfos[SourceId].NumInputChannels;
	}

	bool FMixerSourceManager::IsBus(const int32 SourceId) const
	{
		return SourceInfos[SourceId].BusId != INDEX_NONE;
	}

	void FMixerSourceManager::ComputeNextBlockOfSamples()
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		SCOPE_CYCLE_COUNTER(STAT_AudioMixerSourceManagerUpdate);

		// Get the this blocks commands before rendering audio
		if (bPumpQueue)
		{
			bPumpQueue = false;
			PumpCommandQueue();
		}

		// Update pending tasks and release them if they're finished
		UpdatePendingReleaseData();

		// First generate non-bus audio (bGenerateBuses = false)
		GenerateSourceAudio(false);

		// Now mix in the non-bus audio into the buses
		ComputeBuses();

		// Now generate bus audio (bGenerateBuses = true)
		GenerateSourceAudio(true);

		// Update the buses now
		UpdateBuses();

		// Let the plugin know we finished processing all sources
		if (bUsingSpatializationPlugin)
		{
			AUDIO_MIXER_CHECK(SpatializationPlugin.IsValid());
			SpatializationPlugin->OnAllSourcesProcessed();
		}

		// Update the game thread copy of source doneness
		for (int32 SourceId = 0; SourceId < NumTotalSources; ++SourceId)
		{		
			FSourceInfo& SourceInfo = SourceInfos[SourceId];

			if (SourceInfo.bIsLastBuffer)
			{
				SourceInfo.bIsDone = true;
			}
			else
			{
				SourceInfo.bIsDone = false;
			}

			GameThreadInfo.bIsDone[SourceId] = SourceInfo.bIsDone;
			GameThreadInfo.bEffectTailsDone[SourceId] = SourceInfo.bEffectTailsDone;
		}
	}

	void FMixerSourceManager::AudioMixerThreadCommand(TFunction<void()> InFunction)
	{
		// Add the function to the command queue
		int32 AudioThreadCommandIndex = AudioThreadCommandBufferIndex.GetValue();
		CommandBuffers[AudioThreadCommandIndex].SourceCommandQueue.Enqueue(MoveTemp(InFunction));
	}

	void FMixerSourceManager::PumpCommandQueue()
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		int32 CurrentRenderThreadIndex = RenderThreadCommandBufferIndex.GetValue();

		FCommands& Commands = CommandBuffers[CurrentRenderThreadIndex];

		// Pop and execute all the commands that came since last update tick
		TFunction<void()> CommandFunction;
		while (Commands.SourceCommandQueue.Dequeue(CommandFunction))
		{
			CommandFunction();
		}

		RenderThreadCommandBufferIndex.Set(!CurrentRenderThreadIndex);
	}

	void FMixerSourceManager::UpdatePendingReleaseData(bool bForceWait)
	{
		// Don't block, but let tasks finish naturally
		for (int32 i = PendingReleaseData.Num() - 1; i >= 0; --i)
		{
			FPendingReleaseData* DataEntry = PendingReleaseData[i];
			if (DataEntry->Task)
			{
				bool bDeleteData = false;
				if (bForceWait)
				{
					DataEntry->Task->EnsureCompletion();
					bDeleteData = true;
				}
				else if (DataEntry->Task->IsDone())
				{
					bDeleteData = true;
				}

				if (bDeleteData)
				{
					delete DataEntry->Task;
					DataEntry->Task = nullptr;

					if (DataEntry->Buffer)
					{
						delete DataEntry->Buffer;
					}

					delete DataEntry;
					PendingReleaseData[i] = nullptr;

					PendingReleaseData.RemoveAtSwap(i, 1, false);
				}
			}
			else if (DataEntry->Buffer)
			{
				delete DataEntry->Buffer;
				DataEntry->Buffer = nullptr;

				PendingReleaseData.RemoveAtSwap(i, 1, false);
			}
		}
	}

}
