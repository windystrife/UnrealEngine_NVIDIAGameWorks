// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioPluginUtilities.h"

#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"


/** Platform config section for each platform's target settings. */
FORCEINLINE const TCHAR* GetPlatformConfigSection(EAudioPlatform AudioPlatform)
{
	switch (AudioPlatform)
	{
		case EAudioPlatform::Windows:
			return TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings");

		case EAudioPlatform::Mac:
			return TEXT("/Script/MacTargetPlatform.MacTargetSettings");

		case EAudioPlatform::Linux:
			return TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings");

		case EAudioPlatform::IOS:
			return TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");

		case EAudioPlatform::Android:
			return TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");

		case EAudioPlatform::XboxOne:
			return TEXT("/Script/XboxOnePlatformEditor.XboxOneTargetSettings");

		case EAudioPlatform::Playstation4:
			return TEXT("/Script/PS4PlatformEditor.PS4TargetSettings");

		case EAudioPlatform::Switch:
			return TEXT("/Script/SwitchRuntimeSettings.SwitchRuntimeSettings");

		case EAudioPlatform::HTML5:
			return TEXT("/Script/HTML5PlatformEditor.HTML5TargetSettings");

		case EAudioPlatform::Unknown:
			return TEXT("");

		default:
			checkf(false, TEXT("Undefined audio platform."));
			break;
	}
	return TEXT("");
}

/** Get the target setting name for each platform type. */
FORCEINLINE const TCHAR* GetPluginConfigName(EAudioPlugin PluginType)
{
	switch (PluginType)
	{
		case EAudioPlugin::SPATIALIZATION:
			return TEXT("SpatializationPlugin");

		case EAudioPlugin::REVERB:
			return TEXT("ReverbPlugin");

		case EAudioPlugin::OCCLUSION:
			return TEXT("OcclusionPlugin");

		default:
			checkf(false, TEXT("Undefined audio plugin type."));
			return TEXT("");
	}
}

/************************************************************************/
/* Plugin Utilities                                                     */
/************************************************************************/
IAudioSpatializationFactory* AudioPluginUtilities::GetDesiredSpatializationPlugin(EAudioPlatform AudioPlatform)
{
	//Get the name of the desired spatialization plugin:
	FString DesiredSpatializationPlugin = GetDesiredPluginName(EAudioPlugin::SPATIALIZATION, AudioPlatform);


	TArray<IAudioSpatializationFactory *> SpatializationPluginFactories = IModularFeatures::Get().GetModularFeatureImplementations<IAudioSpatializationFactory>(IAudioSpatializationFactory::GetModularFeatureName());

	//Iterate through all of the plugins we've discovered:
	for (IAudioSpatializationFactory* PluginFactory : SpatializationPluginFactories)
	{
		//if this plugin's name matches the name found in the platform settings, use it:
		if (PluginFactory->GetDisplayName().Equals(DesiredSpatializationPlugin))
		{
			return PluginFactory;
		}
	}

	return nullptr;
}

IAudioReverbFactory* AudioPluginUtilities::GetDesiredReverbPlugin(EAudioPlatform AudioPlatform)
{
	//Get the name of the desired Reverb plugin:
	FString DesiredReverbPlugin = GetDesiredPluginName(EAudioPlugin::REVERB, AudioPlatform);

	TArray<IAudioReverbFactory *> ReverbPluginFactories = IModularFeatures::Get().GetModularFeatureImplementations<IAudioReverbFactory>(IAudioReverbFactory::GetModularFeatureName());

	//Iterate through all of the plugins we've discovered:
	for (IAudioReverbFactory* PluginFactory : ReverbPluginFactories)
	{
		//if this plugin's name matches the name found in the platform settings, use it:
		if (PluginFactory->GetDisplayName().Equals(DesiredReverbPlugin))
		{
			return PluginFactory;
		}
	}

	return nullptr;
}

IAudioOcclusionFactory* AudioPluginUtilities::GetDesiredOcclusionPlugin(EAudioPlatform AudioPlatform)
{
	FString DesiredOcclusionPlugin = GetDesiredPluginName(EAudioPlugin::OCCLUSION, AudioPlatform);

	TArray<IAudioOcclusionFactory *> OcclusionPluginFactories = IModularFeatures::Get().GetModularFeatureImplementations<IAudioOcclusionFactory>(IAudioOcclusionFactory::GetModularFeatureName());

	//Iterate through all of the plugins we've discovered:
	for (IAudioOcclusionFactory* PluginFactory : OcclusionPluginFactories)
	{
		//if this plugin's name matches the name found in the platform settings, use it:
		if (PluginFactory->GetDisplayName().Equals(DesiredOcclusionPlugin))
		{
			return PluginFactory;
		}
	}

	return nullptr;
}

FString AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin PluginType, EAudioPlatform AudioPlatform)
{
	FString PluginName;
	GConfig->GetString(GetPlatformConfigSection(AudioPlatform), GetPluginConfigName(PluginType), PluginName, GEngineIni);
	return PluginName;
}
