// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VoiceCaptureWindows.h"
#include "VoicePrivate.h"
#include "VoiceModule.h"

#if PLATFORM_SUPPORTS_VOICE_CAPTURE

#include "AllowWindowsPlatformTypes.h"

struct FVoiceCaptureWindowsVars
{
	/** GUID of current voice capture device */
	GUID VoiceCaptureDeviceGuid;
	/** Voice capture device */
	LPDIRECTSOUNDCAPTURE8 VoiceCaptureDev;
	/** Voice capture device caps */
	DSCCAPS VoiceCaptureDevCaps;
	/** Voice capture buffer */
	LPDIRECTSOUNDCAPTUREBUFFER8 VoiceCaptureBuffer8;
	/** Wave format of buffer */
	WAVEFORMATEX WavFormat;
	/** Buffer description */
	DSCBUFFERDESC VoiceCaptureBufferDesc;
	/** Buffer caps */
	DSCBCAPS VoiceCaptureBufferCaps8;
	/** Notification events */
	HANDLE StopEvent;
	/** Current audio position of valid data in capture buffer */
	DWORD NextCaptureOffset;

	FVoiceCaptureWindowsVars() :
		VoiceCaptureDev(nullptr),
		VoiceCaptureBuffer8(nullptr),
		NextCaptureOffset(0)
	{
		StopEvent = INVALID_HANDLE_VALUE;
		Reset();
	}

	void Reset()
	{
		if (StopEvent != INVALID_HANDLE_VALUE)
		{
			CloseHandle(StopEvent);
			StopEvent = INVALID_HANDLE_VALUE;
		}

		// Free up DirectSound resources
		if (VoiceCaptureBuffer8)
		{
			VoiceCaptureBuffer8->Release();
			VoiceCaptureBuffer8 = nullptr;
		}

		if (VoiceCaptureDev)
		{
			VoiceCaptureDev->Release();
			VoiceCaptureDev = nullptr;
		}

		NextCaptureOffset = 0;

		FMemory::Memzero(&VoiceCaptureDeviceGuid, sizeof(GUID));
		FMemory::Memzero(&VoiceCaptureDevCaps, sizeof(VoiceCaptureDevCaps));

		FMemory::Memzero(&WavFormat, sizeof(WavFormat));
		FMemory::Memzero(&VoiceCaptureBufferDesc, sizeof(VoiceCaptureBufferDesc));
		FMemory::Memzero(&VoiceCaptureBufferCaps8, sizeof(VoiceCaptureBufferCaps8));
	}
};

/** Calculate silence in an audio buffer by using a RMS threshold */
template<typename T>
bool IsSilence(T* Buffer, int32 BuffSize)
{
	if (BuffSize == 0)
	{
		return true;
	} 

	const int32 IterSize = BuffSize / sizeof(T);

	double Total = 0.0f;
	for (int32 i = 0; i < IterSize; i++) 
	{
		Total += Buffer[i];
	}

	const double Average = Total / IterSize;

	double SumMeanSquare = 0.0f;
	double Diff = 0.0f;
	for (int32 i = 0; i < IterSize; i++) 
	{
		Diff = (Buffer[i] - Average);
		SumMeanSquare += (Diff * Diff);
	}

	double AverageMeanSquare = SumMeanSquare / IterSize;

	static double Threshold = 75.0 * 75.0;
	return AverageMeanSquare < Threshold;
}

FVoiceCaptureWindows::FVoiceCaptureWindows() :
	CV(nullptr),
	LastCaptureTime(0.0),
	VoiceCaptureState(EVoiceCaptureState::UnInitialized)
{
	CV = new FVoiceCaptureWindowsVars();
}

FVoiceCaptureWindows::~FVoiceCaptureWindows()
{
	Shutdown();

	FVoiceCaptureDeviceWindows* VoiceCaptureDev = FVoiceCaptureDeviceWindows::Get();
 	if (VoiceCaptureDev)	
	{
 		VoiceCaptureDev->FreeVoiceCaptureObject(this);
 	}
	
	if (CV)
	{
		delete CV;
		CV = nullptr;
	}	
}

