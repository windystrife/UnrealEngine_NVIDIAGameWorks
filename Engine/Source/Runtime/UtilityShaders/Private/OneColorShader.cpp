// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "OneColorShader.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FClearShaderUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, DrawColorMRT, [8] )
END_UNIFORM_BUFFER_STRUCT(FClearShaderUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FClearShaderUB, TEXT("ClearShaderUB"));

void FOneColorPS::SetColors(FRHICommandList& RHICmdList, const FLinearColor* Colors, int32 NumColors)
{
	check(NumColors <= MaxSimultaneousRenderTargets);

	auto& ClearUBParam = GetUniformBufferParameter<FClearShaderUB>();
	if (ClearUBParam.IsInitialized())
	{
		if (ClearUBParam.IsBound())
		{
			FClearShaderUB ClearData;
			FMemory::Memzero(ClearData.DrawColorMRT);			
			for (int32 i = 0; i < NumColors; ++i)
			{
				ClearData.DrawColorMRT[i].X = Colors[i].R;
				ClearData.DrawColorMRT[i].Y = Colors[i].G;
				ClearData.DrawColorMRT[i].Z = Colors[i].B;
				ClearData.DrawColorMRT[i].W = Colors[i].A;
			}

			FLocalUniformBuffer LocalUB = TUniformBufferRef<FClearShaderUB>::CreateLocalUniformBuffer(RHICmdList, ClearData, UniformBuffer_SingleFrame);	
			RHICmdList.SetLocalShaderUniformBuffer(GetPixelShader(), ClearUBParam.GetBaseIndex(), LocalUB);
		}
	}

	
}

// #define avoids a lot of code duplication
#define IMPLEMENT_ONECOLORVS(A,B) typedef TOneColorVS<A,B> TOneColorVS##A##B; \
IMPLEMENT_SHADER_TYPE2(TOneColorVS##A##B, SF_Vertex);

IMPLEMENT_ONECOLORVS(false,false)
IMPLEMENT_ONECOLORVS(false,true)
IMPLEMENT_ONECOLORVS(true,true)
IMPLEMENT_ONECOLORVS(true,false)
#undef IMPLEMENT_ONECOLORVS

IMPLEMENT_SHADER_TYPE(,FOneColorPS,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShader"),SF_Pixel);
// Compiling a version for every number of MRT's
// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<1>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<2>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<3>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<4>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<5>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<6>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<7>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<8>,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainPixelShaderMRT"),SF_Pixel);

IMPLEMENT_SHADER_TYPE(,FFillTextureCS,TEXT("/Engine/Private/OneColorShader.usf"),TEXT("MainFillTextureCS"),SF_Compute);
