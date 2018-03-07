// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusShaders.h"


IMPLEMENT_SHADER_TYPE(, FOculusVertexShader, TEXT("/Engine/Private/OculusShaders.usf"), TEXT("MainVertexShader"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FOculusWhiteShader, TEXT("/Engine/Private/OculusShaders.usf"), TEXT("MainWhiteShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOculusBlackShader, TEXT("/Engine/Private/OculusShaders.usf"), TEXT("MainBlackShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOculusAlphaInverseShader, TEXT("/Engine/Private/OculusShaders.usf"), TEXT("MainAlphaInverseShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOculusCubemapPS, TEXT("/Engine/Private/OculusShaders.usf"), TEXT("MainForCubemap"), SF_Pixel);