bool FVoiceCaptureWindows::Init(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	FVoiceCaptureDeviceWindows* VoiceDev = FVoiceCaptureDeviceWindows::Get();
	if (!VoiceDev)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("No voice capture interface."));
		return false;
	}
	
	if (DeviceName.IsEmpty())
	{
		return CreateCaptureBuffer(VoiceDev->DefaultVoiceCaptureDevice.DeviceName, SampleRate, NumChannels);
	}

	return CreateCaptureBuffer(DeviceName, SampleRate, NumChannels);
}

bool FVoiceCaptureWindows::CreateCaptureBuffer(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	// Free the previous buffer
	FreeCaptureBuffer();
	VoiceCaptureState = EVoiceCaptureState::NotCapturing;

	if (SampleRate < 8000 || SampleRate > 48000)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Voice capture doesn't support %d hz"), SampleRate);
		return false;
	}

	if (NumChannels < 0 || NumChannels > 2)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Voice capture only supports 1 or 2 channels"));
		return false;
	}

	FVoiceCaptureDeviceWindows* VoiceDev = FVoiceCaptureDeviceWindows::Get();
	if (!VoiceDev)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("No voice capture interface."));
		return false;
	}

	FVoiceCaptureDeviceWindows::FCaptureDeviceInfo* DeviceInfo = nullptr;
	if (DeviceName.IsEmpty())
	{
		DeviceInfo = VoiceDev->Devices.Find(DEFAULT_DEVICE_NAME);
	}
	else
	{
		DeviceInfo = VoiceDev->Devices.Find(DeviceName);
	}

	if (DeviceInfo)
	{
		UE_LOG(LogVoiceCapture, Display, TEXT("Creating capture %s [%d:%d]"), *DeviceInfo->DeviceName, SampleRate, NumChannels);
		CV->VoiceCaptureDeviceGuid = DeviceInfo->DeviceId;

		// DSDEVID_DefaultCapture WAVEINCAPS 
		HRESULT hr = DirectSoundCaptureCreate8(&DeviceInfo->DeviceId, &CV->VoiceCaptureDev, nullptr);
		if (FAILED(hr))
		{
			//DSERR_ALLOCATED, DSERR_INVALIDPARAM, DSERR_NOAGGREGATION, DSERR_OUTOFMEMORY
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create capture device 0x%08x"), hr);
			return false;
		}

		// Device capabilities
		CV->VoiceCaptureDevCaps.dwSize = sizeof(DSCCAPS);
		hr = CV->VoiceCaptureDev->GetCaps(&CV->VoiceCaptureDevCaps);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get mic device caps 0x%08x"), hr);
			return false;
		}

		// Wave format setup
		CV->WavFormat.wFormatTag = WAVE_FORMAT_PCM;
		CV->WavFormat.nChannels = NumChannels;
		CV->WavFormat.wBitsPerSample = 16;
		CV->WavFormat.nSamplesPerSec = SampleRate;
		CV->WavFormat.nBlockAlign = (CV->WavFormat.nChannels * CV->WavFormat.wBitsPerSample) / 8;
		CV->WavFormat.nAvgBytesPerSec = CV->WavFormat.nBlockAlign * CV->WavFormat.nSamplesPerSec;
		CV->WavFormat.cbSize = 0;

		// Buffer setup
		CV->VoiceCaptureBufferDesc.dwSize = sizeof(DSCBUFFERDESC);
		CV->VoiceCaptureBufferDesc.dwFlags = 0;
		CV->VoiceCaptureBufferDesc.dwBufferBytes = CV->WavFormat.nAvgBytesPerSec / 2; // .5 sec buffer
		CV->VoiceCaptureBufferDesc.dwReserved = 0;
		CV->VoiceCaptureBufferDesc.lpwfxFormat = &CV->WavFormat;
		CV->VoiceCaptureBufferDesc.dwFXCount = 0;
		CV->VoiceCaptureBufferDesc.lpDSCFXDesc = nullptr;

		LPDIRECTSOUNDCAPTUREBUFFER VoiceBuffer = nullptr;

		hr = CV->VoiceCaptureDev->CreateCaptureBuffer(&CV->VoiceCaptureBufferDesc, &VoiceBuffer, nullptr);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice capture buffer 0x%08x"), hr);
			return false;
		}

		hr = VoiceBuffer->QueryInterface(IID_IDirectSoundCaptureBuffer8, (LPVOID*)&CV->VoiceCaptureBuffer8);
		VoiceBuffer->Release();
		VoiceBuffer = nullptr;
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice capture buffer 0x%08x"), hr);
			return false;
		}

		CV->VoiceCaptureBufferCaps8.dwSize = sizeof(DSCBCAPS);
		hr = CV->VoiceCaptureBuffer8->GetCaps(&CV->VoiceCaptureBufferCaps8);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer caps 0x%08x"), hr);
			return false;
		}

		// TEST ------------------------
		if (0)
		{
			DWORD SizeWritten8 = 0;
			CV->VoiceCaptureBuffer8->GetFormat(nullptr, sizeof(WAVEFORMATEX), &SizeWritten8);

			LPWAVEFORMATEX BufferFormat8 = (WAVEFORMATEX*)FMemory::Malloc(SizeWritten8);
			CV->VoiceCaptureBuffer8->GetFormat(BufferFormat8, SizeWritten8, &SizeWritten8);
			FMemory::Free(BufferFormat8);
		}
		// TEST ------------------------

		if (CreateNotifications(CV->VoiceCaptureBufferCaps8.dwBufferBytes))
		{
			// Reset notification related values
			LastCaptureTime = FPlatformTime::Seconds();
		}
		else
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice buffer notifications"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("No voice capture device %s found."), *DeviceName);
		return false;
	}

	UncompressedAudioBuffer.Empty(CV->VoiceCaptureBufferDesc.dwBufferBytes);
	check(UncompressedAudioBuffer.Max() >= (int32)CV->VoiceCaptureBufferCaps8.dwBufferBytes);

	return true;
}

