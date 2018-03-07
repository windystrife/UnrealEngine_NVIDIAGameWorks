// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Synth2DSliderStyle.h"
#include "IPluginManager.h"
#include "SynthesisModule.h"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"

FSynth2DSliderStyle::FSynth2DSliderStyle()
{
}

FSynth2DSliderStyle::~FSynth2DSliderStyle()
{
}

void FSynth2DSliderStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
}

const FSynth2DSliderStyle& FSynth2DSliderStyle::GetDefault()
{
	static FSynth2DSliderStyle Default;
	return Default;
}

const FName FSynth2DSliderStyle::TypeName(TEXT("Synth2DSliderStyle"));