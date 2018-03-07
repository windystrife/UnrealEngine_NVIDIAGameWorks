// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


struct FComposureUtils
{

/* Disables the showflags to a scene capture post processing only. */
COMPOSURE_API static void SetEngineShowFlagsForPostprocessingOnly(struct FEngineShowFlags& InOutEngineShowflags);


/** Returns the red and green channel fringe factors from percentage of chromatic aberration. */
COMPOSURE_API static FVector2D GetRedGreenUVFactorsFromChromaticAberration(float ChromaticAberrationAmount);

};
