#include "AudioMixerPlatformSDL.h"
#include "ModuleManager.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"
#include "ADPCMAudioInfo.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAudioMixerSDL, Log, All);
DEFINE_LOG_CATEGORY(LogAudioMixerSDL);

namespace Audio
{
	// Static callback function used in SDL
	static void OnBufferEnd(void* BufferContext, uint8* OutputBuffer, int32 OutputBufferLength)
	{
		check(BufferContext);
		FMixerPlatformSDL* MixerPlatform = (FMixerPlatformSDL*)BufferContext;
		MixerPlatform->HandleOnBufferEnd(OutputBuffer, OutputBufferLength);
	}

	FMixerPlatformSDL::FMixerPlatformSDL()
		: AudioDeviceID(INDEX_NONE)
		, OutputBuffer(nullptr)
		, OutputBufferByteLength(0)
		, bSuspended(false)
		, bInitialized(false)
	{
	}

	FMixerPlatformSDL::~FMixerPlatformSDL()
	{
		if (bInitialized)
		{
			TeardownHardware();
		}
	}

	bool FMixerPlatformSDL::InitializeHardware()
	{
		if (bInitialized)
		{
			UE_LOG(LogAudioMixerSDL, Error, TEXT("SDL Audio already initialized."));
			return false;
		}

		int32 Result = SDL_InitSubSystem(SDL_INIT_AUDIO);
		if (Result < 0)
		{
			UE_LOG(LogAudioMixerSDL, Error, TEXT("SDL_InitSubSystem create failed: %d"), Result);
			return false;
		}

		const char* DriverName = SDL_GetCurrentAudioDriver();
		UE_LOG(LogAudioMixerSDL, Display, TEXT("Initialized SDL using %s platform API backend."), ANSI_TO_TCHAR(DriverName));

		bInitialized = true;
		return true;
	}

	bool FMixerPlatformSDL::TeardownHardware()
	{
		if(!bInitialized)
		{
			return true;
		}

		StopAudioStream();
		CloseAudioStream();

		bInitialized = false;

		return true;
	}

	bool FMixerPlatformSDL::IsInitialized() const
	{
		return bInitialized;
	}

	bool FMixerPlatformSDL::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		if (!bInitialized)
		{
			UE_LOG(LogAudioMixerSDL, Error, TEXT("SDL2 Audio is not initialized."));
			return false;
		}

		SDL_bool IsCapture = SDL_FALSE;
		OutNumOutputDevices = SDL_GetNumAudioDevices(IsCapture);
		return true;
	}

	bool FMixerPlatformSDL::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		// To figure out the output device info, attempt to init at 7.1, and 48k.
		// SDL_OpenAudioDevice will attempt open the audio device with that spec but return what it actually used. We'll report that in OutInfo
		FAudioPlatformSettings PlatformSettings = GetPlatformSettings();
		SDL_AudioSpec DesiredSpec;
		DesiredSpec.freq = PlatformSettings.SampleRate;

		// HTML5 supports s16 format only
#if PLATFORM_HTML5
		DesiredSpec.format = AUDIO_S16;
		DesiredSpec.channels = 2;
#else
		DesiredSpec.format = AUDIO_F32;
		DesiredSpec.channels = 6;
