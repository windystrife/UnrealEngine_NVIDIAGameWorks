// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of FAudioDevice for Apple's CoreAudio

*/

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformAudioUnit.h"


class FAudioMixerModuleAudioUnit : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformAudioUnit());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleAudioUnit, AudioMixerAudioUnit);
