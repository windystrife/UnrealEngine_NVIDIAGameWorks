// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformXAudio2.h"

class FAudioMixerModuleXAudio2 : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformXAudio2());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleXAudio2, AudioMixerXAudio2);


