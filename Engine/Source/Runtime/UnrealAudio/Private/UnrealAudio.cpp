// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "UnrealAudioTypes.h"
#include "Modules/ModuleManager.h"
#include "UnrealAudioPrivate.h"
#include "HAL/LowLevelMemTracker.h"

#if ENABLE_UNREAL_AUDIO

#if PLATFORM_WINDOWS
#define AUDIO_DEFAULT_DEVICE_MODULE_NAME "UnrealAudioXAudio2"
#elif PLATFORM_MAC
#define AUDIO_DEFAULT_DEVICE_MODULE_NAME "UnrealAudioCoreAudio"
#endif

DEFINE_LOG_CATEGORY(LogUnrealAudio);

IMPLEMENT_MODULE(UAudio::FUnrealAudioModule, UnrealAudio);

namespace UAudio
{
	/**
	 * Audio device callback function, wraps UnrealAudio module and fowards call.
	 *
	 * @param [in,out]	CallbackInfo	Information describing the audio device callback.
	 *
	 * @return	true if it succeeds, false if it fails.
	 */

	static bool AudioDeviceCallbackFunc(FCallbackInfo& CallbackInfo)
	{
		FUnrealAudioModule* AudioModule = (FUnrealAudioModule*)CallbackInfo.UserData;
		return AudioModule->AudioDeviceCallback(CallbackInfo);
	}

	/************************************************************************/
	/* FUnrealAudioModule Implementation									*/
	/************************************************************************/

	FUnrealAudioModule::FUnrealAudioModule()
		: UnrealAudioDevice(nullptr)
		, NumBackgroundTasks(0)
		, EmitterManager(this)
		, VoiceManager(this)
		, SoundFileManager(this)
		, SoundFileDllHandle(nullptr)
		, bIsStoppingSystemThread(false)
		, AudioSystemTimeSec(0.0)
		, SystemThread(nullptr)
		, AudioThreadCommandQueue(500)
		, MainThreadCommandQueue(100)
		, SystemThreadUpdateTime(0.033)
	{
	}

	FUnrealAudioModule::~FUnrealAudioModule()
	{
	}

	bool FUnrealAudioModule::Initialize()
	{
		ModuleName = GetDefaultDeviceModuleName();
		return InitializeInternal();
	}

	void FUnrealAudioModule::Update()
	{
		MainThreadChecker.CheckThread();

		ExecuteMainThreadCommands();

		// Update the sound file manager from the main thread.
		SoundFileManager.Update();

	}

	bool FUnrealAudioModule::Initialize(const FString& DeviceModuleName)
	{
		FString Name("UnrealAudio");
		Name += DeviceModuleName;
		ModuleName = *Name;
		return InitializeInternal();
	}

	bool FUnrealAudioModule::InitializeInternal()
	{
		bool bSuccess = false;

		bSuccess = InitializeAudioDevice();
		check(bSuccess);

		bSuccess = InitializeAudioSystem();
		check(bSuccess);

		return bSuccess;
	}

