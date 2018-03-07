// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "AllowWindowsPlatformTypes.h"
#include <xaudio2.h>
#include "HideWindowsPlatformTypes.h"

// Any platform defines
namespace Audio
{
	class FMixerPlatformXAudio2;

	/**
	* FXAudio2VoiceCallback
	* XAudio2 implementation of IXAudio2VoiceCallback
	* This callback class is used to get event notifications on buffer end (when a buffer has finished processing).
	* This is used to signal the I/O thread that it can request another buffer from the user callback.
	*/
	class FXAudio2VoiceCallback final : public IXAudio2VoiceCallback
	{
	public:
		FXAudio2VoiceCallback() {}
		virtual ~FXAudio2VoiceCallback() {}

	private:
		void STDCALL OnVoiceProcessingPassStart(UINT32 BytesRequired) {}
		void STDCALL OnVoiceProcessingPassEnd() {}
		void STDCALL OnStreamEnd() {}
		void STDCALL OnBufferStart(void* BufferContext) {}
		void STDCALL OnLoopEnd(void* BufferContext) {}
		void STDCALL OnVoiceError(void* BufferContext, HRESULT Error) {}

		void STDCALL OnBufferEnd(void* BufferContext);

	};

	class FMixerPlatformXAudio2 : public IAudioMixerPlatformInterface
	{

	public:

		FMixerPlatformXAudio2();
		~FMixerPlatformXAudio2();

		//~ Begin IAudioMixerPlatformInterface
		virtual EAudioMixerPlatformApi::Type GetPlatformApi() const override { return EAudioMixerPlatformApi::XAudio2; }
		virtual bool InitializeHardware() override;
		virtual bool CheckAudioDeviceChange() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual bool MoveAudioStreamToNewAudioDevice(const FString& InNewDeviceId) override;
		virtual void ResumePlaybackOnNewDevice() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual FName GetRuntimeFormat(USoundWave* InSoundWave) override;
		virtual bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) override;
		virtual bool SupportsRealtimeDecompression() const override { return true; }
		virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* InSoundWave) override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		//~ End IAudioMixerPlatformInterface

		//~ Begin IAudioMixerDeviceChangedLister
		virtual void RegisterDeviceChangedListener() override;
		virtual void UnregisterDeviceChangedListener() override;
		virtual void OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDeviceAdded(const FString& DeviceId) override;
		virtual void OnDeviceRemoved(const FString& DeviceId) override;
		virtual void OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState) override;
		virtual FString GetDeviceId() const override;
		//~ End IAudioMixerDeviceChangedLister

	private:

		const TCHAR* GetErrorString(HRESULT Result);

		typedef TArray<long> TChannelTypeMap;
		
		// Bool indicating that the default audio device changed
		// And that we need to restart the audio device.
		FThreadSafeBool bDeviceChanged;

		TChannelTypeMap ChannelTypeMap;
		IXAudio2* XAudio2System;
		IXAudio2MasteringVoice* OutputAudioStreamMasteringVoice;
		IXAudio2SourceVoice* OutputAudioStreamSourceVoice;
		FXAudio2VoiceCallback OutputVoiceCallback;
		FString OriginalAudioDeviceId;
		FString NewAudioDeviceId;
		FThreadSafeBool bMoveAudioStreamToNewAudioDevice;
		uint32 bIsComInitialized : 1;
		uint32 bIsInitialized : 1;
		uint32 bIsDeviceOpen : 1;

	};

}

