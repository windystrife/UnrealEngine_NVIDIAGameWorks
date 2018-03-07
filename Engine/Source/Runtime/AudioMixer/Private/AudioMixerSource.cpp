// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSource.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceVoice.h"
#include "ActiveSound.h"
#include "IAudioExtensionPlugin.h"
#include "Sound/AudioSettings.h"
#include "ContentStreaming.h"

static int32 DisableHRTFCvar = 0;
FAutoConsoleVariableRef CVarDisableHRTF(
	TEXT("au.DisableHRTF"),
	DisableHRTFCvar,
	TEXT("Disables HRTF\n")
	TEXT("0: Not Disabled, 1: Disabled"),
	ECVF_Default);

namespace Audio
{
	FMixerSource::FMixerSource(FAudioDevice* InAudioDevice)
		: FSoundSource(InAudioDevice)
		, MixerDevice((FMixerDevice*)InAudioDevice)
		, MixerBuffer(nullptr)
		, MixerSourceVoice(nullptr)
		, AsyncRealtimeAudioTask(nullptr)
		, CurrentBuffer(0)
		, PreviousAzimuth(-1.0f)
		, bPlayedCachedBuffer(false)
		, bPlaying(false)
		, bLoopCallback(false)
		, bIsFinished(false)
		, bIsPlayingEffectTails(false)
		, bBuffersToFlush(false)
		, bResourcesNeedFreeing(false)
		, bEditorWarnedChangedSpatialization(false)
		, bUsingHRTFSpatialization(false)
		, bIs3D(false)
		, bDebugMode(false)
	{
		// Create the source voice buffers
		for (int32 i = 0; i < Audio::MAX_BUFFERS_QUEUED; ++i)
		{
			SourceVoiceBuffers.Add(FMixerSourceBufferPtr(new FMixerSourceVoiceBuffer()));
		}
	}

	FMixerSource::~FMixerSource()
	{
		FreeResources();
	}

