// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerModule.h"
#include "ModuleManager.h"
#include "AudioMixerLog.h"

DEFINE_LOG_CATEGORY(LogAudioMixer);
DEFINE_LOG_CATEGORY(LogAudioMixerDebug);

class FAudioMixerModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}
};

IMPLEMENT_MODULE(FAudioMixerModule, AudioMixer);