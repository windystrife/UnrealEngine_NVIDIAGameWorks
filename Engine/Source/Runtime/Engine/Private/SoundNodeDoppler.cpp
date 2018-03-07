// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeDoppler.h"
#include "ActiveSound.h"

/*-----------------------------------------------------------------------------
         USoundNodeDoppler implementation.
-----------------------------------------------------------------------------*/
USoundNodeDoppler::USoundNodeDoppler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DopplerIntensity = 1.0f;
}

void USoundNodeDoppler::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FSoundParseParameters UpdatedParams = ParseParams;
	UpdatedParams.Pitch *= GetDopplerPitchMultiplier(AudioDevice->GetListeners()[0], ParseParams.Transform.GetTranslation(), ParseParams.Velocity);

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances );
}

float USoundNodeDoppler::GetDopplerPitchMultiplier(FListener const& InListener, const FVector Location, const FVector Velocity) const
{
	static const float SpeedOfSoundInAirAtSeaLevel = 33000.f;		// cm/sec

	FVector const SourceToListenerNorm = (InListener.Transform.GetTranslation() - Location).GetSafeNormal();

	// find source and listener speeds along the line between them
	float const SourceVelMagTorwardListener = Velocity | SourceToListenerNorm;
	float const ListenerVelMagAwayFromSource = InListener.Velocity | SourceToListenerNorm;

	// multiplier = 1 / (1 - ((sourcevel - listenervel) / speedofsound) );
	float const InvDopplerPitchScale = 1.f - ( (SourceVelMagTorwardListener - ListenerVelMagAwayFromSource) / SpeedOfSoundInAirAtSeaLevel );
	float const PitchScale = 1.f / InvDopplerPitchScale;

	// factor in user-specified intensity
	float const FinalPitchScale = ((PitchScale - 1.f) * DopplerIntensity) + 1.f;

	//UE_LOG(LogAudio, Log, TEXT("Applying doppler pitchscale %f, raw scale %f, deltaspeed was %f"), FinalPitchScale, PitchScale, ListenerVelMagAwayFromSource - SourceVelMagTorwardListener);

	return FinalPitchScale;
}