#endif
		
		DesiredSpec.samples = PlatformSettings.CallbackBufferFrameSize;
		DesiredSpec.callback = OnBufferEnd;
		DesiredSpec.userdata = (void*)this;

		// It's not possible with SDL to tell whether a given index is default. It only supports directly opening a device handle by passing in a nullptr to SDL_OpenAudioDevice.
		OutInfo.bIsSystemDefault = false;

		const char* AudioDeviceName = nullptr;
		FString DeviceName;

		if (InDeviceIndex != AUDIO_MIXER_DEFAULT_DEVICE_INDEX)
		{
			AudioDeviceName = SDL_GetAudioDeviceName(InDeviceIndex, SDL_FALSE);
			DeviceName = ANSI_TO_TCHAR(AudioDeviceName);
		}
		else
		{
			DeviceName = TEXT("Default Audio Device");
		}

		SDL_AudioSpec ActualSpec;
		SDL_AudioDeviceID TempAudioDeviceID = SDL_OpenAudioDevice(AudioDeviceName, SDL_FALSE, &DesiredSpec, &ActualSpec, SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
		if (!TempAudioDeviceID)
		{
			const char* ErrorText = SDL_GetError();
			UE_LOG(LogAudioMixerSDL, Error, TEXT("%s"), ANSI_TO_TCHAR(ErrorText));
		}

		// Name and Id are the same for SDL
		OutInfo.DeviceId = DeviceName;
		OutInfo.Name = OutInfo.DeviceId;
		OutInfo.SampleRate = ActualSpec.freq;
		
		// HTML5 supports s16 format only
#if PLATFORM_HTML5
		OutInfo.Format = EAudioMixerStreamDataFormat::Int16;
#else
		OutInfo.Format = EAudioMixerStreamDataFormat::Float;
#endif
		OutInfo.NumChannels = ActualSpec.channels;

		// Assume default channel map order, SDL doesn't support us querying it directly
		OutInfo.OutputChannelArray.Reset();
		for (int32 i = 0; i < OutInfo.NumChannels; ++i)
		{
			OutInfo.OutputChannelArray.Add(EAudioMixerChannel::Type(i));
		}

		SDL_CloseAudioDevice(TempAudioDeviceID);

		return true;
	}

	bool FMixerPlatformSDL::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		// It's not possible to know what index the default audio device is.
		OutDefaultDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
		return true;
	}

	bool FMixerPlatformSDL::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			return false;
		}

		OpenStreamParams = Params;

		AudioStreamInfo.Reset();
		AudioStreamInfo.OutputDeviceIndex = OpenStreamParams.OutputDeviceIndex;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = OpenStreamParams.NumBuffers;
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;

		if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
		{
			return false;
		}

		// HTML5 supports s16 format only
#if PLATFORM_HTML5
		AudioSpecPrefered.format = AUDIO_S16;
#else
		AudioSpecPrefered.format = AUDIO_F32;
#endif
		AudioSpecPrefered.freq = Params.SampleRate;
		AudioSpecPrefered.channels = AudioStreamInfo.DeviceInfo.NumChannels;
		AudioSpecPrefered.samples = OpenStreamParams.NumFrames;
		AudioSpecPrefered.callback = OnBufferEnd;
		AudioSpecPrefered.userdata = (void*)this;

		const char* DeviceName = nullptr;
		if (OpenStreamParams.OutputDeviceIndex != AUDIO_MIXER_DEFAULT_DEVICE_INDEX && OpenStreamParams.OutputDeviceIndex < (uint32)SDL_GetNumAudioDevices(0))
		{
			DeviceName = SDL_GetAudioDeviceName(OpenStreamParams.OutputDeviceIndex, 0);
		}

		AudioDeviceID = SDL_OpenAudioDevice(DeviceName, 0, &AudioSpecPrefered, &AudioSpecReceived, 0);

		if (!AudioDeviceID)
		{
			const char* ErrorText = SDL_GetError();
			UE_LOG(LogAudioMixerSDL, Error, TEXT("%s"), ANSI_TO_TCHAR(ErrorText));
			return false;
		}

		// Make sure our device initialized as expected, we should have already filtered this out before this point.
		check(AudioSpecReceived.channels == AudioSpecPrefered.channels);
		check(AudioSpecReceived.samples == OpenStreamParams.NumFrames);

		// Compute the expected output byte length
#if PLATFORM_HTML5
		OutputBufferByteLength = OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels * sizeof(int16);
#else
		OutputBufferByteLength = OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels * sizeof(float);
