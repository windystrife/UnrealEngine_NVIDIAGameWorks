// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HdrCustomResolveShaders.h"

IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveVS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolveVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve2xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve2xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve4xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve4xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve8xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve8xPS"),SF_Pixel);

