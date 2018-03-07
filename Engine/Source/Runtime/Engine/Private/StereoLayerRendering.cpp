// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.cpp: Screen rendering implementation.
=============================================================================*/

#include "StereoLayerRendering.h"

IMPLEMENT_SHADER_TYPE(,FStereoLayerVS,TEXT("/Engine/Private/StereoLayerShader.usf"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FStereoLayerPS,TEXT("/Engine/Private/StereoLayerShader.usf"),TEXT("MainPS"),SF_Pixel);
