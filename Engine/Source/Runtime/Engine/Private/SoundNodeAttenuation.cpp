// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeAttenuation.h"
#include "EngineDefines.h"
#include "ActiveSound.h"
#include "AudioDevice.h"

/*-----------------------------------------------------------------------------
	USoundNodeAttenuation implementation.
-----------------------------------------------------------------------------*/

USoundNodeAttenuation::USoundNodeAttenuation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float USoundNodeAttenuation::MaxAudibleDistance( float CurrentMaxDistance ) 
{ 
	float RadiusMax = WORLD_MAX;
	
	if (bOverrideAttenuation)
	{
		RadiusMax = AttenuationOverrides.GetMaxDimension();
	}
	else if (AttenuationSettings)
	{
		RadiusMax = AttenuationSettings->Attenuation.GetMaxDimension();
	}

	return FMath::Max<float>( CurrentMaxDistance, RadiusMax );
}

FSoundAttenuationSettings* USoundNodeAttenuation::GetAttenuationSettingsToApply()
{
	FSoundAttenuationSettings* Settings = nullptr;

	if (bOverrideAttenuation)
	{
		Settings = &AttenuationOverrides;
	}
	else if (AttenuationSettings)
	{
		Settings = &AttenuationSettings->Attenuation;
	}

	return Settings;
}

void USoundNodeAttenuation::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FSoundAttenuationSettings* Settings = (ActiveSound.bAllowSpatialization ? GetAttenuationSettingsToApply() : NULL);

	FSoundParseParameters UpdatedParseParams = ParseParams;
	if (Settings)
	{
		const FListener& Listener = AudioDevice->GetListeners()[0];

		// Update this node's attenuation settings overrides
		ActiveSound.ApplyAttenuation(UpdatedParseParams, Listener, Settings);
	}
	else
	{
		UpdatedParseParams.bUseSpatialization = false;
	}

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParseParams, WaveInstances );
}
