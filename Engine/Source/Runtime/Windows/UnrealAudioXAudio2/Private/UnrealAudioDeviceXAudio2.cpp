// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
Concrete implementation of IAudioDevice for XAudio2

See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405049%28v=vs.85%29.aspx

*/

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioBuffer.h"
#include "ModuleManager.h"

#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <xaudio2.h>
#include "HideWindowsPlatformTypes.h"

#if ENABLE_UNREAL_AUDIO

// Used to toggle on white noise testing for xaudio2 output
#define UNREAL_AUDIO_TEST_WHITE_NOISE 0

// See MSDN documentation for what these error codes mean in the context of the API call
static const TCHAR* GetXAudio2Error(HRESULT Result)
{
	switch (Result)
	{
		case HRESULT(XAUDIO2_E_INVALID_CALL):			return TEXT("XAUDIO2_E_INVALID_CALL");
		case HRESULT(XAUDIO2_E_XMA_DECODER_ERROR):		return TEXT("XAUDIO2_E_XMA_DECODER_ERROR");
		case HRESULT(XAUDIO2_E_XAPO_CREATION_FAILED):	return TEXT("XAUDIO2_E_XAPO_CREATION_FAILED");
		case HRESULT(XAUDIO2_E_DEVICE_INVALIDATED):		return TEXT("XAUDIO2_E_DEVICE_INVALIDATED");
		case REGDB_E_CLASSNOTREG:						return TEXT("REGDB_E_CLASSNOTREG");
		case CLASS_E_NOAGGREGATION:						return TEXT("CLASS_E_NOAGGREGATION");
		case E_NOINTERFACE:								return TEXT("E_NOINTERFACE");
		case E_POINTER:									return TEXT("E_POINTER");
		case E_INVALIDARG:								return TEXT("E_INVALIDARG");
		case E_OUTOFMEMORY:								return TEXT("E_OUTOFMEMORY");
		default: return TEXT("UKNOWN");
	}
}

/** Some helper defines to reduce boilerplate code for error handling windows API calls. */

#define CLEANUP_ON_FAIL(Result)						\
	if (FAILED(Result))								\
	{												\
		const TCHAR* Err = GetXAudio2Error(Result);	\
		UA_DEVICE_PLATFORM_ERROR(Err);				\
		goto Cleanup;								\
	}

#define RETURN_FALSE_ON_FAIL(Result)				\
	if (FAILED(Result))								\
	{												\
		const TCHAR* Err = GetXAudio2Error(Result);	\
		UA_DEVICE_PLATFORM_ERROR(Err);				\
		return false;								\
	}

namespace UAudio
{
	/** Struct used to map Unreal Audio enumerations for speaker types to device speaker types */
	struct FSpeakerMaskMapping
	{
		ESpeaker::Type UnrealSpeaker;
		long XAudio2SpeakerTypeFlag;
	};

	/** Mapping of XAudio2/Windows speaker type enumerations and unreal audio enumerations */
	FSpeakerMaskMapping SpeakerMaskMapping[18] =
	{
		{ ESpeaker::FRONT_LEFT, SPEAKER_FRONT_LEFT },
		{ ESpeaker::FRONT_RIGHT, SPEAKER_FRONT_RIGHT },
		{ ESpeaker::FRONT_CENTER, SPEAKER_FRONT_CENTER },
		{ ESpeaker::LOW_FREQUENCY, SPEAKER_LOW_FREQUENCY },
		{ ESpeaker::BACK_LEFT, SPEAKER_BACK_LEFT },
		{ ESpeaker::BACK_RIGHT, SPEAKER_BACK_RIGHT },
		{ ESpeaker::FRONT_LEFT_OF_CENTER, SPEAKER_FRONT_LEFT_OF_CENTER },
		{ ESpeaker::FRONT_RIGHT_OF_CENTER, SPEAKER_FRONT_RIGHT_OF_CENTER },
		{ ESpeaker::BACK_CENTER, SPEAKER_BACK_CENTER },
		{ ESpeaker::SIDE_LEFT, SPEAKER_SIDE_LEFT },
		{ ESpeaker::SIDE_RIGHT, SPEAKER_SIDE_RIGHT },
		{ ESpeaker::TOP_CENTER, SPEAKER_TOP_CENTER },
		{ ESpeaker::TOP_FRONT_LEFT, SPEAKER_TOP_FRONT_LEFT },
		{ ESpeaker::TOP_FRONT_CENTER, SPEAKER_TOP_FRONT_CENTER },
		{ ESpeaker::TOP_FRONT_RIGHT, SPEAKER_TOP_FRONT_RIGHT },
		{ ESpeaker::TOP_BACK_LEFT, SPEAKER_TOP_BACK_LEFT },
		{ ESpeaker::TOP_BACK_CENTER, SPEAKER_TOP_BACK_CENTER },
		{ ESpeaker::TOP_BACK_RIGHT, SPEAKER_TOP_BACK_RIGHT },
	};