	bool FUnrealAudioModule::InitializeAudioSystem()
	{
		bool bSuccess = false;

		FVoiceManagerSettings VoiceManagerSettings;
		VoiceManagerSettings.MaxVoiceCount = 32;
		VoiceManagerSettings.MaxVirtualVoiceCount = 1000;
		VoiceManagerSettings.MaxPitch = 4.0f;
		VoiceManagerSettings.MinPitch = 0.01f;
		VoiceManagerSettings.ControlUpdateRateSeconds = SystemThreadUpdateTime;
		VoiceManagerSettings.NumDecoders = 2;
		VoiceManagerSettings.DecoderSettings.DecodeBufferFrames = 1024;
		VoiceManagerSettings.DecoderSettings.NumDecodeBuffers = 3;

		VoiceManager.Init(VoiceManagerSettings);

		FSoundFileManagerSettings SoundFileManagerSettings;
		SoundFileManagerSettings.MaxNumberOfLoadedSounds = 5000;
		SoundFileManagerSettings.TargetMemoryLimit = 10 * 1024 * 1024; // Convert MB to Bytes
		SoundFileManagerSettings.NumLoadingThreads = 2;
		SoundFileManagerSettings.FlushTimeThreshold = 10.0f;
		SoundFileManagerSettings.TimeDeltaPerUpdate = 0.033f;
		SoundFileManagerSettings.LoadingThreadPriority = EThreadPriority::TPri_Normal;

		SoundFileManager.Init(SoundFileManagerSettings);

		// Define the default sound file load settings
		DefaultConvertFormat.bPerformPeakNormalization = false;
		DefaultConvertFormat.EncodingQuality = 0.75;
		DefaultConvertFormat.Format = UAudio::ESoundFileFormat::OGG | UAudio::ESoundFileFormat::VORBIS;
		DefaultConvertFormat.SampleRate = 44100;

		bSuccess = LoadSoundFileLib();

		if (bSuccess)
		{
			// Store this thread ID as the "main" thread id
			MainThreadChecker.InitThread();

			// Create the system thread
			bIsStoppingSystemThread = false;
			SystemThread = FRunnableThread::Create(this, TEXT("Audio System Thread"), 0, TPri_Normal);
		}

		InitializeSystemTests(this);

		return bSuccess;
	}

	bool FUnrealAudioModule::InitializeAudioDevice()
	{
		LLM_SCOPE(ELLMTag::Audio);

		UnrealAudioDevice = FModuleManager::LoadModulePtr<IUnrealAudioDeviceModule>(ModuleName);
		if (UnrealAudioDevice)
		{
			if (UnrealAudioDevice->Initialize())
			{
				// TODO: Use some sort of user savings feature to store which audio device index (or GUID) to use
				uint32 DefaultDeviceIndex;
				if (!UnrealAudioDevice->GetDefaultOutputDeviceIndex(DefaultDeviceIndex))
				{
					UE_LOG(LogUnrealAudio, Error, TEXT("Failed to get default audio device index."));
					return false;
				}

				FDeviceInfo DeviceInfo;
				if (!UnrealAudioDevice->GetOutputDeviceInfo(DefaultDeviceIndex, DeviceInfo))
				{
					UE_LOG(LogUnrealAudio, Warning, TEXT("Failed to get audio device info."));
					return false;
				}

				// TODO: Get this information from a settings or ini file
				FCreateStreamParams CreateStreamParams;
				CreateStreamParams.OutputDeviceIndex = DefaultDeviceIndex;
				CreateStreamParams.CallbackFunction = &AudioDeviceCallbackFunc;
				CreateStreamParams.UserData = this;
				CreateStreamParams.CallbackBlockSize = 1024;

				if (!UnrealAudioDevice->CreateStream(CreateStreamParams))
				{
					UE_LOG(LogUnrealAudio, Display, TEXT("Failed to create an audio device stream."));
					return false;
				}

				InitializeDeviceTests(this);

				if (!UnrealAudioDevice->StartStream())
				{
					UE_LOG(LogUnrealAudio, Display, TEXT("Failed to start the audio device stream."));
					return false;
				}

				UE_LOG(LogUnrealAudio, Log, TEXT("Succeeded creating output audio device..."));
			}
			else
			{
				UE_LOG(LogUnrealAudio, Error, TEXT("Failed to initialize audio device module."));
				UnrealAudioDevice = nullptr;
				return false;
			}
		}
		else
		{
			UnrealAudioDevice = CreateDummyDeviceModule();
			return true;
		}

		return true;
	}

	void FUnrealAudioModule::Shutdown()
	{
		MainThreadChecker.CheckThread();

		// Block shutdown until the number of background tasks goes to zero before shutting down the audio module
		static const int32 BackgroundTaskTimeout = 500;
		int32 BackgroundTaskWaitCount = 0;
		while (NumBackgroundTasks.GetValue() != 0 && BackgroundTaskWaitCount++ < BackgroundTaskTimeout)
		{
			FPlatformProcess::Sleep(1);
		}

		if (BackgroundTaskWaitCount == BackgroundTaskTimeout)
		{
			UE_LOG(LogUnrealAudio, Error, TEXT("Timed out while waiting for background tasks to finish when shutting down unreal audio module."));
		}

		SoundFileManager.Shutdown();

		ShutdownAudioDevice();

		// Stop the system thread
		Stop();

		EDeviceApi::Type DeviceApi;
		if (UnrealAudioDevice && UnrealAudioDevice->GetDevicePlatformApi(DeviceApi) && DeviceApi == EDeviceApi::DUMMY)
		{
			delete UnrealAudioDevice;
			UnrealAudioDevice = nullptr;
		}

		ShutdownSoundFileLib();

	}

