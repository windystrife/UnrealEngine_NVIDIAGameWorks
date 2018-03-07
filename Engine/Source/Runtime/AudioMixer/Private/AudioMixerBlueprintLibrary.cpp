// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "CoreMinimal.h"


static FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* WorldContextObject)
{
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	return ThisWorld->GetAudioDevice();
}

static Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* WorldContextObject)
{
	if (FAudioDevice* AudioDevice = GetAudioDeviceFromWorldContext(WorldContextObject))
	{
		return static_cast<Audio::FMixerDevice*>(AudioDevice);
	}
	return nullptr;
}

void UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset)
{
	if (!SubmixEffectPreset)
	{
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		// Immediately create a new sound effect base here before the object becomes potentially invalidated
		FSoundEffectBase* SoundEffectBase = SubmixEffectPreset->CreateNewEffect();

		// Cast it to a sound effect submix type
		FSoundEffectSubmix* SoundEffectSubmix = static_cast<FSoundEffectSubmix*>(SoundEffectBase);

		FSoundEffectSubmixInitData InitData;
		InitData.SampleRate = MixerDevice->GetSampleRate();

		// Initialize and set the preset immediately
		SoundEffectSubmix->Init(InitData);
		SoundEffectSubmix->SetPreset(SubmixEffectPreset);
		SoundEffectSubmix->SetEnabled(true);

		// Get a unique id for the preset object on the game thread. Used to refer to the object on audio render thread.
		uint32 SubmixPresetUniqueId = SubmixEffectPreset->GetUniqueID();

		MixerDevice->AddMasterSubmixEffect(SubmixPresetUniqueId, SoundEffectSubmix);
	}
}

void UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset)
{
	if (!SubmixEffectPreset)
	{
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		// Get the unique id for the preset object on the game thread. Used to refer to the object on audio render thread.
		uint32 SubmixPresetUniqueId = SubmixEffectPreset->GetUniqueID();

		MixerDevice->RemoveMasterSubmixEffect(SubmixPresetUniqueId);
	}
}

void UAudioMixerBlueprintLibrary::ClearMasterSubmixEffects(const UObject* WorldContextObject)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->ClearMasterSubmixEffects();
	}
}


void UAudioMixerBlueprintLibrary::AddSourceEffectToPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, FSourceEffectChainEntry Entry)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		Chain.Add(Entry);
		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}
}

void UAudioMixerBlueprintLibrary::RemoveSourceEffectFromPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		if (EntryIndex < Chain.Num())
		{
			Chain.RemoveAt(EntryIndex);
		}

		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}

}

void UAudioMixerBlueprintLibrary::SetBypassSourceEffectChainEntry(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex, bool bBypassed)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		if (EntryIndex < Chain.Num())
		{
			Chain[EntryIndex].bBypass = bBypassed;
		}

		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}
}

int32 UAudioMixerBlueprintLibrary::GetNumberOfEntriesInSourceEffectChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			return PresetChain->Chain.Num();
		}

		return Chain.Num();
	}

	return 0;
}