	/**
	* FXAudio2VoiceCallback
	* XAudio2 implementation of IXAudio2VoiceCallback
	* This callback class is used to get event notifications on buffer end (when a buffer has finished processing).
	* This is used to signal the I/O thread that it can request another buffer from the user callback.
	*/
	class FXAudio2VoiceCallback final : public IXAudio2VoiceCallback
	{
	public:
		FXAudio2VoiceCallback()
			: BufferEndEvent(nullptr)
		{
		}

		virtual ~FXAudio2VoiceCallback()
		{
		}

		/** Sets the windows handle/event to use on buffer end */
		void SetBufferEndEvent(HANDLE InBufferEndEvent)
		{
			BufferEndEvent = InBufferEndEvent;
		}

	private:
		void STDCALL OnVoiceProcessingPassStart(UINT32 BytesRequired) {}
		void STDCALL OnVoiceProcessingPassEnd() {}
		void STDCALL OnStreamEnd() {}
		void STDCALL OnBufferStart(void* BufferContext) {}
		void STDCALL OnLoopEnd(void* BufferContext) {}
		void STDCALL OnVoiceError(void* BufferContext, HRESULT Error) {}
		void STDCALL OnBufferEnd(void* BufferContext)
		{
			SetEvent(BufferEndEvent);
		}

	private:
		HANDLE BufferEndEvent;
	};

	/**
	* FUnrealAudioXAudio2
	* XAudio2 implementation of IUnrealAudioDeviceModule
	* Inherits from FRunnable so that audio device I/O can be run on separate thread.
	*/
	class FUnrealAudioXAudio2 : public IUnrealAudioDeviceModule,
								public FRunnable
	{
	public:
		FUnrealAudioXAudio2();
		~FUnrealAudioXAudio2();

		// IUnrealAudioDeviceModule
		bool Initialize() override;
		bool Shutdown() override;
		bool GetDevicePlatformApi(EDeviceApi::Type & OutType) const override;
		bool GetNumOutputDevices(uint32& OutNumDevices) const override;
		bool GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& DeviceInfo) const override;
		bool GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const override;
		bool StartStream() override;
		bool StopStream() override;
		bool ShutdownStream() override;
		bool GetLatency(uint32& OutputDeviceLatency) const override;
		bool GetFrameRate(uint32& OutFrameRate) const override;

		// FRunnable
		uint32 Run() override;
		void Stop() override;

	private:

		/**  Simple struct to wrap a user buffer. */
		struct FOutputBuffer
		{
			FOutputBuffer(int32 Size)
			{
				Buffer.Init(0.0f, Size);
			}
			TArray<float> Buffer;
		};

		/**
		* FXAudio2Info
		* 
		* Structure for holding XAudio2 specific resources.
		*/
		struct FXAudio2Info
		{
			FXAudio2Info()
				: XAudio2System(nullptr)
				, MasteringVoice(nullptr)
				, OutputStreamSourceVoice(nullptr)
				, OutputBufferEndEvent(nullptr)
				, MaxQueuedBuffers(3)
				, CurrentBufferIndex(0)
				, bDeviceOpen(false)
			{
			}