	void FUnrealAudioModule::ShutdownAudioDevice()
	{
		check(UnrealAudioDevice);
		bool bSuccess = UnrealAudioDevice->StopStream();
		check(bSuccess);
		bSuccess = UnrealAudioDevice->ShutdownStream();
		check(bSuccess);
	}

	int32 FUnrealAudioModule::GetNumBackgroundTasks() const
	{
		return NumBackgroundTasks.GetValue();
	}

	FName FUnrealAudioModule::GetDefaultDeviceModuleName() const
	{
		// TODO: Pull the device module name to use from an engine ini or an audio ini file
		return FName(AUDIO_DEFAULT_DEVICE_MODULE_NAME);
	}


	IUnrealAudioDeviceModule* FUnrealAudioModule::GetDeviceModule()
	{
		return UnrealAudioDevice;
	}

	void FUnrealAudioModule::IncrementBackgroundTaskCount()
	{
		NumBackgroundTasks.Increment();
	}

	void FUnrealAudioModule::DecrementBackgroundTaskCount()
	{
		NumBackgroundTasks.Decrement();
	}

	double FUnrealAudioModule::GetCurrentTimeSec() const
	{
		return AudioSystemTimeSec;
	}

	bool FUnrealAudioModule::Init()
	{
		AudioThreadChecker.InitThread();
		return true;
	}

	uint32 FUnrealAudioModule::Run()
	{
		LLM_SCOPE(ELLMTag::Audio);

		AudioThreadChecker.CheckThread();

		// Initialize the audio system time to 0.0. This is a time
		// which will be used to synchronize timer-events in various audio subsystems and 
		// represents the up-time of the audio system thread.
		AudioSystemTimeSec = 0.0;
		double PreviousTime = FPlatformTime::Seconds();

		while (!bIsStoppingSystemThread)
		{
			double StartTime = FPlatformTime::Seconds();

			// Update the audio system time before updating any sub-systems
			AudioSystemTimeSec += (StartTime - PreviousTime);
			PreviousTime = StartTime;

			// Update any state from previous commands
			VoiceManager.Update();

			ExecuteAudioThreadCommands();

			// call the update system test function for any test code that needs to be performed in system thread
			FUnrealAudioModule::UpdateSystemTests();

			// To sleep the audio system thread for a set amount of time at regular intervals
			// we need to subtract how long the update took from a set delta update time
			double DeltaTime = FPlatformTime::Seconds() - StartTime;
			double TimeLeft = SystemThreadUpdateTime - DeltaTime;

			// If we still have time left, then sleep for whatever amount that is
			// if we don't have time left, then the system update took way too long and we 
			// want to log the performance issue.
			if (TimeLeft > 0.0)
			{
				FPlatformProcess::Sleep((float)TimeLeft);
			}
			else
			{
				UE_LOG(LogUnrealAudio,
					Warning,
					TEXT("Audio system thread update took longer than %.2f seconds (%.2f)"),
					SystemThreadUpdateTime,
					DeltaTime
				);
			}
		}

		return 0;
	}

	void FUnrealAudioModule::Stop()
	{
		bIsStoppingSystemThread = true;
	}

	void FUnrealAudioModule::SendAudioThreadCommand(const FCommand& Command)
	{
		MainThreadChecker.CheckThread();

		AudioThreadCommandQueue.Enqueue(Command);
	}

	void FUnrealAudioModule::SendMainThreadCommand(const FCommand& Command)
	{
		AudioThreadChecker.CheckThread();

		MainThreadCommandQueue.Enqueue(Command);
	}

