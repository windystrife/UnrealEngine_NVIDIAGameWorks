// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformAndroid.h"


class FAudioMixerModuleAndroid : public IAudioDeviceModule
{
public:
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformAndroid());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleAndroid, AudioMixerAndroid);
