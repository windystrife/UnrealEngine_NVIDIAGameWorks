// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "UpdateTextureShaders.h"

IMPLEMENT_SHADER_TYPE(,FUpdateTexture2DSubresouceCS,TEXT("/Engine/Private/UpdateTextureShaders.usf"),TEXT("UpdateTexture2DSubresourceCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(,FUpdateTexture3DSubresouceCS, TEXT("/Engine/Private/UpdateTextureShaders.usf"), TEXT("UpdateTexture3DSubresourceCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(,FCopyTexture2DCS,TEXT("/Engine/Private/UpdateTextureShaders.usf"),TEXT("CopyTexture2DCS"),SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TCopyDataCS<2>, TEXT("/Engine/Private/UpdateTextureShaders.usf"), TEXT("CopyData2CS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TCopyDataCS<1>, TEXT("/Engine/Private/UpdateTextureShaders.usf"), TEXT("CopyData1CS"), SF_Compute);