	void FUnrealAudioModule::ExecuteAudioThreadCommands()
	{
		AudioThreadChecker.CheckThread();

		FCommand Command;
		while (AudioThreadCommandQueue.Dequeue(Command))
		{
			switch (Command.Id)
			{
				default:
				case EAudioThreadCommand::NONE:

				// Voice commands
				//////////////////////////////////////////////////////////////////////////

				case EAudioThreadCommand::VOICE_PLAY:
					VoiceManager.PlayVoice(Command);
					break;

				case EAudioThreadCommand::VOICE_PAUSE:
					VoiceManager.PauseVoice(Command);
					break;

				case EAudioThreadCommand::VOICE_STOP:
					VoiceManager.StopVoice(Command);
					break;

				case EAudioThreadCommand::VOICE_SET_VOLUME_SCALE:
					VoiceManager.SetVolumeScale(Command);
					break;

				case EAudioThreadCommand::VOICE_SET_PITCH_SCALE:
					VoiceManager.SetPitchScale(Command);
					break;

				// Emitter commands
				//////////////////////////////////////////////////////////////////////////

				case EAudioThreadCommand::EMITTER_CREATE:
					EmitterManager.CreateEmitter(Command);
					break;

				case EAudioThreadCommand::EMITTER_RELEASE:
					EmitterManager.ReleaseEmitter(Command);
					break;

				case EAudioThreadCommand::EMITTER_SET_POSITION:
					EmitterManager.SetEmitterPosition(Command);
					break;
			}

		}
	}

	void FUnrealAudioModule::ExecuteMainThreadCommands()
	{
		FCommand Command;
		while (MainThreadCommandQueue.Dequeue(Command))
		{
			switch (Command.Id)
			{
				default:
				case EMainThreadCommand::NONE:

				// Voice commands
				//////////////////////////////////////////////////////////////////////////

				case EMainThreadCommand::VOICE_DONE:
					VoiceManager.NotifyVoiceDone(Command);
					break;

				case EMainThreadCommand::VOICE_REAL:
					VoiceManager.NotifyVoiceReal(Command);
					break;

				case EMainThreadCommand::VOICE_VIRTUAL:
					VoiceManager.NotifyVoiceVirtual(Command);
					break;

				case EMainThreadCommand::VOICE_SUSPEND:
					VoiceManager.NotifyVoiceSuspend(Command);
					break;

			}

		}
	}

	TSharedPtr<IEmitter> FUnrealAudioModule::EmitterCreate()
	{
		return TSharedPtr<IEmitter>(new FEmitter(this));
	}

	FEmitterManager& FUnrealAudioModule::GetEmitterManager()
	{
		return EmitterManager;
	}

	TSharedPtr<IVoice> FUnrealAudioModule::VoiceCreate(const FVoiceInitializationParams& Params)
	{
		if (!Params.SoundFile.IsValid())
		{
			UE_LOG(LogUnrealAudio, Error, TEXT("Must give a valid ISoundFile object when creating a voice."));
			return TSharedPtr<IVoice>();
		}
		else
		{
			FVoiceInitializationParams InitParams;
			InitParams.BaselinePitchScale = FMath::Clamp(Params.BaselinePitchScale, 0.0f, 4.0f);
			InitParams.BaselineVolumeScale = FMath::Clamp(Params.BaselineVolumeScale, 0.0f, 1.0f);
			InitParams.PriorityWeight = FMath::Max(Params.PriorityWeight, 0.0f);
			InitParams.Emitter = Params.Emitter;
			InitParams.SoundFile = Params.SoundFile;
			InitParams.bIsLooping = Params.bIsLooping;
			TSharedPtr<IVoice> NewVoice = TSharedPtr<IVoice>(new FVoice(this, InitParams));
			return NewVoice;
		}
	}

	FVoiceManager& FUnrealAudioModule::GetVoiceManager()
	{
		return VoiceManager;
	}

	bool FUnrealAudioModule::AudioDeviceCallback(struct FCallbackInfo& CallbackInfo)
	{
		// do any test device callbacks
		DeviceTestCallback(CallbackInfo);
		
		VoiceManager.Mix(CallbackInfo);

		return true;
	}

}

#endif // #if ENABLE_UNREAL_AUDIO