			/** XAudio2 system object */
			IXAudio2* XAudio2System;

			/** Mastering voice is the connection to a specific audio device. */
			IXAudio2MasteringVoice* MasteringVoice;

			/** Single source voice that will serve as our user-callback to platform-independent mixing code */
			IXAudio2SourceVoice* OutputStreamSourceVoice;

			/** Callback object to get notifications on buffer end. */
			FXAudio2VoiceCallback OutputVoiceCallback;

			/** Handle used to synchronize buffer end events and new buffers */
			HANDLE OutputBufferEndEvent;

			/** Max number of buffers to use for "ping-pong" buffer design (i.e. getting new user-buffer while writing out old buffer to hardware) */
			int32 MaxQueuedBuffers;

			/** Current index in the array of user buffers. */
			int32 CurrentBufferIndex;

			/** The array of user buffers */
			TArray<FOutputBuffer> OutputBuffers;
			bool bDeviceOpen;
		};

		/** XAudio2-specific data */
		FXAudio2Info XAudio2Info;

		/** Whether or not the device api has been initialized */
		bool bInitialized;

		/** Whether or not com was initialized */
		bool bComInitialized;

	private:
		bool OpenDevice(const FCreateStreamParams& Params) override;

		// Helper functions
		void GetDeviceInfoFromFormat(const WAVEFORMATEX* WaveFormatEx, FDeviceInfo& DeviceInfo) const;

	};

	/**
	* FUnrealAudioXAudio2 Implementation
	*/

	FUnrealAudioXAudio2::FUnrealAudioXAudio2()
		: bInitialized(false)
		, bComInitialized(false)
	{
	}

	FUnrealAudioXAudio2::~FUnrealAudioXAudio2()
	{
		if (bInitialized)
		{
			Shutdown();
		}
	}

	bool FUnrealAudioXAudio2::Initialize()
	{
		if (bInitialized)
		{
			return false;
		}

		bComInitialized = FWindowsPlatformMisc::CoInitialize();
		HRESULT Result = XAudio2Create(&XAudio2Info.XAudio2System);
		RETURN_FALSE_ON_FAIL(Result);
		bInitialized = true;
		return true;
	}

	bool FUnrealAudioXAudio2::Shutdown()
	{
		if (!bInitialized)
		{
			return false;
		}

		SAFE_RELEASE(XAudio2Info.XAudio2System);

		if (bComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}
		bInitialized = false;
		return true;
	}

	bool FUnrealAudioXAudio2::GetDevicePlatformApi(EDeviceApi::Type& OutType) const
	{
		OutType = EDeviceApi::XAUDIO2;
		return true;
	}

	bool FUnrealAudioXAudio2::GetNumOutputDevices(uint32& OutNumDevices) const
	{
		if (!bInitialized)
		{
			return false;
		}

		HRESULT Result = XAudio2Info.XAudio2System->GetDeviceCount(&OutNumDevices);
		RETURN_FALSE_ON_FAIL(Result);
		return true;
	}

	void FUnrealAudioXAudio2::GetDeviceInfoFromFormat(const WAVEFORMATEX* WaveFormatEx, FDeviceInfo& DeviceInfo) const
	{
		check(WaveFormatEx);
		DeviceInfo.FrameRate = WaveFormatEx->nSamplesPerSec;
		DeviceInfo.NumChannels = WaveFormatEx->nChannels;

		// XAudio2 supports converting formats to float so we don't need to do buffer conversion ourselves
		DeviceInfo.StreamFormat = EStreamFormat::FLT;

		if (WaveFormatEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			// Build a flag field of the speaker outputs of this device
			WAVEFORMATEXTENSIBLE* WaveFormatExtensible = (WAVEFORMATEXTENSIBLE*)WaveFormatEx;
			DeviceInfo.Speakers.Reset();
			uint32 ChanCount = 0;
			for (uint32 SpeakerTypeIndex = 0; SpeakerTypeIndex < ESpeaker::SPEAKER_TYPE_COUNT && ChanCount < DeviceInfo.NumChannels; ++SpeakerTypeIndex)
			{
				if (WaveFormatExtensible->dwChannelMask & SpeakerMaskMapping[SpeakerTypeIndex].XAudio2SpeakerTypeFlag)
				{
					++ChanCount;
					DeviceInfo.Speakers.Add(SpeakerMaskMapping[SpeakerTypeIndex].UnrealSpeaker);
				}
			}
			check(ChanCount == DeviceInfo.NumChannels);
		}
		else
		{
			// Non-extensible formats only support 1 or 2 channels
			DeviceInfo.Speakers.Add(ESpeaker::FRONT_LEFT);
			if (DeviceInfo.NumChannels == 2)
			{
				DeviceInfo.Speakers.Add(ESpeaker::FRONT_RIGHT);
			}
		}
	}

	bool FUnrealAudioXAudio2::GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& DeviceInfo) const
	{
		if (!bInitialized)
		{
			return false;
		}

		XAUDIO2_DEVICE_DETAILS DeviceDetails;
		HRESULT Result = XAudio2Info.XAudio2System->GetDeviceDetails(DeviceIndex, &DeviceDetails);
		RETURN_FALSE_ON_FAIL(Result);

		DeviceInfo.FriendlyName = FString(DeviceDetails.DisplayName);
		DeviceInfo.bIsSystemDefault = (DeviceIndex == 0);

		GetDeviceInfoFromFormat(&DeviceDetails.OutputFormat.Format, DeviceInfo);
		return true;
	}

	bool FUnrealAudioXAudio2::GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const
	{
		OutDefaultIndex = 0;
		return true;
	}

	bool FUnrealAudioXAudio2::OpenDevice(const FCreateStreamParams& CreateStreamParams)
	{
		if (!bInitialized || XAudio2Info.bDeviceOpen)
		{
			return false;
		}
		check(CreateStreamParams.OutputDeviceIndex != INDEX_NONE);
		check(XAudio2Info.XAudio2System && XAudio2Info.MasteringVoice == nullptr);

		StreamInfo.BlockSize = CreateStreamParams.CallbackBlockSize;
		StreamInfo.FrameRate = 44100;

		HRESULT Result = S_OK;

		FDeviceInfo DeviceInfo;
		if (!GetOutputDeviceInfo(CreateStreamParams.OutputDeviceIndex, DeviceInfo))
		{
			Result = S_FALSE;
			goto Cleanup;
		}

		Result = XAudio2Info.XAudio2System->CreateMasteringVoice(&XAudio2Info.MasteringVoice, DeviceInfo.NumChannels, StreamInfo.FrameRate, 0, CreateStreamParams.OutputDeviceIndex, nullptr);
		CLEANUP_ON_FAIL(Result);

		{
			FStreamDeviceInfo& StreamDeviceInfo = StreamInfo.DeviceInfo;
			StreamDeviceInfo.DeviceIndex = CreateStreamParams.OutputDeviceIndex;
			StreamDeviceInfo.bPerformByteSwap = false;
			StreamDeviceInfo.NumChannels = DeviceInfo.NumChannels;
			StreamDeviceInfo.DeviceDataFormat = DeviceInfo.StreamFormat;
			StreamDeviceInfo.FrameRate = DeviceInfo.FrameRate;
			StreamDeviceInfo.Speakers = DeviceInfo.Speakers;

			uint32 BufferSize = StreamDeviceInfo.NumChannels * StreamInfo.BlockSize * GetNumBytesForFormat(EStreamFormat::FLT);
			StreamDeviceInfo.UserBuffer.Init(0, BufferSize);
			XAudio2Info.bDeviceOpen = true;
			XAudio2Info.XAudio2System->StartEngine();
		}

	Cleanup:
		if (FAILED(Result))
		{
			ShutdownStream();
		}
		return SUCCEEDED(Result);
	}

	bool FUnrealAudioXAudio2::StartStream()
	{
		check(StreamInfo.Thread == nullptr);
		StreamInfo.Thread = FRunnableThread::Create(this, TEXT("XAudio2DeviceThread"), 0, TPri_AboveNormal);
		return true;
	}

	bool FUnrealAudioXAudio2::StopStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::SHUTDOWN || StreamInfo.State == EStreamState::STOPPED || StreamInfo.State == EStreamState::STOPPING)
		{
			return false;
		}
		check(StreamInfo.Thread != nullptr);
		check(XAudio2Info.XAudio2System != nullptr);

		// Tell the stream update that we are stopping
		Stop();

		// Wait for the thread to finish
		StreamInfo.Thread->WaitForCompletion();

		check(StreamInfo.State == EStreamState::STOPPED);

		// delete the thread
		delete StreamInfo.Thread;
		StreamInfo.Thread = nullptr;

		return true;
	}

	bool FUnrealAudioXAudio2::ShutdownStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::SHUTDOWN)
		{
			return false;
		}

		if (StreamInfo.State != EStreamState::STOPPED)
		{
			StopStream();
		}

		XAudio2Info.XAudio2System->StopEngine();
		XAudio2Info.OutputStreamSourceVoice->DestroyVoice();
		XAudio2Info.OutputStreamSourceVoice = nullptr;
		XAudio2Info.MasteringVoice->DestroyVoice();
		XAudio2Info.MasteringVoice = nullptr;
		XAudio2Info.bDeviceOpen = false;

		CloseHandle(XAudio2Info.OutputBufferEndEvent);
		XAudio2Info.OutputBufferEndEvent = nullptr;

		StreamInfo.State = EStreamState::SHUTDOWN;
		return true;
	}

	bool FUnrealAudioXAudio2::GetLatency(uint32& OutputDeviceLatency) const
	{
		OutputDeviceLatency = StreamInfo.DeviceInfo.Latency;
		return true;
	}

	bool FUnrealAudioXAudio2::GetFrameRate(uint32& OutFrameRate) const
	{
		OutFrameRate = StreamInfo.FrameRate;
		return true;
	}

	uint32 FUnrealAudioXAudio2::Run()
	{
		bool bThreadComInitialized = FWindowsPlatformMisc::CoInitialize();

		FCallbackInfo CallbackInfo;

		FStreamDeviceInfo& OutputDeviceInfo = StreamInfo.DeviceInfo;
		uint32 OutputDeviceSamples			= StreamInfo.BlockSize * OutputDeviceInfo.NumChannels;
		HRESULT Result						= S_OK;
		WAVEFORMATEX Format					= { 0 };

		check(XAudio2Info.OutputBufferEndEvent == nullptr);
		XAudio2Info.OutputBufferEndEvent = CreateEvent(0, false, false, nullptr);
		if (!XAudio2Info.OutputBufferEndEvent)
		{
			Result = HRESULT_FROM_WIN32(GetLastError());
			CLEANUP_ON_FAIL(Result);
		}

		XAudio2Info.OutputVoiceCallback.SetBufferEndEvent(XAudio2Info.OutputBufferEndEvent);

		Format.nChannels		= OutputDeviceInfo.NumChannels;
		Format.nSamplesPerSec	= StreamInfo.FrameRate;
		Format.wFormatTag		= WAVE_FORMAT_IEEE_FLOAT;
		Format.nAvgBytesPerSec	= Format.nSamplesPerSec * sizeof(float) * Format.nChannels;
		Format.nBlockAlign		= sizeof(float) * Format.nChannels;
		Format.wBitsPerSample	= sizeof(float) * 8;

		Result = XAudio2Info.XAudio2System->CreateSourceVoice(
			&XAudio2Info.OutputStreamSourceVoice,
			&Format,
			XAUDIO2_VOICE_NOSRC | XAUDIO2_VOICE_NOPITCH,
			2.0f,
			&XAudio2Info.OutputVoiceCallback
		);
		CLEANUP_ON_FAIL(Result);

		XAudio2Info.OutputBuffers.Empty();
		for (int32 Index = 0; Index < XAudio2Info.MaxQueuedBuffers; ++Index)
		{
			XAudio2Info.OutputBuffers.Add(FOutputBuffer(OutputDeviceSamples));
		}

		// Setup the callback info struct
		CallbackInfo.OutBuffer		= nullptr;
		CallbackInfo.NumFrames		= StreamInfo.BlockSize;
		CallbackInfo.NumChannels	= OutputDeviceInfo.NumChannels;
		CallbackInfo.StreamTime		= 0.0;
		CallbackInfo.UserData		= StreamInfo.UserData;
		CallbackInfo.StatusFlags	= 0;
		CallbackInfo.OutputSpeakers = OutputDeviceInfo.Speakers;
		CallbackInfo.FrameRate		= StreamInfo.FrameRate;

		check(XAudio2Info.OutputStreamSourceVoice);
		XAudio2Info.OutputStreamSourceVoice->Start();

		StreamInfo.State = EStreamState::RUNNING;

		while (StreamInfo.State != EStreamState::STOPPING)
		{
			CallbackInfo.StatusFlags = 0;
			CallbackInfo.StreamTime = StreamInfo.StreamTime;
			CallbackInfo.OutBuffer = XAudio2Info.OutputBuffers[XAudio2Info.CurrentBufferIndex].Buffer.GetData();

#if UNREAL_AUDIO_TEST_WHITE_NOISE
			uint32 NumSamples = StreamInfo.BlockSize*OutputDeviceInfo.NumChannels;
			for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
			{
				CallbackInfo.OutBuffer[Sample] = 0.5f * FMath::FRandRange(-1.0f, 1.0f);
			}
#else // #if UNREAL_AUDIO_TEST_WHITE_NOISE
			memset((void*)CallbackInfo.OutBuffer, 0, StreamInfo.BlockSize*OutputDeviceInfo.NumChannels*sizeof(float));
			if (!StreamInfo.CallbackFunction(CallbackInfo))
			{
				StreamInfo.State = EStreamState::STOPPING;
			}
#endif // #else // #if UNREAL_AUDIO_TEST_WHITE_NOISE

			UpdateStreamTimeTick();

			// If we read input but our xaudio2 voice has the max number of voices queued to playback, 
			// then wait for the next buffer to finish playing out before writing into it new buffers
			XAUDIO2_VOICE_STATE OutputVoiceState;
			XAudio2Info.OutputStreamSourceVoice->GetState(&OutputVoiceState);
			if (OutputVoiceState.BuffersQueued >= ((uint32)XAudio2Info.MaxQueuedBuffers - 1))
			{
				WaitForSingleObject(XAudio2Info.OutputBufferEndEvent, INFINITE);
			}

			float* Buffer = XAudio2Info.OutputBuffers[XAudio2Info.CurrentBufferIndex].Buffer.GetData();
			// Write out the last user-buffer to the device
			XAUDIO2_BUFFER SubmitBuffer = { 0 };
			SubmitBuffer.AudioBytes = OutputDeviceSamples * sizeof(float);
			SubmitBuffer.pAudioData = (const BYTE*)Buffer;
			XAudio2Info.OutputStreamSourceVoice->SubmitSourceBuffer(&SubmitBuffer);

			// Update the current buffer index (the last buffer is currently being written to device)
			XAudio2Info.CurrentBufferIndex = (XAudio2Info.CurrentBufferIndex + 1) % XAudio2Info.MaxQueuedBuffers;
		}

		XAudio2Info.OutputStreamSourceVoice->Stop();
		XAudio2Info.OutputStreamSourceVoice->FlushSourceBuffers();

	Cleanup:
		if (bThreadComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}

		StreamInfo.State = EStreamState::STOPPED;
		return 0;
	}

	void FUnrealAudioXAudio2::Stop()
	{
		if (StreamInfo.State != EStreamState::STOPPED)
		{
			StreamInfo.State = EStreamState::STOPPING;
		}
	}
}

IMPLEMENT_MODULE(UAudio::FUnrealAudioXAudio2, UnrealAudioXAudio2);

#endif // #if ENABLE_UNREAL_AUDIO
