// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSimple.h"
#include "ActiveSound.h"

bool USoundSimple::IsPlayable() const
{
	return true;
}

void USoundSimple::Parse(class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances)
{
	FWaveInstance* WaveInstance = ActiveSound.FindWaveInstance(NodeWaveInstanceHash);
	if (!WaveInstance)
	{
		ChooseSoundWave();
	}

	// Forward the parse to the chosen sound wave
	check(SoundWave);
	SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
}

void USoundSimple::ChooseSoundWave()
{
	float ProbabilitySum = 0.0f;
	for (int32 i = 0; i < Variations.Num(); ++i)
	{
		ProbabilitySum += Variations[i].ProbabilityWeight;
	}

	float Choice = FMath::FRandRange(0.0f, ProbabilitySum);

	// Find the index chosen
	ProbabilitySum = 0.0f;
	int32 ChosenIndex = 0;
	for (int32 i = 0; i < Variations.Num(); ++i)
	{
		float NextSum = ProbabilitySum + Variations[i].ProbabilityWeight;

		if (Choice >= ProbabilitySum && Choice < NextSum)
		{
			ChosenIndex = i;
			break;
		}

		ProbabilitySum = NextSum;
	}
	
	check(ChosenIndex < Variations.Num());
	FSoundVariation& SoundVariation = Variations[ChosenIndex];

	// Now choise the volume and pitch to use based on prob ranges
	float Volume = FMath::FRandRange(SoundVariation.VolumeRange[0], SoundVariation.VolumeRange[1]);
	float Pitch = FMath::FRandRange(SoundVariation.PitchRange[0], SoundVariation.PitchRange[1]);

	// Assign the sound wave value to the transient sound wave ptr
	SoundWave = SoundVariation.SoundWave;
	SoundWave->Volume = Volume;
	SoundWave->Pitch = Pitch;
}

float USoundSimple::GetMaxAudibleDistance()
{
	float MaxDistance = 0.0f;
	for (int32 i = 0; i < Variations.Num(); ++i)
	{
		float SoundWaveMaxAudibleDistance = Variations[i].SoundWave->GetMaxAudibleDistance();
		if (SoundWaveMaxAudibleDistance > MaxDistance)
		{
			MaxDistance = SoundWaveMaxAudibleDistance;
		}
	}
	return MaxDistance;
}

float USoundSimple::GetDuration()
{
	float MaxDuration = 0.0f;
	for (int32 i = 0; i < Variations.Num(); ++i)
	{
		float SoundWaveMaxDuration = Variations[i].SoundWave->GetDuration();
		if (SoundWaveMaxDuration > MaxDuration)
		{
			MaxDuration = SoundWaveMaxDuration;
		}
	}
	return MaxDuration;	
}