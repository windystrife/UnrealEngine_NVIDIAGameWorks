// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeModulatorContinuous.h"
#include "ActiveSound.h"

float FModulatorContinuousParams::GetValue(const FActiveSound& ActiveSound) const
{
	float ParamFloat = 0.f;

	if (!ActiveSound.GetFloatParameter(ParameterName, ParamFloat))
	{
		ParamFloat = Default;
	}

	if(ParamMode == MPM_Direct)
	{
		return ParamFloat;
	}
	else if(ParamMode == MPM_Abs)
	{
		ParamFloat = FMath::Abs(ParamFloat);
	}

	float Gradient;
	if(MaxInput <= MinInput)
	{
		Gradient = 0.f;
	}
	else
	{
		Gradient = (MaxOutput - MinOutput)/(MaxInput - MinInput);
	}

	const float ClampedParam = FMath::Clamp(ParamFloat, MinInput, MaxInput);

	return MinOutput + ((ClampedParam - MinInput) * Gradient);
}

/*-----------------------------------------------------------------------------
	USoundNodeModulatorContinuous implementation.
-----------------------------------------------------------------------------*/
USoundNodeModulatorContinuous::USoundNodeModulatorContinuous(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeModulatorContinuous::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FSoundParseParameters UpdatedParams = ParseParams;
	UpdatedParams.Volume *= VolumeModulationParams.GetValue( ActiveSound );;
	UpdatedParams.Pitch *= PitchModulationParams.GetValue( ActiveSound );

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances );
}

