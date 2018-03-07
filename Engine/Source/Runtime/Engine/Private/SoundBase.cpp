// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundBase.h"
#include "Sound/SoundSubmix.h"
#include "Sound/AudioSettings.h"

USoundClass* USoundBase::DefaultSoundClassObject = nullptr;
USoundConcurrency* USoundBase::DefaultSoundConcurrencyObject = nullptr;

USoundBase::USoundBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIgnoreFocus_DEPRECATED(false)
	, Priority(1.0f)
{
	MaxConcurrentPlayCount_DEPRECATED = 16;
}

void USoundBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (USoundBase::DefaultSoundClassObject == nullptr)
	{
		const FSoftObjectPath DefaultSoundClassName = GetDefault<UAudioSettings>()->DefaultSoundClassName;
		if (DefaultSoundClassName.IsValid())
		{
			USoundBase::DefaultSoundClassObject = LoadObject<USoundClass>(nullptr, *DefaultSoundClassName.ToString());
		}
	}
	SoundClassObject = USoundBase::DefaultSoundClassObject;

	if (USoundBase::DefaultSoundConcurrencyObject == nullptr)
	{
		const FSoftObjectPath DefaultSoundConcurrencyName = GetDefault<UAudioSettings>()->DefaultSoundConcurrencyName;
		if (DefaultSoundConcurrencyName.IsValid())
		{
			USoundBase::DefaultSoundConcurrencyObject = LoadObject<USoundConcurrency>(nullptr, *DefaultSoundConcurrencyName.ToString());
		}
	}
	SoundConcurrencySettings = USoundBase::DefaultSoundConcurrencyObject;

}

bool USoundBase::IsPlayable() const
{
	return false;
}

const FSoundAttenuationSettings* USoundBase::GetAttenuationSettingsToApply() const
{
	if (AttenuationSettings)
	{
		return &AttenuationSettings->Attenuation;
	}
	return nullptr;
}

float USoundBase::GetMaxAudibleDistance()
{
	return 0.f;
}

float USoundBase::GetDuration()
{
	return Duration;
}

float USoundBase::GetVolumeMultiplier()
{
	return 1.f;
}

float USoundBase::GetPitchMultiplier()
{
	return 1.f;
}

bool USoundBase::IsLooping()
{ 
	return (GetDuration() >= INDEFINITELY_LOOPING_DURATION); 
}

bool USoundBase::ShouldApplyInteriorVolumes() const
{
	return (SoundClassObject && SoundClassObject->Properties.bApplyAmbientVolumes);
}

USoundClass* USoundBase::GetSoundClass() const
{
	return SoundClassObject;
}

USoundSubmix* USoundBase::GetSoundSubmix() const
{
	return SoundSubmixObject;
}

void USoundBase::GetSoundSubmixSends(TArray<FSoundSubmixSendInfo>& OutSends) const
{
	OutSends = SoundSubmixSends;
}

void USoundBase::GetSoundSourceBusSends(TArray<FSoundSourceBusSendInfo>& OutSends) const
{
	OutSends = BusSends;
}

const FSoundConcurrencySettings* USoundBase::GetSoundConcurrencySettingsToApply()
{
	if (bOverrideConcurrency)
	{
		return &ConcurrencyOverrides;
	}
	else if (SoundConcurrencySettings)
	{
		return &SoundConcurrencySettings->Concurrency;
	}
	return nullptr;
}

float USoundBase::GetPriority() const
{
	return FMath::Clamp(Priority, MIN_SOUND_PRIORITY, MAX_SOUND_PRIORITY);
}

uint32 USoundBase::GetSoundConcurrencyObjectID() const
{
	if (SoundConcurrencySettings != nullptr && !bOverrideConcurrency)
	{
		return SoundConcurrencySettings->GetUniqueID();
	}
	return 0;
}

void USoundBase::PostLoad()
{
	Super::PostLoad();

	const int32 LinkerUE4Version = GetLinkerUE4Version();

	if (LinkerUE4Version < VER_UE4_SOUND_CONCURRENCY_PACKAGE)
	{
		bOverrideConcurrency = true;
		ConcurrencyOverrides.bLimitToOwner = false;
		ConcurrencyOverrides.MaxCount = FMath::Max(MaxConcurrentPlayCount_DEPRECATED, 1);
		ConcurrencyOverrides.ResolutionRule = MaxConcurrentResolutionRule_DEPRECATED;
		ConcurrencyOverrides.VolumeScale = 1.0f;
	}
}

