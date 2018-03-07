// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of IAudioDevice for WASAPI (Windows Audio Session API)

	See https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455%28v=vs.85%29.aspx

*/

#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioBuffer.h"

#include "AllowWindowsPlatformTypes.h"
#include <Audioclient.h>					// WASAPI api
#include <avrt.h>							// This module contains the multimedia class scheduler APIs and any public data structures needed to call these APIs.
#include <mmdeviceapi.h>					// For getting endpoint notifications about audio devices
#include <functiondiscoverykeys_devpkey.h>	// Defines property keys for the Plug and Play Device Property API.
#include "HideWindowsPlatformTypes.h"

#if ENABLE_UNREAL_AUDIO

// See MSDN documentation for what these error codes mean in the context of the API call
static const TCHAR* GetWasapiError(HRESULT Result)
{
	switch (Result)
	{
		case AUDCLNT_E_NOT_INITIALIZED:					return TEXT("AUDCLNT_E_NOT_INITIALIZED");
		case AUDCLNT_E_ALREADY_INITIALIZED:				return TEXT("AUDCLNT_E_ALREADY_INITIALIZED");
		case AUDCLNT_E_WRONG_ENDPOINT_TYPE:				return TEXT("AUDCLNT_E_WRONG_ENDPOINT_TYPE");
		case AUDCLNT_E_DEVICE_INVALIDATED:				return TEXT("AUDCLNT_E_DEVICE_INVALIDATED");
		case AUDCLNT_E_NOT_STOPPED:						return TEXT("AUDCLNT_E_NOT_STOPPED");
		case AUDCLNT_E_BUFFER_TOO_LARGE:				return TEXT("AUDCLNT_E_BUFFER_TOO_LARGE");
		case AUDCLNT_E_OUT_OF_ORDER:					return TEXT("AUDCLNT_E_OUT_OF_ORDER");
		case AUDCLNT_E_UNSUPPORTED_FORMAT:				return TEXT("AUDCLNT_E_UNSUPPORTED_FORMAT");
		case AUDCLNT_E_INVALID_SIZE:					return TEXT("AUDCLNT_E_INVALID_SIZE");
		case AUDCLNT_E_DEVICE_IN_USE:					return TEXT("AUDCLNT_E_DEVICE_IN_USE");
		case AUDCLNT_E_BUFFER_OPERATION_PENDING:		return TEXT("AUDCLNT_E_BUFFER_OPERATION_PENDING");
		case AUDCLNT_E_THREAD_NOT_REGISTERED:			return TEXT("AUDCLNT_E_THREAD_NOT_REGISTERED");
		case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:		return TEXT("AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED");
		case AUDCLNT_E_ENDPOINT_CREATE_FAILED:			return TEXT("AUDCLNT_E_ENDPOINT_CREATE_FAILED");
		case AUDCLNT_E_SERVICE_NOT_RUNNING:				return TEXT("AUDCLNT_E_SERVICE_NOT_RUNNING");
		case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED:		return TEXT("AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED");
		case AUDCLNT_E_EXCLUSIVE_MODE_ONLY:				return TEXT("AUDCLNT_E_EXCLUSIVE_MODE_ONLY");
		case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:	return TEXT("AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL");
		case AUDCLNT_E_EVENTHANDLE_NOT_SET:				return TEXT("AUDCLNT_E_EVENTHANDLE_NOT_SET");
		case AUDCLNT_E_INCORRECT_BUFFER_SIZE:			return TEXT("AUDCLNT_E_INCORRECT_BUFFER_SIZE");
		case AUDCLNT_E_BUFFER_SIZE_ERROR:				return TEXT("AUDCLNT_E_BUFFER_SIZE_ERROR");
		case AUDCLNT_E_CPUUSAGE_EXCEEDED:				return TEXT("AUDCLNT_E_CPUUSAGE_EXCEEDED");
		case AUDCLNT_E_BUFFER_ERROR:					return TEXT("AUDCLNT_E_BUFFER_ERROR");
		case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:			return TEXT("AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED");
		case AUDCLNT_E_INVALID_DEVICE_PERIOD:			return TEXT("AUDCLNT_E_INVALID_DEVICE_PERIOD");
		case AUDCLNT_E_INVALID_STREAM_FLAG:				return TEXT("AUDCLNT_E_INVALID_STREAM_FLAG");
		case AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE:	return TEXT("AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE");
		case AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES:		return TEXT("AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES");
		case AUDCLNT_E_OFFLOAD_MODE_ONLY:				return TEXT("AUDCLNT_E_OFFLOAD_MODE_ONLY");
		case AUDCLNT_E_NONOFFLOAD_MODE_ONLY:			return TEXT("AUDCLNT_E_NONOFFLOAD_MODE_ONLY");
		case AUDCLNT_E_RESOURCES_INVALIDATED:			return TEXT("AUDCLNT_E_RESOURCES_INVALIDATED");
		case AUDCLNT_E_RAW_MODE_UNSUPPORTED:			return TEXT("AUDCLNT_E_RAW_MODE_UNSUPPORTED");
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
		const TCHAR* Err = GetWasapiError(Result);	\
		UA_DEVICE_PLATFORM_ERROR(Err);				\
		goto Cleanup;								\
	}

#define RETURN_FALSE_ON_FAIL(Result)				\
	if (FAILED(Result))								\
	{												\
		const TCHAR* Err = GetWasapiError(Result);	\
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
		{ ESpeaker::FRONT_LEFT,				SPEAKER_FRONT_LEFT				},
		{ ESpeaker::FRONT_RIGHT,			SPEAKER_FRONT_RIGHT				},
		{ ESpeaker::FRONT_CENTER,			SPEAKER_FRONT_CENTER			},
		{ ESpeaker::LOW_FREQUENCY,			SPEAKER_LOW_FREQUENCY			},
		{ ESpeaker::BACK_LEFT,				SPEAKER_BACK_LEFT				},
		{ ESpeaker::BACK_RIGHT,				SPEAKER_BACK_RIGHT				},
		{ ESpeaker::FRONT_LEFT_OF_CENTER,	SPEAKER_FRONT_LEFT_OF_CENTER	},
		{ ESpeaker::FRONT_RIGHT_OF_CENTER,	SPEAKER_FRONT_RIGHT_OF_CENTER	},
		{ ESpeaker::BACK_CENTER,			SPEAKER_BACK_CENTER				},
		{ ESpeaker::SIDE_LEFT,				SPEAKER_SIDE_LEFT				},
		{ ESpeaker::SIDE_RIGHT,				SPEAKER_SIDE_RIGHT				},
		{ ESpeaker::TOP_CENTER,				SPEAKER_TOP_CENTER				},
		{ ESpeaker::TOP_FRONT_LEFT,			SPEAKER_TOP_FRONT_LEFT			},
		{ ESpeaker::TOP_FRONT_CENTER,		SPEAKER_TOP_FRONT_CENTER		},
		{ ESpeaker::TOP_FRONT_RIGHT,		SPEAKER_TOP_FRONT_RIGHT			},
		{ ESpeaker::TOP_BACK_LEFT,			SPEAKER_TOP_BACK_LEFT			},
		{ ESpeaker::TOP_BACK_CENTER,		SPEAKER_TOP_BACK_CENTER			},
		{ ESpeaker::TOP_BACK_RIGHT,			SPEAKER_TOP_BACK_RIGHT			},
	};

	/** Helper function which turns a windows speaker flag mask into an array of speaker types. */
	static void GetArrayOfSpeakers(const uint32 NumChannels, TArray<ESpeaker::Type>& Speakers, const ::DWORD ChannelMask)
	{
		Speakers.Reset();
		uint32 ChanCount = 0;
		// Build a flag field of the speaker outputs of this device
		for (uint32 SpeakerTypeIndex = 0; SpeakerTypeIndex < ESpeaker::SPEAKER_TYPE_COUNT && ChanCount < NumChannels; ++SpeakerTypeIndex)
		{
			if (ChannelMask & SpeakerMaskMapping[SpeakerTypeIndex].XAudio2SpeakerTypeFlag)
			{
				++ChanCount;
				Speakers.Add(SpeakerMaskMapping[SpeakerTypeIndex].UnrealSpeaker);
			}
		}

		check(ChanCount == NumChannels);
	}

	/**
	* FUnrealAudioWasapi
	* Wasapi implementation of IUnrealAudioDeviceModule
	* Inherits from FRunnable so that audio device I/O can be run on separate thread.
	*/
	class FUnrealAudioWasapi :	public IUnrealAudioDeviceModule, 
								public FRunnable
	{
	public:
		FUnrealAudioWasapi();
		~FUnrealAudioWasapi();

		// IUnrealAudioDeviceModule
		bool Initialize() override;
		bool Shutdown() override;
		bool GetDevicePlatformApi(EDeviceApi::Type & OutType) const override;
		bool GetNumOutputDevices(uint32& OutNumDevices) const override;
		bool GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& OutInfo) const override;
		bool GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const override;
		bool StartStream() override;
		bool StopStream() override;
		bool ShutdownStream() override;
		bool GetLatency(uint32& OutputDeviceLatency) const override;
		bool GetFrameRate(uint32& OutFrameRate) const override;

		// FRunnable
		uint32 Run() override;
		void Stop() override;
		void Exit() override;

	private:

		/**
		* FWasapiInfo
		*
		* Structure for holding WASAPI specific resources
		*
		*/
		struct FWasapiInfo
		{
			/** The windows device enumerator. Used to query connected audio devices. */
			IMMDeviceEnumerator* DeviceEnumerator;

			/** Allows creation and initialization of audio output stream from audio engine and hardware buffer of audio endpoint device*/
			IAudioClient* RenderClient;

			/** Allows writing data output to capture endpoint buffer */
			IAudioRenderClient* RenderService;

			/** Handle used to notify when hardware is ready for new audio data to be written to it*/
			HANDLE RenderEvent;

			/** Intermediate buffer used to store audio data from user callback, converted to hardware native format*/
			IIntermediateBuffer* RenderIntermediateBuffer;

			/** Whether or not devices are open */
			bool bDevicesOpen;

			FWasapiInfo()
				: DeviceEnumerator(nullptr)
				, RenderClient(nullptr)
				, RenderService(nullptr)
				, RenderEvent(0)
				, RenderIntermediateBuffer(nullptr)
				, bDevicesOpen(false)
			{
			}
		};

		/** The number of active output devices detected by WASAPI */
		uint32 OutputDeviceCount;

		/** WASAPI-specific data */
		FWasapiInfo WasapiInfo;

		/** Whether or not the device api has been initialized */
		bool bInitialized;

		/** Whether or not com was initialized */
		bool bComInitialized;

	private:

		// IUnrealAudioDeviceModule
		bool OpenDevice(const FCreateStreamParams& CreateStreamParams) override;

		// Helper functions
		bool OpenDevice(uint32 DeviceIndex);
		bool GetDeviceCount(uint32& NumDevices) const;
		bool GetDeviceInfo(const uint32 DeviceINdex, FDeviceInfo& DeviceInfo) const;
		bool GetDefaultDeviceIndex(uint32& OutDeviceIndex) const;
		void GetDeviceBufferSize(uint32& DeviceBufferSize);
	};

	/** 
	* FUnrealAudioWasapi Implementation
	*/

	FUnrealAudioWasapi::FUnrealAudioWasapi()
		: OutputDeviceCount(0)
		, bInitialized(false)
		, bComInitialized(false)
	{
	}

	FUnrealAudioWasapi::~FUnrealAudioWasapi()
	{
	}

	bool FUnrealAudioWasapi::Initialize()
	{
 		bComInitialized = FWindowsPlatformMisc::CoInitialize();

		HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&WasapiInfo.DeviceEnumerator));
		RETURN_FALSE_ON_FAIL(Result);

		bInitialized = true;
		return true;
	}

	bool FUnrealAudioWasapi::Shutdown()
	{
		if (!bInitialized)
		{
			return false;
		}

		SAFE_RELEASE(WasapiInfo.DeviceEnumerator);

		if (bComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}
		return true;
	}

	bool FUnrealAudioWasapi::GetDevicePlatformApi(EDeviceApi::Type& OutType) const
	{
		OutType = EDeviceApi::WASAPI;
		return true;
	}

	bool FUnrealAudioWasapi::GetNumOutputDevices(uint32& OutNumDevices) const
	{
		return GetDeviceCount(OutNumDevices);
	}

	bool FUnrealAudioWasapi::GetDeviceCount(uint32& OutNumDevices) const
	{
		if (!bInitialized || !WasapiInfo.DeviceEnumerator)
		{
			return false;
		}

		IMMDeviceCollection* Devices = nullptr;
		HRESULT Result = S_OK;

		Result = WasapiInfo.DeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &Devices);
		CLEANUP_ON_FAIL(Result);

		Result = Devices->GetCount(&OutNumDevices);
		CLEANUP_ON_FAIL(Result);

	Cleanup:
		SAFE_RELEASE(Devices);
		return SUCCEEDED(Result);
	}

	bool FUnrealAudioWasapi::GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& OutInfo) const
	{
		return GetDeviceInfo(DeviceIndex, OutInfo);
	}

	bool FUnrealAudioWasapi::GetDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& DeviceInfo) const
	{
		if (!bInitialized || !WasapiInfo.DeviceEnumerator)
		{
			return false;
		}

		IMMDeviceCollection* Devices = nullptr;
		IMMDevice* DefaultDevice = nullptr;
		IMMDevice* Device = nullptr;
		IPropertyStore* DevicePropertyStore = nullptr;
		IPropertyStore* DefaultDevicePropertyStore = nullptr;
		IAudioClient* WasapiAudioClient = nullptr;
		PROPVARIANT DeviceNameProperty;
		PROPVARIANT DefaultDeviceNameProperty;
		WAVEFORMATEX* WaveFormatEx = nullptr;
		HRESULT Result = S_OK;

		Result = WasapiInfo.DeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &Devices);
		CLEANUP_ON_FAIL(Result);

		uint32 NumDevices = 0;
		Result = Devices->GetCount(&NumDevices);
		CLEANUP_ON_FAIL(Result);

		Result = WasapiInfo.DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &DefaultDevice);
		CLEANUP_ON_FAIL(Result);

		Result = Devices->Item(DeviceIndex, &Device);
		CLEANUP_ON_FAIL(Result);

		Result = Device->OpenPropertyStore(STGM_READ, &DevicePropertyStore);
		CLEANUP_ON_FAIL(Result);

		Result = DefaultDevice->OpenPropertyStore(STGM_READ, &DefaultDevicePropertyStore);
		CLEANUP_ON_FAIL(Result);

		Result = DevicePropertyStore->GetValue(PKEY_Device_FriendlyName, &DeviceNameProperty);
		CLEANUP_ON_FAIL(Result);

		Result = DefaultDevicePropertyStore->GetValue(PKEY_Device_FriendlyName, &DefaultDeviceNameProperty);
		CLEANUP_ON_FAIL(Result);

		Result = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&WasapiAudioClient);
		CLEANUP_ON_FAIL(Result);

		Result = WasapiAudioClient->GetMixFormat(&WaveFormatEx);
		CLEANUP_ON_FAIL(Result);

		// At this point we've succeeded in getting all the device objects from WindowsMM that we need
		DeviceInfo.FriendlyName = FString(DeviceNameProperty.pwszVal);
		DeviceInfo.bIsSystemDefault = (DeviceInfo.FriendlyName == FString(DefaultDeviceNameProperty.pwszVal));
		DeviceInfo.NumChannels = WaveFormatEx->nChannels;
		DeviceInfo.FrameRate = WaveFormatEx->nSamplesPerSec;

		// Figure out native supported data formats of the device
		DeviceInfo.StreamFormat = EStreamFormat::UNSUPPORTED;

		bool bIsFloat = (WaveFormatEx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
		bool bIsExtensible = (WaveFormatEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
		bool bIsSubtypeFloat = (bIsExtensible && (((WAVEFORMATEXTENSIBLE*)WaveFormatEx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

		if (bIsFloat || bIsSubtypeFloat)
		{
			if (WaveFormatEx->wBitsPerSample == 32)
			{
				DeviceInfo.StreamFormat = EStreamFormat::FLT;
			}
			else if (WaveFormatEx->wBitsPerSample == 64)
			{
				DeviceInfo.StreamFormat = EStreamFormat::DBL;
			}
		}
		else
		{
			bool bIsPcm = (WaveFormatEx->wFormatTag == WAVE_FORMAT_PCM);
			bool bIsSubtypePcm = (bIsExtensible && (((WAVEFORMATEXTENSIBLE*)WaveFormatEx)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM));
			if (bIsPcm || bIsSubtypePcm)
			{
				if (WaveFormatEx->wBitsPerSample == 16)
				{
					DeviceInfo.StreamFormat = EStreamFormat::INT_16;
				}
				else if (WaveFormatEx->wBitsPerSample == 24)
				{
					DeviceInfo.StreamFormat = EStreamFormat::INT_24;
				}
				else if (WaveFormatEx->wBitsPerSample == 32)
				{
					DeviceInfo.StreamFormat = EStreamFormat::INT_32;
				}
				else
				{
					goto Cleanup;
				}
			}
		}

		DeviceInfo.Speakers.Reset();
		uint32 ChanCount = 0;
		if (bIsExtensible)
		{
			// Build a flag field of the speaker outputs of this device
			WAVEFORMATEXTENSIBLE* WaveFormatExtensible = (WAVEFORMATEXTENSIBLE*)WaveFormatEx;
			GetArrayOfSpeakers(DeviceInfo.NumChannels, DeviceInfo.Speakers, WaveFormatExtensible->dwChannelMask);
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

Cleanup:
		// Release all our resources that we used above
		SAFE_RELEASE(Devices);
		SAFE_RELEASE(DefaultDevice);
		SAFE_RELEASE(Device);
		SAFE_RELEASE(DevicePropertyStore);
		SAFE_RELEASE(DefaultDevicePropertyStore);

		PropVariantClear(&DeviceNameProperty);
		PropVariantClear(&DefaultDeviceNameProperty);

		CoTaskMemFree(WaveFormatEx);

		return SUCCEEDED(Result);
	}

	bool FUnrealAudioWasapi::GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const
	{
		return GetDefaultDeviceIndex(OutDefaultIndex);
	}

	bool FUnrealAudioWasapi::GetDefaultDeviceIndex(uint32& OutDeviceIndex) const
	{
		uint32 NumDevices = 0;
		if (!GetDeviceCount(NumDevices))
		{
			return false;
		}

		bool bFoundDeviceIndex = false;

		for (uint32 DeviceIndex = 0; DeviceIndex < NumDevices; ++DeviceIndex)
		{
			FDeviceInfo DeviceInfo;
			if (!GetDeviceInfo(DeviceIndex, DeviceInfo))
			{
				return false;
			}

			if (DeviceInfo.bIsSystemDefault)
			{
				bFoundDeviceIndex = true;
				OutDeviceIndex = DeviceIndex;
				break;
			}
		}

		return bFoundDeviceIndex;
	}

	bool FUnrealAudioWasapi::OpenDevice(const FCreateStreamParams& CreateStreamParams)
	{
		if (!bInitialized || WasapiInfo.bDevicesOpen)
		{
			return false;
		}

		StreamInfo.BlockSize = CreateStreamParams.CallbackBlockSize;

		StreamInfo.FrameRate = INDEX_NONE;

		check(CreateStreamParams.OutputDeviceIndex != INDEX_NONE);
		if (!OpenDevice(CreateStreamParams.OutputDeviceIndex))
		{
			return false;
		}
		StreamInfo.FrameRate = FMath::Min(StreamInfo.FrameRate, StreamInfo.DeviceInfo.FrameRate);
		return true;
	}

	bool FUnrealAudioWasapi::OpenDevice(uint32 DeviceIndex)
	{
		check(WasapiInfo.DeviceEnumerator);

		IMMDevice* Device = nullptr;
		IMMDeviceCollection* DeviceList = nullptr;
		WAVEFORMATEX* DeviceFormat = nullptr;
		FDeviceInfo DeviceInfo;
		HRESULT Result = S_OK;

		Result = WasapiInfo.DeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &DeviceList);
		CLEANUP_ON_FAIL(Result);

		if (!GetDeviceInfo(DeviceIndex, DeviceInfo))
		{
			Result = S_FALSE;
			goto Cleanup;
		}

		Result = DeviceList->Item(DeviceIndex, &Device);
		CLEANUP_ON_FAIL(Result);

		FStreamDeviceInfo& StreamDeviceInfo = StreamInfo.DeviceInfo;

		StreamDeviceInfo.DeviceIndex = DeviceIndex;

		IAudioClient*& AudioClient = WasapiInfo.RenderClient;
		check(AudioClient == nullptr);

		Result = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&AudioClient);
		CLEANUP_ON_FAIL(Result);

		Result = AudioClient->GetMixFormat(&DeviceFormat);
		CLEANUP_ON_FAIL(Result);

		StreamDeviceInfo.bPerformByteSwap = false;
		StreamDeviceInfo.NumChannels = DeviceFormat->nChannels;
		StreamDeviceInfo.DeviceDataFormat = DeviceInfo.StreamFormat;
		StreamDeviceInfo.FrameRate = DeviceInfo.FrameRate;
		StreamDeviceInfo.Speakers = DeviceInfo.Speakers;

		if (StreamDeviceInfo.DeviceDataFormat != EStreamFormat::FLT)
		{
			StreamDeviceInfo.bPerformFormatConversion = true;
		}
		SetupBufferFormatConvertInfo();

		// Initialize the raw buffer used to write into
		uint32 BufferSize = StreamDeviceInfo.NumChannels * StreamInfo.BlockSize * sizeof(float);
		StreamDeviceInfo.UserBuffer.Init(0, BufferSize);

	Cleanup:

		SAFE_RELEASE(DeviceList);
		SAFE_RELEASE(Device);
		CoTaskMemFree(DeviceFormat);

		if (FAILED(Result))
		{
			ShutdownStream();
		}

		return SUCCEEDED(Result);
	}

	bool FUnrealAudioWasapi::StartStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::CLOSED || StreamInfo.State == EStreamState::RUNNING)
		{
			return false;
		}
		check(StreamInfo.Thread == nullptr);
		StreamInfo.Thread = FRunnableThread::Create(this, TEXT("WasapiDeviceThread"), 0, TPri_AboveNormal);
		return true;
	}

	bool FUnrealAudioWasapi::StopStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::CLOSED || StreamInfo.State == EStreamState::STOPPED || StreamInfo.State == EStreamState::STOPPING)
		{
			return false;
		}

		check(StreamInfo.Thread != nullptr);
		check(WasapiInfo.RenderClient != nullptr);

		// Tell the stream update that we are stopping
		Stop();

		// Wait for the thread to finish
		StreamInfo.Thread->WaitForCompletion();

		check(StreamInfo.State == EStreamState::STOPPED);

		// delete the thread
		delete StreamInfo.Thread;
		StreamInfo.Thread = nullptr;

		// clean up any audio resources
		HRESULT Result = S_OK;
		bool bSuccess = true;

		if (WasapiInfo.RenderClient)
		{
			Result = WasapiInfo.RenderClient->Stop();
			if (FAILED(Result))
			{
				bSuccess = false;
				UA_DEVICE_PLATFORM_ERROR("Failed to stop render client.");
			}
		}

		return true;
	}

	bool FUnrealAudioWasapi::ShutdownStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::CLOSED)
		{
			return false;
		}

		if (StreamInfo.State != EStreamState::STOPPED)
		{
			StopStream();
		}

		SAFE_RELEASE(WasapiInfo.RenderClient);
		SAFE_RELEASE(WasapiInfo.RenderService);

		if (WasapiInfo.RenderEvent)
		{
			CloseHandle(WasapiInfo.RenderEvent);
			WasapiInfo.RenderEvent = nullptr;
		}

		StreamInfo.State = EStreamState::CLOSED;
		return true;
	}

	bool FUnrealAudioWasapi::GetLatency(uint32& OutputDeviceLatency) const
	{
		OutputDeviceLatency = StreamInfo.DeviceInfo.Latency;
		return true;
	}

	bool FUnrealAudioWasapi::GetFrameRate(uint32& OutFrameRate) const
	{
		OutFrameRate = StreamInfo.FrameRate;
		return true;
	}

	// FRunnable

	uint32 FUnrealAudioWasapi::Run()
	{
		// This is on a new thread, so lets initialize com again in case it wasn't already
		bool bThreadComInitialized = FWindowsPlatformMisc::CoInitialize();

		// Get local stack versions of various wasapi info we need for the device thread
		WAVEFORMATEX* OutputFormat = nullptr;
		BYTE* DeviceByteBuffer = nullptr;
		FStreamDeviceInfo& OutputDeviceInfo = StreamInfo.DeviceInfo;
		uint32 NumBytesRenderFormat = GetNumBytesForFormat(OutputDeviceInfo.DeviceDataFormat);
		uint32 OutputDeviceSamples = StreamInfo.BlockSize * OutputDeviceInfo.NumChannels;

		bool bWasUserCallbackMade = false;
		bool bWasUserBufferWritten = false;

		HRESULT Result = S_OK;
		FCallbackInfo CallbackInfo;

		StreamInfo.State = EStreamState::RUNNING;

		// Number used to compute the REFERENCE_TIME for callback periods (REF_TIME is 100 nanoseconds)
		static const uint64 REF_TIMES_PER_SECOND = 10000000;

		// We haven't set up an actual render service yet
		check(WasapiInfo.RenderService == nullptr);

		Result = WasapiInfo.RenderClient->GetMixFormat(&OutputFormat);
		CLEANUP_ON_FAIL(Result);

		// Compute the callback buffer period using the buffer size
		REFERENCE_TIME CallbackBufferPeriod = (REFERENCE_TIME)((StreamInfo.BlockSize * REF_TIMES_PER_SECOND) / OutputFormat->nSamplesPerSec);

		Result = WasapiInfo.RenderClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,				// Other clients can use this input device
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,		// Processing of the buffer by client is event driven
			CallbackBufferPeriod,					// the size of the buffer in 100-nanosecond units (REF_TIME)
			0,										// always 0 in shared mode 
			OutputFormat,							// the output format to use
			nullptr									// audio session guid, we're ignoring
		);
		CLEANUP_ON_FAIL(Result);

		// Get the output latency
		REFERENCE_TIME OutputLatency = 0;
		WasapiInfo.RenderClient->GetStreamLatency(&OutputLatency);
		OutputDeviceInfo.Latency = (1000 * OutputLatency) / REF_TIMES_PER_SECOND;

		Result = WasapiInfo.RenderClient->GetService(__uuidof(IAudioRenderClient), (void**)&WasapiInfo.RenderService);
		CLEANUP_ON_FAIL(Result);

		WasapiInfo.RenderEvent = CreateEvent(nullptr, false, false, nullptr);
		if (!WasapiInfo.RenderEvent)
		{
			Result = HRESULT_FROM_WIN32(GetLastError());
			CLEANUP_ON_FAIL(Result);
		}

		Result = WasapiInfo.RenderClient->SetEventHandle(WasapiInfo.RenderEvent);
		CLEANUP_ON_FAIL(Result);

		uint32 ReadBufferSize = 0;
		Result = WasapiInfo.RenderClient->GetBufferSize(&ReadBufferSize);
		CLEANUP_ON_FAIL(Result);

		uint32 WriteBufferSize = StreamInfo.BlockSize* OutputDeviceInfo.NumChannels;

		ReadBufferSize *= OutputDeviceInfo.NumChannels;

		WasapiInfo.RenderIntermediateBuffer = IIntermediateBuffer::CreateIntermediateBuffer(OutputDeviceInfo.DeviceDataFormat);
		check(WasapiInfo.RenderIntermediateBuffer);
		WasapiInfo.RenderIntermediateBuffer->Initialize(ReadBufferSize + WriteBufferSize);

		Result = WasapiInfo.RenderClient->Reset();
		CLEANUP_ON_FAIL(Result);

		Result = WasapiInfo.RenderClient->Start();
		CLEANUP_ON_FAIL(Result);

		// Set up the device buffer (buffer read from and written directly to the device in the native device format)

		// If we have an input stream, set up our device buffer size to accommodate
		uint32 DeviceBufferSize = 0;
		GetDeviceBufferSize(DeviceBufferSize);

		StreamInfo.DeviceBuffer.Init(0, DeviceBufferSize);
		uint32 Count = 0;

		// Prepare the struct which will be used to make audio callbacks
		CallbackInfo.OutBuffer = (float *)OutputDeviceInfo.UserBuffer.GetData();;
		CallbackInfo.NumFrames = StreamInfo.BlockSize;
		CallbackInfo.NumChannels = OutputDeviceInfo.NumChannels;
		CallbackInfo.StreamTime = 0.0;
		CallbackInfo.UserData = StreamInfo.UserData;
		CallbackInfo.StatusFlags = 0;
		CallbackInfo.OutputSpeakers = OutputDeviceInfo.Speakers;
		CallbackInfo.FrameRate = OutputDeviceInfo.FrameRate;

		while (StreamInfo.State != EStreamState::STOPPING)
		{
			if (!bWasUserCallbackMade)
			{
				CallbackInfo.StatusFlags = 0;
				CallbackInfo.StreamTime = StreamInfo.StreamTime;
				memset((void*)CallbackInfo.OutBuffer, 0, StreamInfo.BlockSize*OutputDeviceInfo.NumChannels*sizeof(float));
				if (!StreamInfo.CallbackFunction(CallbackInfo))
				{
					StreamInfo.State = EStreamState::STOPPING;
				}
				UpdateStreamTimeTick();
				bWasUserCallbackMade = true;
			}

			if (bWasUserCallbackMade)
			{
				if (OutputDeviceInfo.bPerformFormatConversion)
				{
					ConvertBufferFormat(StreamInfo.DeviceBuffer, OutputDeviceInfo.UserBuffer);
					bWasUserBufferWritten = WasapiInfo.RenderIntermediateBuffer->Write(StreamInfo.DeviceBuffer.GetData(), OutputDeviceSamples);
				}
				else
				{
					bWasUserBufferWritten = WasapiInfo.RenderIntermediateBuffer->Write(OutputDeviceInfo.UserBuffer.GetData(), OutputDeviceSamples);
				}
			}
			else
			{
				bWasUserBufferWritten = true;
			}

			// If the user output buffer was not pushed to the intermediate buffer, wait for the 
			// next render event from the device before going on
			if (bWasUserCallbackMade && !bWasUserBufferWritten)
			{
				WaitForSingleObject(WasapiInfo.RenderEvent, INFINITE);
			}

			uint32 OutputBufferFrameCount = 0;
			uint32 OutputBufferFramePadding = 0;

			// Get render buffer from stream
			Result = WasapiInfo.RenderClient->GetBufferSize(&OutputBufferFrameCount);
			CLEANUP_ON_FAIL(Result);

			Result = WasapiInfo.RenderClient->GetCurrentPadding(&OutputBufferFramePadding);
			CLEANUP_ON_FAIL(Result);

			OutputBufferFrameCount -= OutputBufferFramePadding;

			if (OutputBufferFrameCount != 0)
			{
				Result = WasapiInfo.RenderService->GetBuffer(OutputBufferFrameCount, &DeviceByteBuffer);
				CLEANUP_ON_FAIL(Result);

				// Read the next buffer from the intermediate output buffer
				uint32 ReadSamples = OutputBufferFrameCount * OutputDeviceInfo.NumChannels;
				bool bSuccess = WasapiInfo.RenderIntermediateBuffer->Read((uint8*)DeviceByteBuffer, ReadSamples);
				if (bSuccess)
				{
					Result = WasapiInfo.RenderService->ReleaseBuffer(OutputBufferFrameCount, 0);
					CLEANUP_ON_FAIL(Result);
				}
				else
				{
					Result = WasapiInfo.RenderService->ReleaseBuffer(0, 0);
					CLEANUP_ON_FAIL(Result);
				}
			}

			if (bWasUserBufferWritten)
			{
				bWasUserCallbackMade = false;
			}
		}

		Cleanup:

		CoTaskMemFree(OutputFormat);
		if (bThreadComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}
		StreamInfo.State = EStreamState::STOPPED;
		return 0;
	}

	void FUnrealAudioWasapi::Stop()
	{
		StreamInfo.State = EStreamState::STOPPING;
	}

	void FUnrealAudioWasapi::Exit()
	{

	}

	void FUnrealAudioWasapi::GetDeviceBufferSize(uint32& DeviceBufferSize)
	{
		uint32 NumBytes = GetNumBytesForFormat(StreamInfo.DeviceInfo.DeviceDataFormat);
		uint32 NumChannels = StreamInfo.DeviceInfo.NumChannels;
		DeviceBufferSize = StreamInfo.BlockSize * NumChannels * NumBytes;
	}
}

IMPLEMENT_MODULE(UAudio::FUnrealAudioWasapi, UnrealAudioWasapi);

#endif // #if ENABLE_UNREAL_AUDIO


