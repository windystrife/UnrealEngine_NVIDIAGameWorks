//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononPluginManager.h"
#include "SteamAudioModule.h"
#include "AudioDevice.h"

namespace SteamAudio
{
	FPhononPluginManager::FPhononPluginManager()
		: bEnvironmentCreated(false)
		, ReverbPtr(nullptr)
		, OcclusionPtr(nullptr)
	{
	}

	FPhononPluginManager::~FPhononPluginManager()
	{
		if (bEnvironmentCreated)
		{
			Environment.Shutdown();
			bEnvironmentCreated = false;
		}
	}

	void FPhononPluginManager::OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
		IPLhandle RendererPtr = Environment.Initialize(ListenerWorld, AudioDevice);

		// If we've succeeded, pass the phonon environmental renderer to the occlusion and reverb plugins, if we're using them
		if (RendererPtr)
		{
			if (IsUsingSteamAudioPlugin(EAudioPlugin::REVERB))
			{
				ReverbPtr = (FPhononReverb*)AudioDevice->ReverbPluginInterface.Get();
				ReverbPtr->SetEnvironmentalRenderer(RendererPtr);
				ReverbPtr->SetEnvironmentCriticalSection(Environment.GetEnvironmentCriticalSection());
				ReverbPtr->CreateReverbEffect();
			}

			if (IsUsingSteamAudioPlugin(EAudioPlugin::OCCLUSION))
			{
				OcclusionPtr = (FPhononOcclusion*)AudioDevice->OcclusionInterface.Get();
				OcclusionPtr->SetEnvironmentalRenderer(RendererPtr);
				OcclusionPtr->SetCriticalSectionHandle(Environment.GetEnvironmentCriticalSection());
			}

			bEnvironmentCreated = true;
		}
	}

	void FPhononPluginManager::OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
		if (!bEnvironmentCreated)
		{
			return;
		}

		FVector Position = ListenerTransform.GetLocation();
		FVector Forward = ListenerTransform.GetUnitAxis(EAxis::Y);
		FVector Up = ListenerTransform.GetUnitAxis(EAxis::Z);

		if (OcclusionPtr)
		{
			OcclusionPtr->UpdateDirectSoundSources(Position, Forward, Up);
		}

		if (ReverbPtr)
		{
			ReverbPtr->UpdateListener(Position, Forward, Up);
		}
	}

	void FPhononPluginManager::OnListenerShutdown(FAudioDevice* AudioDevice)
	{
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");
		if (Module != nullptr)
		{
			Module->UnregisterAudioDevice(AudioDevice);
		}
	}

	bool FPhononPluginManager::IsUsingSteamAudioPlugin(EAudioPlugin PluginType)
	{
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");

		// If we can't get the module from the module manager, then we don't have any of these plugins loaded.
		if (Module == nullptr)
		{
			return false;
		}

		FString SteamPluginName = Module->GetPluginFactory(PluginType)->GetDisplayName();
		FString CurrentPluginName = AudioPluginUtilities::GetDesiredPluginName(PluginType, AudioPluginUtilities::CurrentPlatform);
		return CurrentPluginName.Equals(SteamPluginName);
	}
}