void FVoiceCaptureWindows::FreeCaptureBuffer()
{
	// Stop playback
	Stop();

	// Release all D3D8 resources
	CV->Reset();

	VoiceCaptureState = EVoiceCaptureState::UnInitialized;
}

void FVoiceCaptureWindows::Shutdown()
{
	FreeCaptureBuffer();
}

bool FVoiceCaptureWindows::Start()
{
	check(VoiceCaptureState != EVoiceCaptureState::UnInitialized);

	if (CV->VoiceCaptureBuffer8 == nullptr)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("CV->VoiceCaptureBuffer8 == nullptr"));
		return false;
	}

	HRESULT hr = CV->VoiceCaptureBuffer8->Start(DSCBSTART_LOOPING);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to start capture 0x%08x"), hr);
		return false;
	}

	VoiceCaptureState = EVoiceCaptureState::NoData;
	return true;
}

void FVoiceCaptureWindows::Stop()
{
	if (CV->VoiceCaptureBuffer8 &&
		VoiceCaptureState != EVoiceCaptureState::Stopping && 
		VoiceCaptureState != EVoiceCaptureState::NotCapturing)
	{
		CV->VoiceCaptureBuffer8->Stop();
		VoiceCaptureState = EVoiceCaptureState::Stopping;
	}
}

bool FVoiceCaptureWindows::ChangeDevice(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized)
	{
		return CreateCaptureBuffer(DeviceName, SampleRate, NumChannels);
	}
	else
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Unable to change device, not initialized"));
		return false;
	}
}