#endif
		check(OutputBufferByteLength == AudioSpecReceived.size);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;

		return true;
	}

	bool FMixerPlatformSDL::CloseAudioStream()
	{
		if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Closed)
		{
			return false;
		}

		if (!StopAudioStream())
		{
			return false;
		}

		if (AudioDeviceID != INDEX_NONE)
		{
			SDL_CloseAudioDevice(AudioDeviceID);
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
		return true;
	}

	bool FMixerPlatformSDL::StartAudioStream()
	{
		if (!bInitialized || (AudioStreamInfo.StreamState != EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
		{
			return false;
		}

		// Start generating audio
		BeginGeneratingAudio();

		// Unpause audio device to start it rendering audio
		SDL_PauseAudioDevice(AudioDeviceID, 0);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;

		return true;
	}

	bool FMixerPlatformSDL::StopAudioStream()
	{
		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped && AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			// Pause the audio device
			SDL_PauseAudioDevice(AudioDeviceID, 1);

			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
			{
				StopGeneratingAudio();
			}

			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}

		return true;
	}

	FAudioPlatformDeviceInfo FMixerPlatformSDL::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

	void FMixerPlatformSDL::SubmitBuffer(const uint8* Buffer)
	{
		if (OutputBuffer)
		{
			FMemory::Memcpy(OutputBuffer, Buffer, OutputBufferByteLength);
		}
	}

	void FMixerPlatformSDL::HandleOnBufferEnd(uint8* InOutputBuffer, int32 InOutputBufferByteLength)
	{
		if (!bIsDeviceInitialized)
		{
			return;
		}

		OutputBuffer = InOutputBuffer;
		check(InOutputBufferByteLength == OutputBufferByteLength);

		ReadNextBuffer();
	}

	FName FMixerPlatformSDL::GetRuntimeFormat(USoundWave* InSoundWave)
	{
		if (InSoundWave->IsStreaming())
		{
			return FName(TEXT("OPUS"));
		}

		static FName NAME_OGG(TEXT("OGG"));
		if (InSoundWave->HasCompressedData(NAME_OGG))
		{
			return NAME_OGG;
		}

		static FName NAME_ADPCM(TEXT("ADPCM"));
		return NAME_ADPCM;
	}

	bool FMixerPlatformSDL::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		return true;
	}

	ICompressedAudioInfo* FMixerPlatformSDL::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
		check(InSoundWave);

		if (InSoundWave->IsStreaming())
		{
			return new FOpusAudioInfo();
		}

		static FName NAME_OGG(TEXT("OGG"));
		if (InSoundWave->HasCompressedData(NAME_OGG))
		{
			return new FVorbisAudioInfo();
		}

		return new FADPCMAudioInfo();
	}

	FString FMixerPlatformSDL::GetDefaultDeviceName()
	{
		static FString DefaultName(TEXT("Default SDL Audio Device."));
		return DefaultName;
	}

	void FMixerPlatformSDL::ResumeContext()
	{
		if (bSuspended)
		{
			SDL_UnlockAudioDevice(AudioDeviceID);
			UE_LOG(LogAudioMixerSDL, Display, TEXT("Resuming Audio"));
			bSuspended = false;
		}
	}

	void FMixerPlatformSDL::SuspendContext()
	{
		if (!bSuspended)
		{
			SDL_LockAudioDevice(AudioDeviceID);
			UE_LOG(LogAudioMixerSDL, Display, TEXT("Suspending Audio"));
			bSuspended = true;
		}
	}

	FAudioPlatformSettings FMixerPlatformSDL::GetPlatformSettings() const
	{
#if PLATFORM_LINUX
		return FAudioPlatformSettings::GetPlatformSettings(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"));
#else
		// On HTML5 and Windows, use default parameters.
		FAudioPlatformSettings Settings;
		Settings.SampleRate = 48000;
		Settings.MaxChannels = 0;
		Settings.NumBuffers = 2;
		Settings.CallbackBufferFrameSize = 1024;
		return Settings;
#endif
	}
}
