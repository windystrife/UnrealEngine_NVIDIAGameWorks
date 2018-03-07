// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
SeparableSSS.h: Computing the kernel for the Separable Screen Space Subsurface Scattering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

// @param TargetBuffer, needs to be preallocated with TargetBufferSize (RGB is the sample weight, A is the offset), [0] is the center samples, following elements need to be mirrored with A, -A
// @param TargetBufferSize >0
// @parma SubsurfaceColor see SubsurfaceProfile.h
// @parma FalloffColor see SubsurfaceProfile.h
void ComputeMirroredSSSKernel(FLinearColor* TargetBuffer, uint32 TargetBufferSize, FLinearColor SubsurfaceColor, FLinearColor FalloffColor);