	bool FMixerSource::Init(FWaveInstance* InWaveInstance)
	{
		AUDIO_MIXER_CHECK(MixerBuffer);
		AUDIO_MIXER_CHECK(MixerBuffer->IsRealTimeSourceReady());

		FSoundSource::InitCommon();

		// Get the number of frames before creating the buffer
		int32 NumFrames = INDEX_NONE;

		if (InWaveInstance->WaveData->DecompressionType != DTYPE_Procedural)
		{
			const int32 NumBytes = InWaveInstance->WaveData->RawPCMDataSize;
			NumFrames = NumBytes / (InWaveInstance->WaveData->NumChannels * sizeof(int16));
		}

		// Reset our releasing bool
		bIsReleasing = false;

		FSoundBuffer* SoundBuffer = static_cast<FSoundBuffer*>(MixerBuffer);
		if (SoundBuffer->NumChannels > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_AudioSourceInitTime);

			AUDIO_MIXER_CHECK(MixerDevice);
			MixerSourceVoice = MixerDevice->GetMixerSourceVoice();
			if (!MixerSourceVoice)
			{
				return false;
			}

			// Initialize the source voice with the necessary format information
			FMixerSourceVoiceInitParams InitParams;
			InitParams.BufferQueueListener = this;
			InitParams.NumInputChannels = InWaveInstance->WaveData->NumChannels;
			InitParams.NumInputFrames = NumFrames;
			InitParams.SourceVoice = MixerSourceVoice;
			InitParams.bUseHRTFSpatialization = UseObjectBasedSpatialization();
			InitParams.AudioComponentUserID = InWaveInstance->ActiveSound->GetAudioComponentUserID();

			InitParams.SourceEffectChainId = 0;

			if (InitParams.NumInputChannels <= 2)
			{
				if (InWaveInstance->SourceEffectChain)
				{
					InitParams.SourceEffectChainId = InWaveInstance->SourceEffectChain->GetUniqueID();

					for (int32 i = 0; i < InWaveInstance->SourceEffectChain->Chain.Num(); ++i)
					{
						InitParams.SourceEffectChain.Add(InWaveInstance->SourceEffectChain->Chain[i]);
						InitParams.bPlayEffectChainTails = InWaveInstance->SourceEffectChain->bPlayEffectChainTails;
					}
				}

				// Setup the bus Id if this source is a bus
				if (InWaveInstance->WaveData->bIsBus)
				{
					InitParams.BusId = InWaveInstance->WaveData->GetUniqueID();
					if (!InWaveInstance->WaveData->IsLooping())
					{
						InitParams.BusDuration = InWaveInstance->WaveData->GetDuration();
					}
				}

				// Toggle muting the source if sending only to output bus. 
				// This can get set even if the source doesn't have bus sends since bus sends can be dynamically enabled.
				InitParams.bOutputToBusOnly = InWaveInstance->bOutputToBusOnly;

				// If this source is sending its audio to a bus
				if (InWaveInstance->SoundSourceBusSends.Num() > 0)
				{
					// And add all the source bus sends
					for (FSoundSourceBusSendInfo& SendInfo : InWaveInstance->SoundSourceBusSends)
					{
						if (SendInfo.SoundSourceBus != nullptr)
						{
							FMixerBusSend BusSend;
							BusSend.BusId = SendInfo.SoundSourceBus->GetUniqueID();
							BusSend.SendLevel = SendInfo.SendLevel;
							InitParams.BusSends.Add(BusSend);
						}
					}
				}
			}

			// Don't set up any submixing if we're set to output to bus only
			if (!InitParams.bOutputToBusOnly)
			{
				// If we're spatializing using HRTF and its an external send, don't need to setup a default/base submix send to master or EQ submix
				// We'll only be using non-default submix sends (e.g. reverb).
				if (!(InWaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && MixerDevice->bSpatializationIsExternalSend))
				{
					// If we've overridden which submix we're sending the sound, then add that as the first send
					if (InWaveInstance->SoundSubmix != nullptr)
					{
						FMixerSourceSubmixSend SubmixSend;
						SubmixSend.Submix = MixerDevice->GetSubmixInstance(InWaveInstance->SoundSubmix);
						SubmixSend.SendLevel = 1.0f;
						SubmixSend.bIsMainSend = true;
						InitParams.SubmixSends.Add(SubmixSend);
					}
					else
					{
						// Send the voice to the EQ submix if it's enabled
						const bool bIsEQDisabled = GetDefault<UAudioSettings>()->bDisableMasterEQ;
						if (!bIsEQDisabled && IsEQFilterApplied())
						{
							// Default the submix to use to use the master submix if none are set
							FMixerSourceSubmixSend SubmixSend;
							SubmixSend.Submix = MixerDevice->GetMasterEQSubmix();
							SubmixSend.SendLevel = 1.0f;
							SubmixSend.bIsMainSend = true;
							InitParams.SubmixSends.Add(SubmixSend);
						}
						else
						{
							// Default the submix to use to use the master submix if none are set
							FMixerSourceSubmixSend SubmixSend;
							SubmixSend.Submix = MixerDevice->GetMasterSubmix();
							SubmixSend.SendLevel = 1.0f;
							SubmixSend.bIsMainSend = true;
							InitParams.SubmixSends.Add(SubmixSend);
						}
					}
				}

				// Now add any addition submix sends for this source
				for (FSoundSubmixSendInfo& SendInfo : InWaveInstance->SoundSubmixSends)
				{
					if (SendInfo.SoundSubmix != nullptr)
					{
						FMixerSourceSubmixSend SubmixSend;
						SubmixSend.Submix = MixerDevice->GetSubmixInstance(SendInfo.SoundSubmix);
						SubmixSend.SendLevel = SendInfo.SendLevel;
						SubmixSend.bIsMainSend = false;
						InitParams.SubmixSends.Add(SubmixSend);
					}
				}
			}

			// Check to see if this sound has been flagged to be in debug mode
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
			InitParams.DebugName = InWaveInstance->GetName();

			bool bIsDebug = false;
			FString WaveInstanceName = WaveInstance->GetName(); //-V595
			FString TestName = GEngine->GetAudioDeviceManager()->GetAudioMixerDebugSoundName();
			if (WaveInstanceName.Contains(TestName))
			{
				bDebugMode = true;
				InitParams.bIsDebugMode = bDebugMode;
			}
#endif

			// Whether or not we're 3D
			bIs3D = !UseObjectBasedSpatialization() && WaveInstance->bUseSpatialization && SoundBuffer->NumChannels < 3;

			// Grab the source's reverb plugin settings 
			InitParams.SpatializationPluginSettings = UseSpatializationPlugin() ? InWaveInstance->SpatializationPluginSettings : nullptr;

			// Grab the source's occlusion plugin settings 
			InitParams.OcclusionPluginSettings = UseOcclusionPlugin() ? InWaveInstance->OcclusionPluginSettings : nullptr;

			// Grab the source's reverb plugin settings 
			InitParams.ReverbPluginSettings = UseReverbPlugin() ? InWaveInstance->ReverbPluginSettings : nullptr;

			// We support reverb
			SetReverbApplied(true);

			// Update the buffer sample rate to the wave instance sample rate in case it was serialized incorrectly
			MixerBuffer->InitSampleRate(InWaveInstance->WaveData->SampleRate);

			if (MixerSourceVoice->Init(InitParams))
			{
				AUDIO_MIXER_CHECK(WaveInstance);

				const EBufferType::Type BufferType = MixerBuffer->GetType();
				switch (BufferType)
				{
					case EBufferType::PCM:
					case EBufferType::PCMPreview:
						SubmitPCMBuffers();
						break;

					case EBufferType::PCMRealTime:
					case EBufferType::Streaming:
						SubmitPCMRTBuffers();
						break;

					case EBufferType::Invalid:
						break;
				}

				bInitialized = true;

				ChannelMap.Reset();

				Update();

				return true;
			}
		}
		return false;
	}

	void FMixerSource::Update()
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioUpdateSources);

		if (!WaveInstance || !MixerSourceVoice || Paused || !bInitialized)
		{
			return;
		}

		UpdatePitch();

		UpdateVolume();

		UpdateSpatialization();

		UpdateEffects();

		UpdateChannelMaps();

		FSoundSource::DrawDebugInfo();
	}

	bool FMixerSource::PrepareForInitialization(FWaveInstance* InWaveInstance)
	{
		// We are currently not supporting playing audio on a controller
		if (InWaveInstance->OutputTarget == EAudioOutputTarget::Controller)
		{
			return false;
		}

		// We are not initialized yet. We won't be until the sound file finishes loading and parsing the header.
		bInitialized = false;

		//  Reset so next instance will warn if algorithm changes in-flight
		bEditorWarnedChangedSpatialization = false;

		check(InWaveInstance);
		check(MixerBuffer == nullptr);
		check(AudioDevice);

		MixerBuffer = FMixerBuffer::Init(AudioDevice, InWaveInstance->WaveData, InWaveInstance->StartTime > 0.0f);
		if (MixerBuffer)
		{
			Buffer = MixerBuffer;
			WaveInstance = InWaveInstance;

			LPFFrequency = MAX_FILTER_FREQUENCY;
			LastLPFFrequency = FLT_MAX;
			bIsFinished = false;

			EBufferType::Type BufferType = MixerBuffer->GetType();
			bResourcesNeedFreeing = (BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming);

			// Not all wave data types have PCM data size at this point (e.g. procedural sound waves)
			if (InWaveInstance->WaveData->RawPCMDataSize > 0)
			{
				int32 NumBytes = InWaveInstance->WaveData->RawPCMDataSize;
				NumTotalFrames = NumBytes / (Buffer->NumChannels * sizeof(int16));
				check(NumTotalFrames > 0);
			}

			// Set up buffer areas to decompress audio into
			for (int32 BufferIndex = 0; BufferIndex < Audio::MAX_BUFFERS_QUEUED; ++BufferIndex)
			{
				const uint32 TotalSamples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
				SourceVoiceBuffers[BufferIndex]->AudioData.Reset();
				SourceVoiceBuffers[BufferIndex]->AudioData.AddZeroed(TotalSamples);
				SourceVoiceBuffers[BufferIndex]->Samples = TotalSamples;
				SourceVoiceBuffers[BufferIndex]->bRealTimeBuffer = true;
				SourceVoiceBuffers[BufferIndex]->LoopCount = 0;
			}

			// We succeeded in preparing the buffer for initialization, but we are not technically initialized yet.
			// If the buffer is asynchronously preparing a file-handle, we may not yet initialize the source.
			return true;
		}

		// Something went wrong with initializing the generator
		return false;
	}

	bool FMixerSource::IsPreparedToInit()
	{
		if (MixerBuffer && MixerBuffer->IsRealTimeSourceReady() && !bIsReleasing)
		{
			// Check if we have a realtime audio task already (doing first decode)
			if (AsyncRealtimeAudioTask)
			{
				// not ready
				return AsyncRealtimeAudioTask->IsDone();
			}
			else if (WaveInstance->WaveData->bIsBus)
			{
				// Buses don't need to do anything to play audio
				return true;
			}
			else
			{
				// Now check to see if we need to kick off a decode the first chunk of audio
				const EBufferType::Type BufferType = MixerBuffer->GetType();
				if ((BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming) && WaveInstance->WaveData)
				{
					// If any of these conditions meet, we need to do an initial async decode before we're ready to start playing the sound
					if (WaveInstance->StartTime > 0.0f || WaveInstance->WaveData->bProcedural || WaveInstance->WaveData->bIsBus || !WaveInstance->WaveData->CachedRealtimeFirstBuffer)
					{
						// Before reading more PCMRT data, we first need to seek the buffer
						if (WaveInstance->StartTime > 0.0f && !WaveInstance->WaveData->bIsBus && !WaveInstance->WaveData->bProcedural)
						{
							MixerBuffer->Seek(WaveInstance->StartTime);
						}

						ReadMorePCMRTData(0, EBufferReadMode::Asynchronous);

						// not ready
						return false;
					}
				}
			}

			return true;
		}

		return false;
	}

	void FMixerSource::Play()
	{
		if (!WaveInstance)
		{
			return;
		}

		// It's possible if Pause and Play are called while a sound is async initializing. In this case
		// we'll just not actually play the source here. Instead we'll call play when the sound finishes loading.
		if (MixerSourceVoice && bInitialized)
		{
			MixerSourceVoice->Play();
		}

		Paused = false;
		Playing = true;
		bBuffersToFlush = false;
		bLoopCallback = false;
	}

	void FMixerSource::Stop()
	{
		bInitialized = false;
		IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

		if (WaveInstance)
		{
			FScopeLock Lock(&RenderThreadCritSect);

			if (MixerSourceVoice && Playing)
			{
				MixerSourceVoice->Stop();
			}

			Paused = false;
			Playing = false;

			FreeResources();
		}

		FSoundSource::Stop();
	}

	void FMixerSource::Pause()
	{
		if (!WaveInstance)
		{
			return;
		}

		if (MixerSourceVoice)
		{
			MixerSourceVoice->Pause();
		}

		Paused = true;
	}

	bool FMixerSource::IsFinished()
	{
		// A paused source is not finished.
		if (Paused || !bInitialized)
		{
			return false;
		}

		if (WaveInstance && MixerSourceVoice)
		{
			if (bIsFinished && MixerSourceVoice->IsSourceEffectTailsDone())
			{
				WaveInstance->NotifyFinished();
				return true;
			}
			// Buses don't do buffer end callbacks, so we need to directly query buses doneness
			else if (WaveInstance->WaveData->bIsBus)
			{
				if (MixerSourceVoice->IsSourceEffectTailsDone() && MixerSourceVoice->IsDone())
				{
					bIsFinished = true;
					WaveInstance->NotifyFinished();
					return true;
				}
			}

			if (bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification)
			{
				WaveInstance->NotifyFinished();
				bLoopCallback = false;
			}

			return false;
		}
		return true;
	}

	FString FMixerSource::Describe(bool bUseLongName)
	{
		return FString(TEXT("Stub"));
	}

	float FMixerSource::GetPlaybackPercent() const
	{
		if (NumTotalFrames > 0)
		{
			int64 NumFrames = MixerSourceVoice->GetNumFramesPlayed();
			AUDIO_MIXER_CHECK(NumTotalFrames > 0);
			return (float)NumFrames / NumTotalFrames;
		}
		else
		{
			// If we don't have any frames, that means it's a procedural sound wave, which means
			// that we're never going to have a playback percentage.
			return 0.0f;
		}
	}

	void FMixerSource::SubmitPCMBuffers()
	{
		if (!AudioDevice)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("SubmitPCMBuffers: Audio device is nullptr"));
			return;
		}

		if (!MixerSourceVoice)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("SubmitPCMBuffers: Source is nullptr"));
			return;
		}

		CurrentBuffer = 0;

		RawPCMDataBuffer.Data = nullptr;
		RawPCMDataBuffer.DataSize = 0;
		MixerBuffer->GetPCMData(&RawPCMDataBuffer.Data, &RawPCMDataBuffer.DataSize);

		RawPCMDataBuffer.NumSamples = RawPCMDataBuffer.DataSize / sizeof(int16);
		RawPCMDataBuffer.CurrentSample = 0;

		// Only submit data if we've successfully loaded it
		if (!RawPCMDataBuffer.Data || !RawPCMDataBuffer.DataSize)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Failed to load PCM data from sound source %s"), *WaveInstance->GetName());
			return;
		}

		RawPCMDataBuffer.LoopCount = (WaveInstance->LoopingMode != LOOP_Never) ? Audio::LOOP_FOREVER : 0;

		// Submit the first two format-converted chunks to the source voice
		const uint32 NumSamplesPerBuffer = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
		int16* RawPCMBufferDataPtr = (int16*)RawPCMDataBuffer.Data;

		RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[0], NumSamplesPerBuffer);
		MixerSourceVoice->SubmitBuffer(SourceVoiceBuffers[0], false);

		CurrentBuffer = 1;
	}

	void FMixerSource::SubmitPCMRTBuffers()
	{
		CurrentBuffer = 0;

		bPlayedCachedBuffer = false;
		bool bIsSeeking = (WaveInstance->StartTime > 0.0f);
		if (!bIsSeeking && WaveInstance->WaveData && WaveInstance->WaveData->CachedRealtimeFirstBuffer)
		{
			bPlayedCachedBuffer = true;

			// Format convert the first cached buffers
			const uint32 NumSamples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
			const uint32 BufferSize = MONO_PCM_BUFFER_SIZE * MixerBuffer->NumChannels;

			int16* CachedBufferPtr0 = (int16*)WaveInstance->WaveData->CachedRealtimeFirstBuffer;
			int16* CachedBufferPtr1 = (int16*)(WaveInstance->WaveData->CachedRealtimeFirstBuffer + BufferSize);
			float* AudioData0 = SourceVoiceBuffers[0]->AudioData.GetData();
			float* AudioData1 = SourceVoiceBuffers[1]->AudioData.GetData();
			for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
			{
				AudioData0[Sample] = CachedBufferPtr0[Sample] / 32768.0f;
				AudioData1[Sample] = CachedBufferPtr1[Sample] / 32768.0f;
			}

			// Submit the already decoded and cached audio buffers
			MixerSourceVoice->SubmitBuffer(SourceVoiceBuffers[0], false);
			MixerSourceVoice->SubmitBuffer(SourceVoiceBuffers[1], false);

			CurrentBuffer = 2;
		}
		else if (WaveInstance->WaveData && !WaveInstance->WaveData->bIsBus)
		{
			// We should have already kicked off and finished a task. 
			check(AsyncRealtimeAudioTask != nullptr);

			ProcessRealTimeSource(true, false);
		}

		// We should have set this
		check(bResourcesNeedFreeing);
	}

	bool FMixerSource::ReadMorePCMRTData(const int32 BufferIndex, EBufferReadMode BufferReadMode, bool* OutLooped)
	{
		USoundWave* WaveData = WaveInstance->WaveData;

		if (WaveData)
		{
			if (WaveData->bProcedural)
			{
				const int32 MaxSamples = MONO_PCM_BUFFER_SAMPLES * Buffer->NumChannels;

				FProceduralAudioTaskData NewTaskData;
				NewTaskData.ProceduralSoundWave = WaveData;
				NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
				NewTaskData.NumSamples = MaxSamples;
				NewTaskData.NumChannels = Buffer->NumChannels;
				check(!AsyncRealtimeAudioTask);
				AsyncRealtimeAudioTask = CreateAudioTask(NewTaskData);

				// Procedural sound waves never loop
				return false;
			}
			else if (!MixerBuffer->IsRealTimeBuffer())
			{
				check(RawPCMDataBuffer.Data != nullptr);

				// Read the next raw PCM buffer into the source buffer index. This converts raw PCM to float.
				const uint32 NumSamplesPerBuffer = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
				return RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[BufferIndex], NumSamplesPerBuffer);
			}
			else
			{
				FDecodeAudioTaskData NewTaskData;
				NewTaskData.MixerBuffer = MixerBuffer;
				NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
				NewTaskData.bLoopingMode = WaveInstance->LoopingMode != LOOP_Never;
				NewTaskData.bSkipFirstBuffer = (BufferReadMode == EBufferReadMode::AsynchronousSkipFirstFrame);
				NewTaskData.NumFramesToDecode = MONO_PCM_BUFFER_SAMPLES;

				check(!AsyncRealtimeAudioTask);
				AsyncRealtimeAudioTask = CreateAudioTask(NewTaskData);

				// Not looping
				return false;
			}
		}
		return true;
	}

	void FMixerSource::SubmitRealTimeSourceData(const bool bLooped, const bool bSubmitSynchronously)
	{
		// Have we reached the end of the sound
		if (bLooped)
		{
			switch (WaveInstance->LoopingMode)
			{
				case LOOP_Never:
					// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
					bBuffersToFlush = true;
					break;

				case LOOP_WithNotification:
					// If we have just looped, and we are looping, send notification
					// This will trigger a WaveInstance->NotifyFinished() in the FXAudio2SoundSournce::IsFinished() function on main thread.
					bLoopCallback = true;
					break;

				case LOOP_Forever:
					// Let the sound loop indefinitely
					break;
			}
		}

		if (MixerSourceVoice && SourceVoiceBuffers[CurrentBuffer]->AudioData.Num() > 0)
		{
			MixerSourceVoice->SubmitBuffer(SourceVoiceBuffers[CurrentBuffer], bSubmitSynchronously);
		}
	}

	void FMixerSource::ProcessRealTimeSource(const bool bBlockForData, const bool bSubmitSynchronously)
	{
		const bool bGetMoreData = bBlockForData || (AsyncRealtimeAudioTask == nullptr);
		if (AsyncRealtimeAudioTask)
		{
			const bool bTaskDone = AsyncRealtimeAudioTask->IsDone();
			if (bTaskDone || bBlockForData)
			{
				bool bLooped = false;

				if (!bTaskDone)
				{
					AsyncRealtimeAudioTask->EnsureCompletion();
				}

				switch (AsyncRealtimeAudioTask->GetType())
				{
					case EAudioTaskType::Decode:
					{
						FDecodeAudioTaskResults TaskResult;
						AsyncRealtimeAudioTask->GetResult(TaskResult);

						SourceVoiceBuffers[CurrentBuffer]->Samples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
						bLooped = TaskResult.bLooped;
					}
					break;

					case EAudioTaskType::Procedural:
					{
						FProceduralAudioTaskResults TaskResult;
						AsyncRealtimeAudioTask->GetResult(TaskResult);

						SourceVoiceBuffers[CurrentBuffer]->Samples = TaskResult.NumSamplesWritten;
					}
					break;
				}

				delete AsyncRealtimeAudioTask;
				AsyncRealtimeAudioTask = nullptr;
				
				SubmitRealTimeSourceData(bLooped, bSubmitSynchronously);
			}
		}

		if (bGetMoreData)
		{
			// Update the buffer index
			if (++CurrentBuffer > 2)
			{
				CurrentBuffer = 0;
			}

			EBufferReadMode DataReadMode;
			if (bPlayedCachedBuffer)
			{
				bPlayedCachedBuffer = false;
				DataReadMode = EBufferReadMode::AsynchronousSkipFirstFrame;
			}
			else
			{
				DataReadMode = EBufferReadMode::Asynchronous;
			}
			const bool bLooped = ReadMorePCMRTData(CurrentBuffer, DataReadMode);

			// If this was a synchronous read, then immediately write it
			if (AsyncRealtimeAudioTask == nullptr)
			{
				SubmitRealTimeSourceData(bLooped, bSubmitSynchronously);
			}
		}
	}

	void FMixerSource::OnSourceBufferEnd()
	{
		FScopeLock Lock(&RenderThreadCritSect);

		if (Playing && MixerSourceVoice)
		{
			int32 BuffersQueued = MixerSourceVoice->GetNumBuffersQueued();

			if (BuffersQueued == 0 && bBuffersToFlush)
			{
				bIsFinished = true;
			}
			else if (!bBuffersToFlush && BuffersQueued <= (Audio::MAX_BUFFERS_QUEUED - 1))
			{
				// OnSourceBufferEnd is always called from render thread and the source needs to be 
				// processed and any decoded buffers submitted to render thread synchronously			
				const bool bSubmitSynchronously = true;
				ProcessRealTimeSource(BuffersQueued < (Audio::MAX_BUFFERS_QUEUED - 1), bSubmitSynchronously);
			}
		}
	}

	void FMixerSource::OnRelease(TArray<FPendingReleaseData*>& OutPendingReleaseData)
	{
		FPendingReleaseData* PendingReleaseData = nullptr;

		while (PendingReleases.Dequeue(PendingReleaseData))
		{
			OutPendingReleaseData.Add(PendingReleaseData);
		}

		bIsReleasing = false;
	}

	void FMixerSource::FreeResources()
	{
		if (MixerBuffer)
		{
			MixerBuffer->EnsureHeaderParseTaskFinished();
		}

		FPendingReleaseData* PendingDecodeTask = nullptr;

		if (MixerSourceVoice)
		{
			// Hand off the ptr of the async task so it can be shutdown on the audio render thread.
			if (AsyncRealtimeAudioTask)
			{
				PendingDecodeTask = new FPendingReleaseData();
				PendingDecodeTask->Task = AsyncRealtimeAudioTask;
			}

			// We're now "releasing" so don't recycle this voice until we get notified that the source has finished
			bIsReleasing = true;

			// This will trigger FMixerSource::OnRelease from audio render thread.
			MixerSourceVoice->Release();
			MixerSourceVoice = nullptr;
		}

		if (bResourcesNeedFreeing)
		{
			// If we have a buffer, we can't delete until the async decoding task has been ensured to complete.
			if (Buffer)
			{
				check(Buffer->ResourceID == 0);

				if (!PendingDecodeTask)
				{
					PendingDecodeTask = new FPendingReleaseData();
				}

				PendingDecodeTask->Buffer = Buffer;
			}

			CurrentBuffer = 0;
		}

		if (PendingDecodeTask)
		{
			PendingReleases.Enqueue(PendingDecodeTask);
		}

		// Reset the raw PCM buffer data
		RawPCMDataBuffer = FRawPCMDataBuffer();

		MixerBuffer = nullptr;
		AsyncRealtimeAudioTask = nullptr;
		Buffer = nullptr;
		bBuffersToFlush = false;
		bLoopCallback = false;
		bResourcesNeedFreeing = false;
	}

	void FMixerSource::UpdatePitch()
	{
		AUDIO_MIXER_CHECK(MixerBuffer);

		check(WaveInstance);

		Pitch = WaveInstance->Pitch;

		// Don't apply global pitch scale to UI sounds
		if (!WaveInstance->bIsUISound)
		{
			Pitch *= AudioDevice->GetGlobalPitchScale().GetValue();
		}

		Pitch = FMath::Clamp<float>(Pitch, AUDIO_MIXER_MIN_PITCH, AUDIO_MIXER_MAX_PITCH);

		// Scale the pitch by the ratio of the audio buffer sample rate and the actual sample rate of the hardware
		if (MixerBuffer)
		{
			const float MixerBufferSampleRate = MixerBuffer->GetSampleRate();
			const float AudioDeviceSampleRate = AudioDevice->GetSampleRate();
			Pitch *= MixerBufferSampleRate / AudioDeviceSampleRate;

			MixerSourceVoice->SetPitch(Pitch);
		}
	}

	void FMixerSource::UpdateVolume()
	{
		float CurrentVolume;
		if (AudioDevice->IsAudioDeviceMuted())
		{
			CurrentVolume = 0.0f;
		}
		else
		{
			CurrentVolume = WaveInstance->GetVolume();
			CurrentVolume *= WaveInstance->GetVolumeApp();
			CurrentVolume *= AudioDevice->GetPlatformAudioHeadroom();
			CurrentVolume = FMath::Clamp<float>(GetDebugVolume(CurrentVolume), 0.0f, MAX_VOLUME);
		}

		MixerSourceVoice->SetVolume(CurrentVolume);
		MixerSourceVoice->SetDistanceAttenuation(WaveInstance->GetDistanceAttenuation());
	}

	void FMixerSource::UpdateSpatialization()
	{
		SpatializationParams = GetSpatializationParams();
		if (WaveInstance->bUseSpatialization)
		{
			MixerSourceVoice->SetSpatializationParams(SpatializationParams);
		}
	}

	void FMixerSource::UpdateEffects()
	{
		// Update the default LPF filter frequency 
		SetFilterFrequency();

		if (LastLPFFrequency != LPFFrequency)
		{
			MixerSourceVoice->SetLPFFrequency(LPFFrequency);
			LastLPFFrequency = LPFFrequency;
		}

		if (LastHPFFrequency != HPFFrequency)
		{
			MixerSourceVoice->SetHPFFrequency(HPFFrequency);
			LastHPFFrequency = HPFFrequency;
		}

		// If reverb is applied, figure out how of the source to "send" to the reverb.
		if (bReverbApplied)
		{
			float ReverbSendLevel = 0.0f;

			if (WaveInstance->ReverbSendMethod == EReverbSendMethod::Manual)
			{
				ReverbSendLevel = FMath::Clamp(WaveInstance->ManualReverbSendLevel, 0.0f, 1.0f);
			}
			else
			{
				// The alpha value is determined identically between manual and custom curve methods
				const FVector2D& ReverbSendRadialRange = WaveInstance->ReverbSendLevelDistanceRange;
				const float Denom = FMath::Max(ReverbSendRadialRange.Y - ReverbSendRadialRange.X, 1.0f);
				const float Alpha = FMath::Clamp((WaveInstance->ListenerToSoundDistance - ReverbSendRadialRange.X) / Denom, 0.0f, 1.0f);

				if (WaveInstance->ReverbSendMethod == EReverbSendMethod::Linear)
				{
					ReverbSendLevel = FMath::Clamp(FMath::Lerp(WaveInstance->ReverbSendLevelRange.X, WaveInstance->ReverbSendLevelRange.Y, Alpha), 0.0f, 1.0f);
				}
				else
				{
					ReverbSendLevel = FMath::Clamp(WaveInstance->CustomRevebSendCurve.GetRichCurveConst()->Eval(Alpha), 0.0f, 1.0f);
				}
			}

			// Send the source audio to the reverb plugin if enabled
			if (UseReverbPlugin())
			{
				FMixerSubmixPtr MasterReverbPluginSubmix = MixerDevice->GetMasterReverbPluginSubmix();
				MixerSourceVoice->SetSubmixSendInfo(MasterReverbPluginSubmix, ReverbSendLevel);
			}
			else 
			{
				// Send the source audio to the master reverb
				FMixerSubmixPtr MasterReverbSubmix = MixerDevice->GetMasterReverbSubmix();
				MixerSourceVoice->SetSubmixSendInfo(MasterReverbSubmix, ReverbSendLevel);
			}
		}

		for (FSoundSubmixSendInfo& SendInfo : WaveInstance->SoundSubmixSends)
		{
			if (SendInfo.SoundSubmix)
			{
				FMixerSubmixPtr SubmixInstance = MixerDevice->GetSubmixInstance(SendInfo.SoundSubmix);
				MixerSourceVoice->SetSubmixSendInfo(SubmixInstance, SendInfo.SendLevel);
			}
		}
	}

	void FMixerSource::UpdateChannelMaps()
	{
		SetStereoBleed();

		SetLFEBleed();

		bool bChanged = false;

		check(Buffer);
		bChanged = ComputeChannelMap(Buffer->NumChannels);

		if (bChanged)
		{
			MixerSourceVoice->SetChannelMap(ChannelMap, bIs3D, WaveInstance->bCenterChannelOnly);
		}
	}

	bool FMixerSource::ComputeMonoChannelMap()
	{
		if (UseObjectBasedSpatialization())
		{
			if (WaveInstance->SpatializationMethod != ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && !bEditorWarnedChangedSpatialization)
			{
				bEditorWarnedChangedSpatialization = true;
				UE_LOG(LogAudioMixer, Warning, TEXT("Changing the spatialization method on a playing sound is not supported (WaveInstance: %s)"), *WaveInstance->WaveData->GetFullName());
			}

			// Treat the source as if it is a 2D stereo source
			return ComputeStereoChannelMap();
		}
		else if (WaveInstance->bUseSpatialization && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Don't need to compute the source channel map if the absolute azimuth hasn't changed much
			PreviousAzimuth = WaveInstance->AbsoluteAzimuth;
			ChannelMap.Reset();
			MixerDevice->Get3DChannelMap(WaveInstance, WaveInstance->AbsoluteAzimuth, SpatializationParams.NormalizedOmniRadius, ChannelMap);
			return true;
		}
		else if (!ChannelMap.Num())
		{
			// Only need to compute the 2D channel map once
			MixerDevice->Get2DChannelMap(1, MixerDevice->GetNumDeviceChannels(), WaveInstance->bCenterChannelOnly, ChannelMap);
			return true;
		}

		// Return false means the channel map hasn't changed
		return false;
	}

	bool FMixerSource::ComputeStereoChannelMap()
	{
		if (!UseObjectBasedSpatialization() && WaveInstance->bUseSpatialization && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Make sure our stereo emitter positions are updated relative to the sound emitter position
			UpdateStereoEmitterPositions();

			float AzimuthOffset = 0.0f;
			if (WaveInstance->ListenerToSoundDistance > 0.0f)
			{
				AzimuthOffset = FMath::Atan(0.5f * WaveInstance->StereoSpread / WaveInstance->ListenerToSoundDistance);
				AzimuthOffset = FMath::RadiansToDegrees(AzimuthOffset);
			}

			float LeftAzimuth = WaveInstance->AbsoluteAzimuth - AzimuthOffset;
			if (LeftAzimuth < 0.0f)
			{
				LeftAzimuth += 360.0f;
			}

			float RightAzimuth = WaveInstance->AbsoluteAzimuth + AzimuthOffset;
			if (RightAzimuth > 360.0f)
			{
				RightAzimuth -= 360.0f;
			}

			// Reset the channel map, the stereo spatialization channel mapping calls below will append their mappings
			ChannelMap.Reset();

			MixerDevice->Get3DChannelMap(WaveInstance, LeftAzimuth, SpatializationParams.NormalizedOmniRadius, ChannelMap);
			MixerDevice->Get3DChannelMap(WaveInstance, RightAzimuth, SpatializationParams.NormalizedOmniRadius, ChannelMap);

			int32 NumDeviceChannels = MixerDevice->GetNumDeviceChannels();
			check(ChannelMap.Num() == 2 * NumDeviceChannels);
			return true;
		}
		else if (!ChannelMap.Num())
		{
			MixerDevice->Get2DChannelMap(2, MixerDevice->GetNumDeviceChannels(), WaveInstance->bCenterChannelOnly, ChannelMap);
			return true;
		}

		return false;
	}

	bool FMixerSource::ComputeChannelMap(const int32 NumChannels)
	{
		if (NumChannels == 1)
		{
			return ComputeMonoChannelMap();
		}
		else if (NumChannels == 2)
		{
			return ComputeStereoChannelMap();
		}
		else if (!ChannelMap.Num())
		{
			MixerDevice->Get2DChannelMap(NumChannels, MixerDevice->GetNumDeviceChannels(), WaveInstance->bCenterChannelOnly, ChannelMap);
			return true;
		}
		return false;
	}

	bool FMixerSource::UseObjectBasedSpatialization() const
	{
		return (Buffer->NumChannels == 1 &&
				AudioDevice->IsSpatializationPluginEnabled() &&
				DisableHRTFCvar == 0 &&
				WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF);
	}

	bool FMixerSource::UseSpatializationPlugin() const
	{
		return (Buffer->NumChannels == 1) &&
			AudioDevice->IsSpatializationPluginEnabled() &&
			WaveInstance->SpatializationPluginSettings != nullptr;
	}

	bool FMixerSource::UseOcclusionPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
				AudioDevice->IsOcclusionPluginEnabled() &&
				WaveInstance->OcclusionPluginSettings != nullptr;
	}

	bool FMixerSource::UseReverbPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
				AudioDevice->IsReverbPluginEnabled() &&
				WaveInstance->ReverbPluginSettings != nullptr;
	}

	bool FRawPCMDataBuffer::GetNextBuffer(FMixerSourceBufferPtr OutSourceBufferPtr, const uint32 NumSampleToGet)
	{
		// TODO: support loop counts
		float* OutBufferPtr = OutSourceBufferPtr->AudioData.GetData();
		int16* DataPtr = (int16*)Data;

		if (LoopCount == Audio::LOOP_FOREVER)
		{
			bool bLooped = false;
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = DataPtr[CurrentSample++] / 32768.0f;

				// Loop around if we're looping
				if (CurrentSample >= NumSamples)
				{
					CurrentSample = 0;
					bLooped = true;
				}
			}
			return bLooped;
		}
		else if (CurrentSample < NumSamples)
		{
			uint32 Sample = 0;
			while (Sample < NumSampleToGet && CurrentSample < NumSamples)
			{
				OutBufferPtr[Sample++] = (float)DataPtr[CurrentSample++] / 32768.0f;
			}

			// Zero out the rest of the buffer
			while (Sample < NumSampleToGet)
			{
				OutBufferPtr[Sample++] = 0.0f;
			}
		}
		else
		{
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = 0.0f;
			}
		}

		// If the current sample is greater or equal to num samples we hit the end of the buffer
		return CurrentSample >= NumSamples;
	}

}