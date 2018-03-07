// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOculusAudioPlugin.h"

void FOculusAudioPlugin::StartupModule()
{
	FOculusAudioLibraryManager::Initialize();
	IModularFeatures::Get().RegisterModularFeature(FOculusSpatializationPluginFactory::GetModularFeatureName(), &PluginFactory);
};

void FOculusAudioPlugin::ShutdownModule()
{
	FOculusAudioLibraryManager::Shutdown();
}

IMPLEMENT_MODULE(FOculusAudioPlugin, OculusAudio)