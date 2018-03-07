// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeSoundClass.h"
#include "ActiveSound.h"

/*-----------------------------------------------------------------------------
	USoundNodeSoundClass implementation.
-----------------------------------------------------------------------------*/

USoundNodeSoundClass::USoundNodeSoundClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeSoundClass::ParseNodes( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FSoundParseParameters UpdatedParseParams = ParseParams;
	if (SoundClassOverride)
	{
		UpdatedParseParams.SoundClass = SoundClassOverride;
	}

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParseParams, WaveInstances );
}
