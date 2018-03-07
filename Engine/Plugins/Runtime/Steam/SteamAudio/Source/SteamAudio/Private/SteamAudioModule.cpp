//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioModule.h"
#include "Misc/Paths.h"

IMPLEMENT_MODULE(SteamAudio::FSteamAudioModule, SteamAudio)

namespace SteamAudio
{
	void* FSteamAudioModule::PhononDllHandle = nullptr;

	static bool bModuleStartedUp = false;

	FSteamAudioModule::FSteamAudioModule()
	{
	}

	FSteamAudioModule::~FSteamAudioModule()
	{
	}

	void FSteamAudioModule::StartupModule()
	{
		check(bModuleStartedUp == false);

		bModuleStartedUp = true;

		UE_LOG(LogSteamAudio, Log, TEXT("FSteamAudioModule Startup"));

		//Register the Steam Audio plugin factories
		IModularFeatures::Get().RegisterModularFeature(FSpatializationPluginFactory::GetModularFeatureName(), &SpatializationPluginFactory);
		IModularFeatures::Get().RegisterModularFeature(FReverbPluginFactory::GetModularFeatureName(), &ReverbPluginFactory);
		IModularFeatures::Get().RegisterModularFeature(FOcclusionPluginFactory::GetModularFeatureName(), &OcclusionPluginFactory);

		if (!FSteamAudioModule::PhononDllHandle)
		{
#if PLATFORM_32BITS
			FString PathToDll = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Phonon/Win32/");
#else
			FString PathToDll = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Phonon/Win64/");
#endif

			FString DLLToLoad = PathToDll + TEXT("phonon.dll");
			FSteamAudioModule::PhononDllHandle = LoadDll(DLLToLoad);
		}
	}

	void FSteamAudioModule::ShutdownModule()
	{
		UE_LOG(LogSteamAudio, Log, TEXT("FSteamAudioModule Shutdown"));

		check(bModuleStartedUp == true);

		bModuleStartedUp = false;

		if (FSteamAudioModule::PhononDllHandle)
		{
			FPlatformProcess::FreeDllHandle(FSteamAudioModule::PhononDllHandle);
			FSteamAudioModule::PhononDllHandle = nullptr;
		}
	}

	IAudioPluginFactory* FSteamAudioModule::GetPluginFactory(EAudioPlugin PluginType)
	{
		switch (PluginType)
		{
		case EAudioPlugin::SPATIALIZATION:
			return &SpatializationPluginFactory;
			break;
		case EAudioPlugin::REVERB:
			return &ReverbPluginFactory;
			break;
		case EAudioPlugin::OCCLUSION:
			return &OcclusionPluginFactory;
			break;
		default:
			return nullptr;
			break;
		}
	}

	void FSteamAudioModule::RegisterAudioDevice(FAudioDevice* AudioDeviceHandle)
	{
		if (!RegisteredAudioDevices.Contains(AudioDeviceHandle))
		{
			TAudioPluginListenerPtr NewPhononPluginManager = TAudioPluginListenerPtr(new FPhononPluginManager());
			AudioDeviceHandle->RegisterPluginListener(NewPhononPluginManager);
			RegisteredAudioDevices.Add(AudioDeviceHandle);
		}
	}

	void FSteamAudioModule::UnregisterAudioDevice(FAudioDevice* AudioDeviceHandle)
	{
		RegisteredAudioDevices.Remove(AudioDeviceHandle);
	}

	TAudioOcclusionPtr FOcclusionPluginFactory::CreateNewOcclusionPlugin(FAudioDevice* OwningDevice)
	{
		//Register audio device to the Steam Module
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");
		if (Module != nullptr)
		{
			Module->RegisterAudioDevice(OwningDevice);
		}

		return TAudioOcclusionPtr(new FPhononOcclusion());
	}

	TAudioReverbPtr FReverbPluginFactory::CreateNewReverbPlugin(FAudioDevice* OwningDevice)
	{
		//Register the audio device to the steam module:
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");
		if (Module != nullptr)
		{
			Module->RegisterAudioDevice(OwningDevice);
		}

		return TAudioReverbPtr(new FPhononReverb());
	}

	TAudioSpatializationPtr FSpatializationPluginFactory::CreateNewSpatializationPlugin(FAudioDevice* OwningDevice)
	{
		return TAudioSpatializationPtr(new FPhononSpatialization());
	}

}

