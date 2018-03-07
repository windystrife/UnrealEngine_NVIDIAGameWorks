// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/InputScaleBias.h"

/////////////////////////////////////////////////////
// FInputScaleBias

float FInputScaleBias::ApplyTo(float Value) const
{
	return FMath::Clamp<float>( Value * Scale + Bias, 0.0f, 1.0f );
}
