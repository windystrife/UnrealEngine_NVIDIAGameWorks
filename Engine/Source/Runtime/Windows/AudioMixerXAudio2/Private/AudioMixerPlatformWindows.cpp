// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of FAudioDevice for XAudio2

	See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405049%28v=vs.85%29.aspx
*/

#include "AudioMixerPlatformXAudio2.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"

#define INITGUID
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

class FWindowsMMNotificationClient final : public IMMNotificationClient
{
public:
	FWindowsMMNotificationClient()
		: Ref(1)
		, DeviceEnumerator(nullptr)
	{
		bComInitialized = FWindowsPlatformMisc::CoInitialize();
		HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&DeviceEnumerator);
		if (Result == S_OK)
		{
			DeviceEnumerator->RegisterEndpointNotificationCallback(this);
		}
	}

	virtual ~FWindowsMMNotificationClient()
	{
		if (DeviceEnumerator)
		{
			DeviceEnumerator->UnregisterEndpointNotificationCallback(this);
			SAFE_RELEASE(DeviceEnumerator);
		}

		if (bComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}
	}

	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow InFlow, ERole InRole, LPCWSTR pwstrDeviceId) override
	{
		Audio::EAudioDeviceRole AudioDeviceRole;

		if (InRole == eConsole)
		{
			AudioDeviceRole = Audio::EAudioDeviceRole::Console;
		}
		else if (InRole == eMultimedia)
		{
			AudioDeviceRole = Audio::EAudioDeviceRole::Multimedia;
		}
		else
		{
			AudioDeviceRole = Audio::EAudioDeviceRole::Communications;
		}

		if (InFlow == eRender)
		{
			for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
			{
				Listener->OnDefaultRenderDeviceChanged(AudioDeviceRole, FString(pwstrDeviceId));
			}
		}
		else if (InFlow == eCapture)
		{
			for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
			{
				Listener->OnDefaultCaptureDeviceChanged(AudioDeviceRole, FString(pwstrDeviceId));
			}
		}
		else
		{
			for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
			{
				Listener->OnDefaultCaptureDeviceChanged(AudioDeviceRole, FString(pwstrDeviceId));
				Listener->OnDefaultRenderDeviceChanged(AudioDeviceRole, FString(pwstrDeviceId));
			}
		}


		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override
	{
		for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
		{
			Listener->OnDeviceAdded(FString(pwstrDeviceId));
		}
		return S_OK;
	};

	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override
	{
		for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
		{
			Listener->OnDeviceRemoved(FString(pwstrDeviceId));
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override
	{
		if (dwNewState == DEVICE_STATE_DISABLED || dwNewState == DEVICE_STATE_UNPLUGGED || dwNewState == DEVICE_STATE_NOTPRESENT)
		{
			for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
			{
				Listener->OnDeviceRemoved(FString(pwstrDeviceId));
			}
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
	{
		FString ChangedId = FString(pwstrDeviceId);

		// look for ids we care about!
		if (key.fmtid == PKEY_AudioEndpoint_PhysicalSpeakers.fmtid || 
			key.fmtid == PKEY_AudioEngine_DeviceFormat.fmtid ||
			key.fmtid == PKEY_AudioEngine_OEMFormat.fmtid)
		{
			for (Audio::IAudioMixerDeviceChangedLister* Listener : Listeners)
			{
				Listener->OnDeviceRemoved(FString(pwstrDeviceId));
			}
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID &, void **) override
	{
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&Ref);
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG ulRef = InterlockedDecrement(&Ref);
		if (0 == ulRef)
		{
			delete this;
		}
		return ulRef;
	}

	void RegisterDeviceChangedListener(Audio::IAudioMixerDeviceChangedLister* DeviceChangedListener)
	{
		Listeners.Add(DeviceChangedListener);
	}

	void UnRegisterDeviceDeviceChangedListener(Audio::IAudioMixerDeviceChangedLister* DeviceChangedListener)
	{
		Listeners.Remove(DeviceChangedListener);
	}

private:
	LONG Ref;
	TSet<Audio::IAudioMixerDeviceChangedLister*> Listeners;
	IMMDeviceEnumerator* DeviceEnumerator;
	bool bComInitialized;
};




namespace Audio
{
	TSharedPtr<FWindowsMMNotificationClient> WindowsNotificationClient;

	void FMixerPlatformXAudio2::RegisterDeviceChangedListener()
	{
		if (!WindowsNotificationClient.IsValid())
		{
			WindowsNotificationClient = TSharedPtr<FWindowsMMNotificationClient>(new FWindowsMMNotificationClient);
		}

		WindowsNotificationClient->RegisterDeviceChangedListener(this);
	}

	void FMixerPlatformXAudio2::UnregisterDeviceChangedListener() 
	{
		WindowsNotificationClient->UnRegisterDeviceDeviceChangedListener(this);
	}

	void FMixerPlatformXAudio2::OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId)
	{
		UE_LOG(LogTemp, Log, TEXT("OnDefaultCaptureDeviceChanged: %s"), *DeviceId);
	}

	void FMixerPlatformXAudio2::OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId)
	{
		UE_LOG(LogTemp, Log, TEXT("OnDefaultRenderDeviceChanged: %s"), *DeviceId);
		NewAudioDeviceId = "";
		bMoveAudioStreamToNewAudioDevice = true;
	}

	void FMixerPlatformXAudio2::OnDeviceAdded(const FString& DeviceId)
	{
		UE_LOG(LogTemp, Log, TEXT("OnDeviceAdded: %s"), *DeviceId);

		// If the device that was added is our original device and our current device is NOT our original device, 
		// move our audio stream to this newly added device.
		if (AudioStreamInfo.DeviceInfo.DeviceId != OriginalAudioDeviceId && DeviceId == OriginalAudioDeviceId)
		{
			NewAudioDeviceId = OriginalAudioDeviceId;
			bMoveAudioStreamToNewAudioDevice = true;
		}
	}

	void FMixerPlatformXAudio2::OnDeviceRemoved(const FString& DeviceId)
	{
		UE_LOG(LogTemp, Log, TEXT("OnDeviceRemoved: %s"), *DeviceId);

		// If the device we're currently using was removed... then switch to the new default audio device.
		if (AudioStreamInfo.DeviceInfo.DeviceId == DeviceId)
		{
			NewAudioDeviceId = "";
			bMoveAudioStreamToNewAudioDevice = true;
		}

	}

	void FMixerPlatformXAudio2::OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState)
	{
		UE_LOG(LogTemp, Log, TEXT("OnDeviceStateChanged: %s"), *DeviceId);
	}

	FString FMixerPlatformXAudio2::GetDeviceId() const
	{
		return AudioStreamInfo.DeviceInfo.DeviceId;
	}
}

#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"

#else 
// Nothing for XBOXOne
namespace Audio
{
	void FMixerPlatformXAudio2::RegisterDeviceChangedListener() {}
	void FMixerPlatformXAudio2::UnregisterDeviceChangedListener() {}
	void FMixerPlatformXAudio2::OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) {}
	void FMixerPlatformXAudio2::OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) {}
	void FMixerPlatformXAudio2::OnDeviceAdded(const FString& DeviceId) {}
	void FMixerPlatformXAudio2::OnDeviceRemoved(const FString& DeviceId) {}
	void FMixerPlatformXAudio2::OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState){}
	FString FMixerPlatformXAudio2::GetDeviceId() const { return TEXT("XboxOneAudioDevice"); }
}
#endif