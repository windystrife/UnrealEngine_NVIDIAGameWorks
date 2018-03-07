// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerDevice.h"
#include "AudioMixerSource.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerSourceVoice.h"
#include "UObject/UObjectHash.h"
#include "AudioMixerEffectsManager.h"
#include "SubmixEffects/AudioMixerSubmixEffectReverb.h"
#include "SubmixEffects/AudioMixerSubmixEffectEQ.h"
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "DSP/Noise.h"
#include "DSP/SinOsc.h"
#include "UObject/UObjectIterator.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
#include "Misc/App.h"

#if WITH_EDITOR
#include "AudioEditorModule.h"
#endif

namespace Audio
{
	TArray<USoundSubmix*> FMixerDevice::MasterSubmixes;

	FMixerDevice::FMixerDevice(IAudioMixerPlatformInterface* InAudioMixerPlatform)
		: AudioMixerPlatform(InAudioMixerPlatform)
		, NumSpatialChannels(0)
		, OmniPanFactor(0.0f)
		, AudioClockDelta(0.0)
		, AudioClock(0.0)
		, SourceManager(this)
		, GameOrAudioThreadId(INDEX_NONE)
		, AudioPlatformThreadId(INDEX_NONE)
		, bDebugOutputEnabled(false)
	{
		// This audio device is the audio mixer
		bAudioMixerModuleLoaded = true;
	}

	FMixerDevice::~FMixerDevice()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(this);

