// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.cpp: Screen rendering implementation.
=============================================================================*/

#include "ScreenRendering.h"

/** Vertex declaration for screen-space rendering. */
TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

// Shader implementations.
IMPLEMENT_SHADER_TYPE(,FScreenPS,TEXT("/Engine/Private/ScreenPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FScreenVS,TEXT("/Engine/Private/ScreenVertexShader.usf"),TEXT("Main"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<> ,TScreenVSForGS<false>,TEXT("/Engine/Private/ScreenVertexShader.usf"),TEXT("MainForGS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<> ,TScreenVSForGS<true>,TEXT("/Engine/Private/ScreenVertexShader.usf"),TEXT("MainForGS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FScreenPS_OSE,TEXT("/Engine/Private/ScreenPixelShaderOES.usf"),TEXT("Main"),SF_Pixel);