bool FVoiceCaptureWindows::IsCapturing()
{		
	if (CV->VoiceCaptureBuffer8)
	{
		DWORD Status = 0;

		HRESULT hr = CV->VoiceCaptureBuffer8->GetStatus(&Status);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer status 0x%08x"), hr);
		}

		// Status & DSCBSTATUS_LOOPING
		return Status & DSCBSTATUS_CAPTURING ? true : false;
	}

	return false;
}

EVoiceCaptureState::Type FVoiceCaptureWindows::GetCaptureState(uint32& OutAvailableVoiceData) const
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized &&
		VoiceCaptureState != EVoiceCaptureState::Error)
	{
		OutAvailableVoiceData = UncompressedAudioBuffer.Num();
	}
	else
	{
		OutAvailableVoiceData = 0;
	}

	return VoiceCaptureState;
}

void FVoiceCaptureWindows::ProcessData()
{
	DWORD CurrentCapturePos = 0;
	DWORD CurrentReadPos = 0;

	HRESULT hr = CV->VoiceCaptureBuffer8 ? CV->VoiceCaptureBuffer8->GetCurrentPosition(&CurrentCapturePos, &CurrentReadPos) : E_FAIL;
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer cursor position 0x%08x"), hr);
		VoiceCaptureState = EVoiceCaptureState::Error;
		return;
	}

	DWORD LockSize = ((CurrentReadPos - CV->NextCaptureOffset) + CV->VoiceCaptureBufferCaps8.dwBufferBytes) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;
	if(LockSize != 0) 
	{ 
		//UE_LOG( LogVoiceCapture, Log, TEXT( "LockSize: %i, CurrentCapturePos: %i, CurrentReadPos: %i, NextCaptureOffset: %i" ), LockSize, CurrentCapturePos, CurrentReadPos, CV->NextCaptureOffset );

		DWORD CaptureFlags = 0;
		DWORD CaptureLength = 0;
		void* CaptureData = nullptr;
		DWORD CaptureLength2 = 0;
		void* CaptureData2 = nullptr;
		hr = CV->VoiceCaptureBuffer8->Lock(CV->NextCaptureOffset, LockSize,
			&CaptureData, &CaptureLength, 
			&CaptureData2, &CaptureLength2, CaptureFlags);
		if (SUCCEEDED(hr))
		{
			const DWORD OriginalCaptureLength = CaptureLength;
			const DWORD OriginalCaptureLength2 = CaptureLength2;

			if (UncompressedAudioBuffer.Num() + CaptureLength + CaptureLength2 > (DWORD)UncompressedAudioBuffer.Max())
			{ 
				UE_LOG(LogVoiceCapture, Warning, TEXT("Resetting UncompressedAudioBuffer."));
				UncompressedAudioBuffer.Empty(UncompressedAudioBuffer.Max());
				VoiceCaptureState = EVoiceCaptureState::NoData;
			}

			const int32 Offset = UncompressedAudioBuffer.Num();

			CaptureLength = FMath::Min(CaptureLength, (DWORD)UncompressedAudioBuffer.Max());
			CaptureLength2 = FMath::Min(CaptureLength2, (DWORD)UncompressedAudioBuffer.Max() - CaptureLength);

			UncompressedAudioBuffer.AddUninitialized(CaptureLength + CaptureLength2);

			uint8* AudioBuffer = UncompressedAudioBuffer.GetData() + Offset;
			FMemory::Memcpy(AudioBuffer, CaptureData, CaptureLength);

			if (CaptureData2 && CaptureLength2 > 0)
			{
				FMemory::Memcpy(AudioBuffer + CaptureLength, CaptureData2, CaptureLength2);
			}

			CA_SUPPRESS(6385);
			CV->VoiceCaptureBuffer8->Unlock(CaptureData, OriginalCaptureLength, CaptureData2, OriginalCaptureLength2);

			// Move the capture offset forward.
			CV->NextCaptureOffset = (CV->NextCaptureOffset + CaptureLength) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;
			CV->NextCaptureOffset = (CV->NextCaptureOffset + CaptureLength2) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;

			// If Offset > 0 then we know a previous check guarantees it's not silent data
			if (Offset == 0 && IsSilence((int16*)AudioBuffer, UncompressedAudioBuffer.Num()))
			{
				VoiceCaptureState = EVoiceCaptureState::NoData;
				UncompressedAudioBuffer.Empty(UncompressedAudioBuffer.Max());
			}
			else
			{
				VoiceCaptureState = EVoiceCaptureState::Ok;
			}

#if !UE_BUILD_SHIPPING
			const double NewTime = FPlatformTime::Seconds();
			UE_LOG(LogVoiceCapture, VeryVerbose, TEXT("LastCapture: %f %s"), (NewTime - LastCaptureTime) * 1000.0, EVoiceCaptureState::ToString(VoiceCaptureState));
			LastCaptureTime = NewTime;
#endif
		}
		else
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to lock voice buffer 0x%08x"), hr);
			VoiceCaptureState = EVoiceCaptureState::Error;
		}
	}
}

