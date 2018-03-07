// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerEffectsManager.h"
#include "AudioMixerDevice.h"
#include "AudioMixerEffectsManager.h"
#include "SubmixEffects/AudioMixerSubmixEffectReverb.h"
#include "SubmixEffects/AudioMixerSubmixEffectEQ.h"

namespace Audio
{
#define ENABLE_REVERB_SETTINGS_PRINTING 0
#define ENABLE_EQ_SETTINGS_PRINTING 0

	static void PrintReverbSettings(const FAudioReverbEffect& Settings)
	{
#if ENABLE_REVERB_SETTINGS_PRINTING

		const char* FmtText =
			"\nVolume: %.4f\n"
			"Density: %.4f\n"
			"Diffusion: %.4f\n"
			"Gain: %.4f\n"
			"GainHF: %.4f\n"
			"DecayTime: %.4f\n"
			"DecayHFRatio: %.4f\n"
			"ReflectionsGain: %.4f\n"
			"ReflectionsDelay: %.4f\n"
			"LateGain: %.4f\n"
			"LateDelay: %.4f\n"
			"AirAbsorptionGainHF: %.4f\n"
			"RoomRolloffFactor: %.4f\n";

		FString FmtString(FmtText);

		FString Params = FString::Printf(
			*FmtString,
			Settings.Volume,
			Settings.Density,
			Settings.Diffusion,
			Settings.Gain,
			Settings.GainHF,
			Settings.DecayTime,
			Settings.DecayHFRatio,
			Settings.ReflectionsGain,
			Settings.ReflectionsDelay,
			Settings.LateGain,
			Settings.LateDelay,
			Settings.AirAbsorptionGainHF,
			Settings.RoomRolloffFactor
			);

		UE_LOG(LogTemp, Log, TEXT("%s"), *Params);
#endif
	}

	static void PrintEQSettings(const FAudioEQEffect& Settings)
	{
#if ENABLE_EQ_SETTINGS_PRINTING
		const char* FmtText =
			"\nFrequencyCenter0: %.4f\n"
			"Gain0: %.4f\n"
			"Bandwidth0: %.4f\n"
			"FrequencyCenter1: %.4f\n"
			"Gain1: %.4f\n"
			"Bandwidth1: %.4f\n"
			"FrequencyCenter2: %.4f\n"
			"Gain2: %.4f\n"
			"Bandwidth2: %.4f\n"
			"FrequencyCenter3: %.4f\n"
			"Gain3: %.4f\n"
			"Bandwidth3: %.4f\n";

		FString FmtString(FmtText);

		FString Params = FString::Printf(
			*FmtString,
			Settings.FrequencyCenter0,
			Settings.Gain0,
			Settings.Bandwidth0,
			Settings.FrequencyCenter1,
			Settings.Gain1,
			Settings.Bandwidth1,
			Settings.FrequencyCenter2,
			Settings.Gain2,
			Settings.Bandwidth2,
			Settings.FrequencyCenter3,
			Settings.Gain3,
			Settings.Bandwidth3
		);

		UE_LOG(LogTemp, Log, TEXT("%s"), *Params);
#endif
	}



	FAudioMixerEffectsManager::FAudioMixerEffectsManager(FAudioDevice* InDevice)
		: FAudioEffectsManager(InDevice)
	{}

	FAudioMixerEffectsManager::~FAudioMixerEffectsManager()
	{}

	void FAudioMixerEffectsManager::SetReverbEffectParameters(const FAudioReverbEffect& ReverbEffectParameters)
	{
		FMixerDevice* MixerDevice = (FMixerDevice*)AudioDevice;

		FMixerSubmixPtr MasterReverbSubmix = MixerDevice->GetMasterReverbSubmix();
		
		if (MasterReverbSubmix.IsValid())
		{
			FSoundEffectSubmixPtr SoundEffectSubmix = MasterReverbSubmix->GetSubmixEffect(0);
			if (SoundEffectSubmix.IsValid())
			{
				FSubmixEffectReverb* SoundEffectReverb = static_cast<FSubmixEffectReverb*>(SoundEffectSubmix.Get());
				SoundEffectReverb->SetEffectParameters(ReverbEffectParameters);
				PrintReverbSettings(ReverbEffectParameters);
			}
		}
	}

	void FAudioMixerEffectsManager::SetEQEffectParameters(const FAudioEQEffect& InEQEffectParameters)
	{
		FMixerDevice* MixerDevice = (FMixerDevice*)AudioDevice;

		FMixerSubmixPtr MasterEQSubmix = MixerDevice->GetMasterEQSubmix();

		if (MasterEQSubmix.IsValid())
		{
			FSoundEffectSubmixPtr SoundEffectSubmix = MasterEQSubmix->GetSubmixEffect(0);
			if (SoundEffectSubmix.IsValid())
			{
				FSubmixEffectSubmixEQ* SoundEffectEQ = static_cast<FSubmixEffectSubmixEQ*>(SoundEffectSubmix.Get());
				SoundEffectEQ->SetEffectParameters(InEQEffectParameters);
				PrintEQSettings(InEQEffectParameters);
			}
		}

	}

	void FAudioMixerEffectsManager::SetRadioEffectParameters(const FAudioRadioEffect& ReverbEffectParameters)
	{

	}

}
