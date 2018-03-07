// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include <SLES/OpenSLES.h>
#include "SLES/OpenSLES_Android.h"

// Any platform defines
namespace Audio
{

	class FMixerPlatformAndroid : public IAudioMixerPlatformInterface
	{

	public:

		FMixerPlatformAndroid();
		~FMixerPlatformAndroid();

		//~ Begin IAudioMixerPlatformInterface
		virtual EAudioMixerPlatformApi::Type GetPlatformApi() const override { return EAudioMixerPlatformApi::OpenSLES; }
		virtual bool InitializeHardware() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual FName GetRuntimeFormat(USoundWave* InSoundWave) override;
		virtual bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) override;
		virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* InSoundWave) override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual void SuspendContext() override;
		virtual void ResumeContext() override;
		//~ End IAudioMixerPlatformInterface
		
	private:
		const TCHAR* GetErrorString(SLresult Result);

		SLObjectItf	SL_EngineObject;
		SLEngineItf	SL_EngineEngine;
		SLObjectItf	SL_OutputMixObject;
		SLObjectItf	SL_PlayerObject;
		SLPlayItf SL_PlayerPlayInterface;
		SLAndroidSimpleBufferQueueItf SL_PlayerBufferQueue;

		bool bSuspended;
		bool bInitialized;
		bool bInCallback;
		
		static void OpenSLBufferQueueCallback( SLAndroidSimpleBufferQueueItf InQueueInterface, void* pContext );		
	};

}