EVoiceCaptureState::Type FVoiceCaptureWindows::GetVoiceData(uint8* OutVoiceBuffer, const uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData)
{
	EVoiceCaptureState::Type NewMicState = VoiceCaptureState;
	OutAvailableVoiceData = 0;

	if (VoiceCaptureState == EVoiceCaptureState::Ok ||
		VoiceCaptureState == EVoiceCaptureState::Stopping)
	{
		check(UncompressedAudioBuffer.Num() <= (int32)CV->VoiceCaptureBufferCaps8.dwBufferBytes);

		OutAvailableVoiceData = UncompressedAudioBuffer.Num();
		if (InVoiceBufferSize >= OutAvailableVoiceData)
		{
			FMemory::Memcpy(OutVoiceBuffer, UncompressedAudioBuffer.GetData(), OutAvailableVoiceData);
			VoiceCaptureState = EVoiceCaptureState::NoData;
			UncompressedAudioBuffer.Empty(UncompressedAudioBuffer.Max());
		}
		else
		{
			NewMicState = EVoiceCaptureState::BufferTooSmall;
		}
	}

	return NewMicState;
}

int32 FVoiceCaptureWindows::GetBufferSize() const
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized)
	{
		return CV->VoiceCaptureBufferCaps8.dwBufferBytes;
	}

	return 0;
}

bool FVoiceCaptureWindows::CreateNotifications(uint32 BufferSize)
{
	bool bSuccess = false;

	LPDIRECTSOUNDNOTIFY8 NotifyInt = nullptr;

	HRESULT hr = CV->VoiceCaptureBuffer8->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&NotifyInt);
	if (SUCCEEDED(hr))
	{
		// Create stop event
		CV->StopEvent = CreateEvent(nullptr, true, false, nullptr);
		if (CV->StopEvent == NULL || CV->StopEvent == INVALID_HANDLE_VALUE)
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Error creating stop event"));
		}
		else
		{
			DSBPOSITIONNOTIFY StopEvent;

			// when buffer stops
			StopEvent.dwOffset = DSBPN_OFFSETSTOP;
			StopEvent.hEventNotify = CV->StopEvent;

			hr = NotifyInt->SetNotificationPositions(1, &StopEvent);
			if (SUCCEEDED(hr))
			{
				bSuccess = true;
			}
			else
			{
				UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to set stop notifications 0x%08x"), hr);

				CloseHandle(CV->StopEvent);
				CV->StopEvent = INVALID_HANDLE_VALUE;
			}
		}

		NotifyInt->Release();
	}
	else
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice notification interface 0x%08x"), hr);
	}

	return bSuccess;
}