		if (AudioMixerPlatform != nullptr)
		{
			delete AudioMixerPlatform;
		}
	}

	void FMixerDevice::CheckAudioThread()
	{
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		// "Audio Thread" is the game/audio thread ID used above audio rendering thread.
		AUDIO_MIXER_CHECK(IsInAudioThread());
#endif
	}

	void FMixerDevice::ResetAudioRenderingThreadId()
	{
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		AudioPlatformThreadId = INDEX_NONE;
		CheckAudioRenderingThread();
#endif
	}

	void FMixerDevice::CheckAudioRenderingThread()
	{
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		if (AudioPlatformThreadId == INDEX_NONE)
		{
			AudioPlatformThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		AUDIO_MIXER_CHECK(CurrentThreadId == AudioPlatformThreadId);
#endif
	}

	bool FMixerDevice::IsAudioRenderingThread()
	{
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		return CurrentThreadId == AudioPlatformThreadId;
	}

	void FMixerDevice::GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const
	{
		if (AudioMixerPlatform && AudioMixerPlatform->IsInitialized())
		{
			uint32 NumOutputDevices;
			if (AudioMixerPlatform->GetNumOutputDevices(NumOutputDevices))
			{
				for (uint32 i = 0; i < NumOutputDevices; ++i)
				{
					FAudioPlatformDeviceInfo DeviceInfo;
					if (AudioMixerPlatform->GetOutputDeviceInfo(i, DeviceInfo))
					{
						OutAudioDeviceNames.Add(DeviceInfo.Name);
					}
				}
			}
		}
	}

	bool FMixerDevice::InitializeHardware()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(this);
	
		// Log that we're inside the audio mixer
		UE_LOG(LogAudioMixer, Display, TEXT("Initializing audio mixer."));

		if (AudioMixerPlatform && AudioMixerPlatform->InitializeHardware())
		{
			// Set whether we're the main audio mixer
			bIsMainAudioMixer = IsMainAudioDevice();

			AUDIO_MIXER_CHECK(SampleRate != 0.0f);

			AudioMixerPlatform->RegisterDeviceChangedListener();

			// Allow platforms to override the platform settings callback buffer frame size (i.e. restrict to particular values, etc)
			PlatformSettings.CallbackBufferFrameSize = AudioMixerPlatform->GetNumFrames(PlatformSettings.CallbackBufferFrameSize);

			OpenStreamParams.NumBuffers = PlatformSettings.NumBuffers;
			OpenStreamParams.NumFrames = PlatformSettings.CallbackBufferFrameSize;
			OpenStreamParams.OutputDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX; // TODO: Support overriding which audio device user wants to open, not necessarily default.
			OpenStreamParams.SampleRate = SampleRate;
			OpenStreamParams.AudioMixer = this;

			FString DefaultDeviceName = AudioMixerPlatform->GetDefaultDeviceName();

			// Allow HMD to specify audio device, if one was not specified in settings
			if (DefaultDeviceName.IsEmpty() && FAudioDevice::CanUseVRAudioDevice() && IHeadMountedDisplayModule::IsAvailable())
			{
				DefaultDeviceName = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();
			}

			if (!DefaultDeviceName.IsEmpty())
			{
				uint32 NumOutputDevices = 0;
				AudioMixerPlatform->GetNumOutputDevices(NumOutputDevices);

				for (uint32 i = 0; i < NumOutputDevices; ++i)
				{
					FAudioPlatformDeviceInfo DeviceInfo;
					AudioMixerPlatform->GetOutputDeviceInfo(i, DeviceInfo);

					if (DeviceInfo.Name == DefaultDeviceName || DeviceInfo.DeviceId == DefaultDeviceName)
					{
						OpenStreamParams.OutputDeviceIndex = i;

						// If we're intentionally selecting an audio device (and not just using the default device) then 
						// lets try to restore audio to that device if it's removed and then later is restored
						OpenStreamParams.bRestoreIfRemoved = true;
						break;
					}
				}
			}

			if (AudioMixerPlatform->OpenAudioStream(OpenStreamParams))
			{
				// Get the platform device info we're using
				PlatformInfo = AudioMixerPlatform->GetPlatformDeviceInfo();

				// Initialize some data that depends on speaker configuration, etc.
				InitializeChannelAzimuthMap(PlatformInfo.NumChannels);

				FSourceManagerInitParams SourceManagerInitParams;
				SourceManagerInitParams.NumSources = MaxChannels;
				SourceManagerInitParams.NumSourceWorkers = 4;

				SourceManager.Init(SourceManagerInitParams);

				AudioClock = 0.0;
				AudioClockDelta = (double)OpenStreamParams.NumFrames / OpenStreamParams.SampleRate;

				FAudioPluginInitializationParams PluginInitializationParams;
				PluginInitializationParams.NumSources = MaxChannels;
				PluginInitializationParams.SampleRate = SampleRate;
				PluginInitializationParams.BufferLength = OpenStreamParams.NumFrames;
				PluginInitializationParams.AudioDevicePtr = this;

				// Initialize any plugins if they exist
				if (SpatializationPluginInterface.IsValid())
				{
					SpatializationPluginInterface->Initialize(PluginInitializationParams);
				}

				if (OcclusionInterface.IsValid())
				{
					OcclusionInterface->Initialize(PluginInitializationParams);
				}

				if (ReverbPluginInterface.IsValid())
				{
					ReverbPluginInterface->Initialize(PluginInitializationParams);
				}

				// Need to set these up before we start the audio stream.
				InitSoundSubmixes();

				AudioMixerPlatform->PostInitializeHardware();

				// Start streaming audio
				return AudioMixerPlatform->StartAudioStream();
			}
		}
		return false;
	}

	void FMixerDevice::FadeIn()
	{
		AudioMixerPlatform->FadeIn();
	}

	void FMixerDevice::FadeOut()
	{
		// In editor builds, we aren't going to fade out the main audio device.
#if WITH_EDITOR
		if (!IsMainAudioDevice())
#endif
		{
			AudioMixerPlatform->FadeOut();
		}
	}

	void FMixerDevice::TeardownHardware()
	{
		if (AudioMixerPlatform)
		{
			SourceManager.Update();

			AudioMixerPlatform->UnregisterDeviceChangedListener();
			AudioMixerPlatform->StopAudioStream();
			AudioMixerPlatform->CloseAudioStream();
			AudioMixerPlatform->TeardownHardware();
		}
	}

	void FMixerDevice::UpdateHardware()
	{
		SourceManager.Update();

		if (AudioMixerPlatform->CheckAudioDeviceChange())
		{
			// Get the platform device info we're using
			PlatformInfo = AudioMixerPlatform->GetPlatformDeviceInfo();

			// Initialize some data that depends on speaker configuration, etc.
			InitializeChannelAzimuthMap(PlatformInfo.NumChannels);

			SourceManager.UpdateDeviceChannelCount(PlatformInfo.NumChannels);

			// Audio rendering was suspended in CheckAudioDeviceChange if it changed.
			AudioMixerPlatform->ResumePlaybackOnNewDevice();
		}
	}

	double FMixerDevice::GetAudioTime() const
	{
		return AudioClock;
	}

	FAudioEffectsManager* FMixerDevice::CreateEffectsManager()
	{
		return new FAudioMixerEffectsManager(this);
	}

	FSoundSource* FMixerDevice::CreateSoundSource()
	{
		return new FMixerSource(this);
	}

	FName FMixerDevice::GetRuntimeFormat(USoundWave* InSoundWave)
	{
		check(AudioMixerPlatform);
		return AudioMixerPlatform->GetRuntimeFormat(InSoundWave);
	}

	bool FMixerDevice::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		check(InSoundWave);
		check(AudioMixerPlatform);
		return AudioMixerPlatform->HasCompressedAudioInfoClass(InSoundWave);
	}

	bool FMixerDevice::SupportsRealtimeDecompression() const
	{
		return AudioMixerPlatform->SupportsRealtimeDecompression();
	}

	class ICompressedAudioInfo* FMixerDevice::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
		check(InSoundWave);
		check(AudioMixerPlatform);
		return AudioMixerPlatform->CreateCompressedAudioInfo(InSoundWave);
	}

	bool FMixerDevice::ValidateAPICall(const TCHAR* Function, uint32 ErrorCode)
	{
		return false;
	}

	bool FMixerDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FAudioDevice::Exec(InWorld, Cmd, Ar))
		{
			return true;
		}

		return false;
	}

	void FMixerDevice::CountBytes(FArchive& InArchive)
	{
		FAudioDevice::CountBytes(InArchive);
	}

	bool FMixerDevice::IsExernalBackgroundSoundActive()
	{
		return false;
	}

	void FMixerDevice::ResumeContext()
	{
        AudioMixerPlatform->ResumeContext();
	}

	void FMixerDevice::SuspendContext()
	{
        AudioMixerPlatform->SuspendContext();
	}

	void FMixerDevice::EnableDebugAudioOutput()
	{
		bDebugOutputEnabled = true;
	}

	bool FMixerDevice::OnProcessAudioStream(AlignedFloatBuffer& Output)
	{
#if WITH_EDITOR
		// Turn on to only hear PIE audio
		bool bBypassMainAudioDevice = FParse::Param(FCommandLine::Get(), TEXT("AudioPIEOnly"));
		if (bBypassMainAudioDevice && IsMainAudioDevice())
		{
			return true;
		}
#endif
		// This function could be called in a task manager, which means the thread ID may change between calls.
		ResetAudioRenderingThreadId();

		// Pump the command queue to the audio render thread
		PumpCommandQueue();

		// Compute the next block of audio in the source manager
		SourceManager.ComputeNextBlockOfSamples();

		FMixerSubmixPtr MasterSubmix = GetMasterSubmix();

		{
			SCOPE_CYCLE_COUNTER(STAT_AudioMixerSubmixes);

			// Process the audio output from the master submix
			MasterSubmix->ProcessAudio(Output);
		}

		// Do any debug output performing
		if (bDebugOutputEnabled)
		{
			SineOscTest(Output);
		}

		// Update the audio clock
		AudioClock += AudioClockDelta;

		return true;
	}

	void FMixerDevice::OnAudioStreamShutdown()
	{
		// Make sure the source manager pumps any final commands on shutdown. These allow for cleaning up sources, interfacing with plugins, etc.
		// Because we double buffer our command queues, we call this function twice to ensure all commands are successfully pumped.
		SourceManager.PumpCommandQueue();
		SourceManager.PumpCommandQueue();

		// Make sure we force any pending release data to happen on shutdown
		SourceManager.UpdatePendingReleaseData(true);
	}

	void FMixerDevice::InitSoundSubmixes()
	{
		if (!IsInAudioThread())
		{
			FAudioThread::RunCommandOnAudioThread([this]()
			{
				InitSoundSubmixes();
			});
			return;
		}

		// Create the master, master reverb, and master eq sound submixes
		if (!FMixerDevice::MasterSubmixes.Num())
		{
			// Master
			USoundSubmix* MasterSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), TEXT("Master Submix"));
			MasterSubmix->AddToRoot();
			FMixerDevice::MasterSubmixes.Add(MasterSubmix);

			// Master Reverb Plugin
			USoundSubmix* ReverbPluginSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), TEXT("Master Reverb Plugin Submix"));
			ReverbPluginSubmix->AddToRoot();
			FMixerDevice::MasterSubmixes.Add(ReverbPluginSubmix);

			// Master Reverb
			USoundSubmix* ReverbSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), TEXT("Master Reverb Submix"));
			ReverbSubmix->AddToRoot();
			FMixerDevice::MasterSubmixes.Add(ReverbSubmix);

			// Master EQ
			USoundSubmix* EQSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), TEXT("Master EQ Submix"));
			EQSubmix->AddToRoot();
			FMixerDevice::MasterSubmixes.Add(EQSubmix);
		}

		// Register and setup the master submixes so that the rest of the submixes can hook into these core master submixes

		if (MasterSubmixInstances.Num() == 0)
		{
			for (int32 i = 0; i < EMasterSubmixType::Count; ++i)
			{
				MasterSubmixInstances.Add(FMixerSubmixPtr(new FMixerSubmix(this)));
			}

			FMixerSubmixPtr MasterSubmixInstance = MasterSubmixInstances[EMasterSubmixType::Master];
			FSoundEffectSubmixInitData InitData;
			InitData.SampleRate = GetSampleRate();

			// Setup the master reverb plugin
			if (ReverbPluginInterface.IsValid())
			{
				auto ReverbPluginEffectSubmix = ReverbPluginInterface->GetEffectSubmix(FMixerDevice::MasterSubmixes[EMasterSubmixType::ReverbPlugin]);

				ReverbPluginEffectSubmix->Init(InitData);
				ReverbPluginEffectSubmix->SetEnabled(true);

				const uint32 ReverbPluginId = FMixerDevice::MasterSubmixes[EMasterSubmixType::ReverbPlugin]->GetUniqueID();

				FMixerSubmixPtr MasterReverbPluginSubmix = MasterSubmixInstances[EMasterSubmixType::ReverbPlugin];
				MasterReverbPluginSubmix->AddSoundEffectSubmix(ReverbPluginId, MakeShareable(ReverbPluginEffectSubmix));
				MasterReverbPluginSubmix->SetParentSubmix(MasterSubmixInstance);
				MasterSubmixInstance->AddChildSubmix(MasterReverbPluginSubmix);
			}
			else
			{
				// Setup the master reverb only if we don't have a reverb plugin

				USoundSubmix* MasterReverbSubix = FMixerDevice::MasterSubmixes[EMasterSubmixType::Reverb];
				USubmixEffectReverbPreset* ReverbPreset = NewObject<USubmixEffectReverbPreset>(MasterReverbSubix, TEXT("Master Reverb Effect Preset"));

				FSoundEffectSubmix* ReverbEffectSubmix = static_cast<FSoundEffectSubmix*>(ReverbPreset->CreateNewEffect());

				ReverbEffectSubmix->Init(InitData);
				ReverbEffectSubmix->SetPreset(ReverbPreset);
				ReverbEffectSubmix->SetEnabled(true);

				const uint32 ReverbPresetId = ReverbPreset->GetUniqueID();

				FMixerSubmixPtr MasterReverbSubmix = MasterSubmixInstances[EMasterSubmixType::Reverb];
				MasterReverbSubmix->AddSoundEffectSubmix(ReverbPresetId, MakeShareable(ReverbEffectSubmix));
				MasterReverbSubmix->SetParentSubmix(MasterSubmixInstance);
				MasterSubmixInstance->AddChildSubmix(MasterReverbSubmix);
			}

			// Setup the master EQ
			USoundSubmix* MasterEQSoundSubmix = FMixerDevice::MasterSubmixes[EMasterSubmixType::EQ];
			USubmixEffectSubmixEQPreset* EQPreset = NewObject<USubmixEffectSubmixEQPreset>(MasterEQSoundSubmix, TEXT("Master EQ Effect preset"));

			FSoundEffectSubmix* EQEffectSubmix = static_cast<FSoundEffectSubmix*>(EQPreset->CreateNewEffect());
			EQEffectSubmix->Init(InitData);
			EQEffectSubmix->SetPreset(EQPreset);
			EQEffectSubmix->SetEnabled(true);

			const uint32 EQPresetId = EQPreset->GetUniqueID();

			FMixerSubmixPtr MasterEQSubmix = MasterSubmixInstances[EMasterSubmixType::EQ];
			MasterEQSubmix->AddSoundEffectSubmix(EQPresetId, MakeShareable(EQEffectSubmix));
			MasterEQSubmix->SetParentSubmix(MasterSubmixInstance);
			MasterSubmixInstance->AddChildSubmix(MasterEQSubmix);
		}

		// Now register all the non-core submixes

		// Reset existing submixes if they exist
		Submixes.Reset();

		// Make sure all submixes are registered but not initialized
		for (TObjectIterator<USoundSubmix> It; It; ++It)
		{
			RegisterSoundSubmix(*It, false);
		}

		// Now setup the graph for all the submixes
		for (auto& Entry : Submixes)
		{
			USoundSubmix* SoundSubmix = Entry.Key;
			FMixerSubmixPtr& SubmixInstance = Entry.Value;

			// Setup up the submix instance's parent and add the submix instance as a child
			FMixerSubmixPtr ParentSubmixInstance;
			if (SoundSubmix->ParentSubmix)
			{
				ParentSubmixInstance = GetSubmixInstance(SoundSubmix->ParentSubmix);
			}
			else
			{
				ParentSubmixInstance = GetMasterSubmix();
			}
			ParentSubmixInstance->AddChildSubmix(SubmixInstance);
			SubmixInstance->ParentSubmix = ParentSubmixInstance;

			// Now add all the child submixes to this submix instance
			for (USoundSubmix* ChildSubmix : SoundSubmix->ChildSubmixes)
			{
				// ChildSubmix lists can contain null entries.
				if (ChildSubmix)
				{
					FMixerSubmixPtr ChildSubmixInstance = GetSubmixInstance(ChildSubmix);
					SubmixInstance->ChildSubmixes.Add(ChildSubmixInstance->GetId(), ChildSubmixInstance);
				}
			}

			// Perform any other initialization on the submix instance
			SubmixInstance->Init(SoundSubmix);
		}
	}
	
 	FAudioPlatformSettings FMixerDevice::GetPlatformSettings() const
 	{
		FAudioPlatformSettings Settings = AudioMixerPlatform->GetPlatformSettings();

		UE_LOG(LogAudioMixer, Display, TEXT("Audio Mixer Platform Settings:"));
		UE_LOG(LogAudioMixer, Display, TEXT("	Sample Rate:						  %d"), Settings.SampleRate);
		UE_LOG(LogAudioMixer, Display, TEXT("	Callback Buffer Frame Size Requested: %d"), Settings.CallbackBufferFrameSize);
		UE_LOG(LogAudioMixer, Display, TEXT("	Callback Buffer Frame Size To Use:	  %d"), AudioMixerPlatform->GetNumFrames(PlatformSettings.CallbackBufferFrameSize));
		UE_LOG(LogAudioMixer, Display, TEXT("	Number of buffers to queue:			  %d"), Settings.NumBuffers);
		UE_LOG(LogAudioMixer, Display, TEXT("	Max Channels (voices):				  %d"), Settings.MaxChannels);
		UE_LOG(LogAudioMixer, Display, TEXT("	Number of Async Source Workers:		  %d"), Settings.NumSourceWorkers);

 		return Settings;
 	}

	FMixerSubmixPtr FMixerDevice::GetMasterSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::Master];
	}

	FMixerSubmixPtr FMixerDevice::GetMasterReverbPluginSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::ReverbPlugin];
	}

	FMixerSubmixPtr FMixerDevice::GetMasterReverbSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::Reverb];
	}

	FMixerSubmixPtr FMixerDevice::GetMasterEQSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::EQ];
	}

	void FMixerDevice::AddMasterSubmixEffect(uint32 SubmixEffectId, FSoundEffectSubmix* SoundEffectSubmix)
	{
		AudioRenderThreadCommand([this, SubmixEffectId, SoundEffectSubmix]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->AddSoundEffectSubmix(SubmixEffectId, MakeShareable(SoundEffectSubmix));
		});
	}

	void FMixerDevice::RemoveMasterSubmixEffect(uint32 SubmixEffectId)
	{
		AudioRenderThreadCommand([this, SubmixEffectId]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->RemoveSoundEffectSubmix(SubmixEffectId);
		});
	}

	void FMixerDevice::ClearMasterSubmixEffects()
	{
		AudioRenderThreadCommand([this]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->ClearSoundEffectSubmixes();
		});
	}

	void FMixerDevice::UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails)
	{
		TArray<FSourceEffectChainEntry>* ExistingOverride = SourceEffectChainOverrides.Find(SourceEffectChainId);
		if (ExistingOverride)
		{
			*ExistingOverride = SourceEffectChain;
		}
		else
		{
			SourceEffectChainOverrides.Add(SourceEffectChainId, SourceEffectChain);
		}

		SourceManager.UpdateSourceEffectChain(SourceEffectChainId, SourceEffectChain, bPlayEffectChainTails);
	}

	bool FMixerDevice::GetCurrentSourceEffectChain(const uint32 SourceEffectChainId, TArray<FSourceEffectChainEntry>& OutCurrentSourceEffectChainEntries)
	{
		TArray<FSourceEffectChainEntry>* ExistingOverride = SourceEffectChainOverrides.Find(SourceEffectChainId);
		if (ExistingOverride)
		{
			OutCurrentSourceEffectChainEntries = *ExistingOverride;
			return true;
		}
		return false;
	}

	void FMixerDevice::AudioRenderThreadCommand(TFunction<void()> Command)
	{
		CommandQueue.Enqueue(MoveTemp(Command));
	}

	void FMixerDevice::PumpCommandQueue()
	{
		// Execute the pushed lambda functions
		TFunction<void()> Command;
		while (CommandQueue.Dequeue(Command))
		{
			Command();
		}
	}


	bool FMixerDevice::IsMasterSubmixType(USoundSubmix* InSubmix) const
	{
		for (int32 i = 0; i < EMasterSubmixType::Count; ++i)
		{
			if (InSubmix == FMixerDevice::MasterSubmixes[i])
			{
				return true;
			}
		}
		return false;
	}

	void FMixerDevice::RegisterSoundSubmix(USoundSubmix* InSoundSubmix, bool bInit)
	{
		if (InSoundSubmix)
		{
			if (!IsInAudioThread())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RegisterSoundSubmix"), STAT_AudioRegisterSoundSubmix, STATGROUP_AudioThreadCommands);

				FMixerDevice* MixerDevice = this;
				FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix]()
				{
					MixerDevice->RegisterSoundSubmix(InSoundSubmix);
				}, GET_STATID(STAT_AudioRegisterSoundSubmix));
				return;
			}

			if (!IsMasterSubmixType(InSoundSubmix))
			{
				// If the sound submix wasn't already registered get it into the system.
				if (!Submixes.Contains(InSoundSubmix))
				{
					FMixerSubmixPtr MixerSubmix = FMixerSubmixPtr(new FMixerSubmix(this));
					Submixes.Add(InSoundSubmix, MixerSubmix);

					if (bInit)
					{
						// Setup the parent-child relationship
						FMixerSubmixPtr ParentSubmixInstance;
						if (InSoundSubmix->ParentSubmix)
						{
							ParentSubmixInstance = GetSubmixInstance(InSoundSubmix->ParentSubmix);
						}
						else
						{
							ParentSubmixInstance = GetMasterSubmix();
						}

						ParentSubmixInstance->AddChildSubmix(MixerSubmix);
						MixerSubmix->SetParentSubmix(ParentSubmixInstance);

						MixerSubmix->Init(InSoundSubmix);
					}
				}
			}
		}
	}

	void FMixerDevice::UnregisterSoundSubmix(USoundSubmix* InSoundSubmix)
	{
		if (InSoundSubmix)
		{
			if (!IsInAudioThread())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UnregisterSoundSubmix"), STAT_AudioUnregisterSoundSubmix, STATGROUP_AudioThreadCommands);

				FMixerDevice* MixerDevice = this;
				FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix]()
				{
					MixerDevice->UnregisterSoundSubmix(InSoundSubmix);
				}, GET_STATID(STAT_AudioUnregisterSoundSubmix));
				return;
			}

			if (!IsMasterSubmixType(InSoundSubmix))
			{
				if (InSoundSubmix)
				{
					Submixes.Remove(InSoundSubmix);
				}
			}
		}
	}

	void FMixerDevice::InitSoundEffectPresets()
	{
#if WITH_EDITOR
		IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
		AudioEditorModule->RegisterEffectPresetAssetActions();
#endif
	}

	FMixerSubmixPtr FMixerDevice::GetSubmixInstance(USoundSubmix* SoundSubmix)
	{
		check(SoundSubmix);
		FMixerSubmixPtr* MixerSubmix = Submixes.Find(SoundSubmix);

		// If the submix hasn't been registered yet, then register it now
		if (!MixerSubmix)
		{
			RegisterSoundSubmix(SoundSubmix, true);
			MixerSubmix = Submixes.Find(SoundSubmix);
		}

		// At this point, this should exist
		check(MixerSubmix);
		return *MixerSubmix;		
	}

	FMixerSourceVoice* FMixerDevice::GetMixerSourceVoice()
	{
		FMixerSourceVoice* Voice = nullptr;
		if (!SourceVoices.Dequeue(Voice))
		{
			Voice = new FMixerSourceVoice();
		}

		Voice->Reset(this);
		return Voice;
	}

	void FMixerDevice::ReleaseMixerSourceVoice(FMixerSourceVoice* InSourceVoice)
	{
		SourceVoices.Enqueue(InSourceVoice);
	}

	int32 FMixerDevice::GetNumSources() const
	{
		return Sources.Num();
	}

	int32 FMixerDevice::GetNumActiveSources() const
	{
		return SourceManager.GetNumActiveSources();
	}

	void FMixerDevice::Get3DChannelMap(const FWaveInstance* InWaveInstance, float EmitterAzimith, float NormalizedOmniRadius, TArray<float>& OutChannelMap)
	{
		// If we're center-channel only, then no need for spatial calculations, but need to build a channel map
		if (InWaveInstance->bCenterChannelOnly)
		{
			// If we only have stereo channels
			if (NumSpatialChannels == 2)
			{
				// Equal volume in left + right channel with equal power panning
				static const float Pan = 1.0f / FMath::Sqrt(2.0f);
				OutChannelMap.Add(Pan);
				OutChannelMap.Add(Pan);
			}
			else
			{
				for (EAudioMixerChannel::Type Channel : PlatformInfo.OutputChannelArray)
				{
					float Pan = (Channel == EAudioMixerChannel::FrontCenter) ? 1.0f : 0.0f;
					OutChannelMap.Add(Pan);
				}
			}

			return;
		}

		float Azimuth = EmitterAzimith;

		const FChannelPositionInfo* PrevChannelInfo = nullptr;
		const FChannelPositionInfo* NextChannelInfo = nullptr;

		for (int32 i = 0; i < CurrentChannelAzimuthPositions.Num(); ++i)
		{
			const FChannelPositionInfo& ChannelPositionInfo = CurrentChannelAzimuthPositions[i];

			if (Azimuth <= ChannelPositionInfo.Azimuth)
			{
				NextChannelInfo = &CurrentChannelAzimuthPositions[i];

				int32 PrevIndex = i - 1;
				if (PrevIndex < 0)
				{
					PrevIndex = CurrentChannelAzimuthPositions.Num() - 1;
				}

				PrevChannelInfo = &CurrentChannelAzimuthPositions[PrevIndex];
				break;
			}
		}

		// If we didn't find anything, that means our azimuth position is at the top of the mapping
		if (PrevChannelInfo == nullptr)
		{
			PrevChannelInfo = &CurrentChannelAzimuthPositions[CurrentChannelAzimuthPositions.Num() - 1];
			NextChannelInfo = &CurrentChannelAzimuthPositions[0];
			AUDIO_MIXER_CHECK(PrevChannelInfo != NextChannelInfo);
		}

		float NextChannelAzimuth = NextChannelInfo->Azimuth;
		float PrevChannelAzimuth = PrevChannelInfo->Azimuth;

		if (NextChannelAzimuth < PrevChannelAzimuth)
		{
			NextChannelAzimuth += 360.0f;
		}

		if (Azimuth < PrevChannelAzimuth)
		{
			Azimuth += 360.0f;
		}

		AUDIO_MIXER_CHECK(NextChannelAzimuth > PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Azimuth > PrevChannelAzimuth);
		float Fraction = (Azimuth - PrevChannelAzimuth) / (NextChannelAzimuth - PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Fraction >= 0.0f && Fraction <= 1.0f);

		// Compute the panning values using equal-power panning law
		float PrevChannelPan; 
		float NextChannelPan;

		FMath::SinCos(&NextChannelPan, &PrevChannelPan, Fraction * 0.5f * PI);

		// Note that SinCos can return values slightly greater than 1.0 when very close to PI/2
		NextChannelPan = FMath::Clamp(NextChannelPan, 0.0f, 1.0f);
		PrevChannelPan = FMath::Clamp(PrevChannelPan, 0.0f, 1.0f);

		float NormalizedOmniRadSquared = NormalizedOmniRadius * NormalizedOmniRadius;
		float OmniAmount = 0.0f;

		if (NormalizedOmniRadSquared > 1.0f)
		{
			OmniAmount = 1.0f - 1.0f / NormalizedOmniRadSquared;
		}

		// OmniPan is the amount of pan to use if fully omni-directional
		AUDIO_MIXER_CHECK(NumSpatialChannels > 0);

		// Build the output channel map based on the current platform device output channel array 

		float DefaultEffectivePan = !OmniAmount ? 0.0f : FMath::Lerp(0.0f, OmniPanFactor, OmniAmount);

		for (EAudioMixerChannel::Type Channel : PlatformInfo.OutputChannelArray)
		{
			float EffectivePan = DefaultEffectivePan;

			// Check for manual channel mapping parameters (LFE and Front Center)
			if (Channel == EAudioMixerChannel::LowFrequency)
			{
				EffectivePan = InWaveInstance->LFEBleed;
			}
			else if (Channel == PrevChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? PrevChannelPan : FMath::Lerp(PrevChannelPan, OmniPanFactor, OmniAmount);
			}
			else if (Channel == NextChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? NextChannelPan : FMath::Lerp(NextChannelPan, OmniPanFactor, OmniAmount);
			}

			if (Channel == EAudioMixerChannel::FrontCenter)
			{
				EffectivePan = FMath::Max(InWaveInstance->VoiceCenterChannelVolume, EffectivePan);
			}

			AUDIO_MIXER_CHECK(EffectivePan >= 0.0f && EffectivePan <= 1.0f);
			OutChannelMap.Add(EffectivePan);

		}
	}

	void FMixerDevice::SetChannelAzimuth(EAudioMixerChannel::Type ChannelType, int32 Azimuth)
	{
		if (ChannelType >= EAudioMixerChannel::TopCenter)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Unsupported mixer channel type: %s"), EAudioMixerChannel::ToString(ChannelType));
			return;
		}

		if (Azimuth < 0 || Azimuth >= 360)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Supplied azimuth is out of range: %d [0, 360)"), Azimuth);
			return;
		}

		DefaultChannelAzimuthPosition[ChannelType].Azimuth = Azimuth;
	}



	int32 FMixerDevice::GetAzimuthForChannelType(EAudioMixerChannel::Type ChannelType)
	{
		if (ChannelType >= EAudioMixerChannel::TopCenter)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Unsupported mixer channel type: %s"), EAudioMixerChannel::ToString(ChannelType));
			return 0;
		}

		return DefaultChannelAzimuthPosition[ChannelType].Azimuth;
	}

	int32 FMixerDevice::GetDeviceSampleRate() const
	{
		return SampleRate;
	}

	int32 FMixerDevice::GetDeviceOutputChannels() const
	{
		return PlatformInfo.NumChannels;
	}

	FMixerSourceManager* FMixerDevice::GetSourceManager()
	{
		return &SourceManager;
	}

	bool FMixerDevice::IsMainAudioDevice() const
	{
		bool bIsMain = (this == GEngine->GetMainAudioDevice());
		return bIsMain;
	}

	void FMixerDevice::WhiteNoiseTest(AlignedFloatBuffer& Output)
	{
		const int32 NumFrames = OpenStreamParams.NumFrames;
		const int32 NumChannels = PlatformInfo.NumChannels;

		static FWhiteNoise WhiteNoise(0.2f);

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				int32 Index = FrameIndex * NumChannels + ChannelIndex;
				Output[Index] += WhiteNoise.Generate();
			}
		}
	}

	void FMixerDevice::SineOscTest(AlignedFloatBuffer& Output)
	{
		const int32 NumFrames = OpenStreamParams.NumFrames;
		const int32 NumChannels = PlatformInfo.NumChannels;

		check(NumChannels > 0);

		static FSineOsc SineOscLeft(PlatformInfo.SampleRate, 440.0f, 0.2f);
		static FSineOsc SineOscRight(PlatformInfo.SampleRate, 220.0f, 0.2f);

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			int32 Index = FrameIndex * NumChannels;

			Output[Index] += SineOscLeft.ProcessAudio();

			if (NumChannels > 1)
			{
				Output[Index + 1] += SineOscRight.ProcessAudio();
			}
		}
	}

}
