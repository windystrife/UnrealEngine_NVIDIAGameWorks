// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

struct FColor;

class FMaterialBakingHelpers
{
public:
	/** Applies a box blur to magenta pixels found in given texture represented by InBMP using non-magenta pixels, this creates a smear across the magenta/filled pixels */
	static void PerformUVBorderSmear(TArray<FColor>& InOutPixels, int32 ImageWidth, int32 ImageHeight, bool bIsNormalMap);	
protected:
	static FColor BoxBlurSample(TArray<FColor>& InBMP, int32 X, int32 Y, int32 InImageWidth, int32 InImageHeight, bool bIsNormalMap);	
};