bool FVoiceCaptureWindows::Tick(float DeltaTime)
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized &&
		VoiceCaptureState != EVoiceCaptureState::NotCapturing)
	{
		ProcessData();

		if (CV->StopEvent != INVALID_HANDLE_VALUE && WaitForSingleObject(CV->StopEvent, 0) == WAIT_OBJECT_0)
		{
			UE_LOG(LogVoiceCapture, Verbose, TEXT("Voice capture stopped"));
			ResetEvent(CV->StopEvent);
			VoiceCaptureState = EVoiceCaptureState::NotCapturing;
			UncompressedAudioBuffer.Empty(UncompressedAudioBuffer.Max());
		}
	}

	return true;
}

void FVoiceCaptureWindows::DumpState() const
{
	if (CV)
	{
		extern FString PrintMSGUID(LPGUID Guid);
		UE_LOG(LogVoiceCapture, Display, TEXT("Device %s"), *PrintMSGUID(&CV->VoiceCaptureDeviceGuid));

		UE_LOG(LogVoiceCapture, Display, TEXT("CaptureDev: 0x%08x"), CV->VoiceCaptureDev);
		UE_LOG(LogVoiceCapture, Display, TEXT("CaptureBuffer: 0x%08x"), CV->VoiceCaptureBuffer8);

		UE_LOG(LogVoiceCapture, Display, TEXT("Capture Format"));
		UE_LOG(LogVoiceCapture, Display, TEXT("- Tag: %d"), CV->WavFormat.wFormatTag);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Channels: %d"), CV->WavFormat.nChannels);
		UE_LOG(LogVoiceCapture, Display, TEXT("- BitsPerSample: %d"), CV->WavFormat.wBitsPerSample);
		UE_LOG(LogVoiceCapture, Display, TEXT("- SamplesPerSec: %d"), CV->WavFormat.nSamplesPerSec);
		UE_LOG(LogVoiceCapture, Display, TEXT("- BlockAlign: %d"), CV->WavFormat.nBlockAlign);
		UE_LOG(LogVoiceCapture, Display, TEXT("- AvgBytesPerSec: %d"), CV->WavFormat.nAvgBytesPerSec);

		UE_LOG(LogVoiceCapture, Display, TEXT("Capture Buffer"));
		UE_LOG(LogVoiceCapture, Display, TEXT("- Flags: 0x%08x"), CV->VoiceCaptureBufferDesc.dwFlags);
		UE_LOG(LogVoiceCapture, Display, TEXT("- BufferBytes: %d"), CV->VoiceCaptureBufferDesc.dwBufferBytes);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Format: 0x%08x"), CV->VoiceCaptureBufferDesc.lpwfxFormat);

		UE_LOG(LogVoiceCapture, Display, TEXT("Device Caps"));
		UE_LOG(LogVoiceCapture, Display, TEXT("- Size: %d"), CV->VoiceCaptureDevCaps.dwSize);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Flags: 0x%08x"), CV->VoiceCaptureDevCaps.dwFlags);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Formats: %d"), CV->VoiceCaptureDevCaps.dwFormats);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Channels: %d"), CV->VoiceCaptureDevCaps.dwChannels);

		UE_LOG(LogVoiceCapture, Display, TEXT("D3D8 Caps"));
		UE_LOG(LogVoiceCapture, Display, TEXT("- Size: %d"), CV->VoiceCaptureBufferCaps8.dwSize);
		UE_LOG(LogVoiceCapture, Display, TEXT("- Flags: 0x%08x"), CV->VoiceCaptureBufferCaps8.dwFlags);
		UE_LOG(LogVoiceCapture, Display, TEXT("- BufferBytes: %d"), CV->VoiceCaptureBufferCaps8.dwBufferBytes);
	}
	else
	{
		UE_LOG(LogVoiceCapture, Display, TEXT("No capture device to dump state"));
	}
}

#include "HideWindowsPlatformTypes.h"

#endif // PLATFORM_SUPPORTS_VOICE_CAPTURE