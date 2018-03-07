// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateShaders.h"
#include "RenderingCommon.h"

/** Flag to determine if we are running with a color vision deficiency shader on */
uint32 GSlateShaderColorVisionDeficiencyType = 0;

IMPLEMENT_SHADER_TYPE(, FSlateElementVS, TEXT("/Engine/Private/SlateVertexShader.usf"), TEXT("Main"), SF_Vertex);

IMPLEMENT_SHADER_TYPE(, FSlateDebugOverdrawPS, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("DebugOverdrawMain"), SF_Pixel );

IMPLEMENT_SHADER_TYPE(, FSlatePostProcessBlurPS, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("GaussianBlurMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSlatePostProcessDownsamplePS, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("DownsampleMain"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FSlateMaskingVS, TEXT("/Engine/Private/SlateMaskingShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FSlateMaskingPS, TEXT("/Engine/Private/SlateMaskingShader.usf"), TEXT("MainPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FSlateDebugBatchingPS, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("DebugBatchingMain"), SF_Pixel );

#define IMPLEMENT_SLATE_PIXELSHADER_TYPE(ShaderType, bDrawDisabledEffect, bUseTextureAlpha) \
	typedef TSlateElementPS<ESlateShader::ShaderType,bDrawDisabledEffect,bUseTextureAlpha> TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha##A; \
	IMPLEMENT_SHADER_TYPE(template<>,TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha##A,TEXT("/Engine/Private/SlateElementPixelShader.usf"),TEXT("Main"),SF_Pixel);

/**
* All the different permutations of shaders used by slate. Uses #defines to avoid dynamic branches
*/
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, true, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(Font, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Font, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment, true, true);

/** The Slate vertex declaration. */
TGlobalResource<FSlateVertexDeclaration> GSlateVertexDeclaration;
TGlobalResource<FSlateInstancedVertexDeclaration> GSlateInstancedVertexDeclaration;
TGlobalResource<FSlateMaskingVertexDeclaration> GSlateMaskingVertexDeclaration;


/************************************************************************/
/* FSlateVertexDeclaration                                              */
/************************************************************************/
void FSlateVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;
	uint32 Stride = sizeof(FSlateVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, TexCoords), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, MaterialTexCoords), VET_Float2, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Position), VET_Float2, 2, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Color), VET_Color, 3, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, PixelSize), VET_UShort2, 4, Stride));

	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
}

void FSlateVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}


/************************************************************************/
/* FSlateInstancedVertexDeclaration                                     */
/************************************************************************/
void FSlateInstancedVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;
	uint32 Stride = sizeof(FSlateVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, TexCoords), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, MaterialTexCoords), VET_Float2, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Position), VET_Float2, 2, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Color), VET_Color, 3, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, PixelSize), VET_UShort2, 4, Stride));
	Elements.Add(FVertexElement(1, 0, VET_Float4, 5, sizeof(FVector4), true));
	
	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
}

void FSlateElementPS::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
	OutEnvironment.SetDefine(TEXT("USE_709"), CVar ? (CVar->GetValueOnGameThread() == 1) : 1);
}


/************************************************************************/
/* FSlateMaskingVertexDeclaration                                              */
/************************************************************************/
void FSlateMaskingVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;
	uint32 Stride = sizeof(FVector2D);
	Elements.Add(FVertexElement(0, 0, VET_Float2, 0, Stride));

	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
}

void FSlateMaskingVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}


/************************************************************************/
/* FSlateDefaultVertexShader                                            */
/************************************************************************/

FSlateElementVS::FSlateElementVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
	: FGlobalShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	VertexShaderParams.Bind( Initializer.ParameterMap, TEXT("VertexShaderParams"));
	SwitchVerticalAxisMultiplier.Bind( Initializer.ParameterMap, TEXT("SwitchVerticalAxisMultiplier"));
}

void FSlateElementVS::SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection )
{
	SetShaderValue(RHICmdList, GetVertexShader(), ViewProjection, InViewProjection );
}

void FSlateElementVS::SetShaderParameters(FRHICommandList& RHICmdList, const FVector4& ShaderParams )
{
	SetShaderValue(RHICmdList, GetVertexShader(), VertexShaderParams, ShaderParams );
}

void FSlateElementVS::SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier )
{
	SetShaderValue(RHICmdList, GetVertexShader(), SwitchVerticalAxisMultiplier, InMultiplier );
}

void FSlateElementVS::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
}

/** Serializes the shader data */
bool FSlateElementVS::Serialize( FArchive& Ar )
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

	Ar << ViewProjection;
	Ar << VertexShaderParams;
	Ar << SwitchVerticalAxisMultiplier;

	return bShaderHasOutdatedParameters;
}


/************************************************************************/
/* FSlateMaskingVS                                            */
/************************************************************************/

FSlateMaskingVS::FSlateMaskingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	SwitchVerticalAxisMultiplier.Bind(Initializer.ParameterMap, TEXT("SwitchVerticalAxisMultiplier"));
}

void FSlateMaskingVS::SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection)
{
	SetShaderValue(RHICmdList, GetVertexShader(), ViewProjection, InViewProjection);
}

void FSlateMaskingVS::SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier )
{
	SetShaderValue(RHICmdList, GetVertexShader(), SwitchVerticalAxisMultiplier, InMultiplier );
}

/** Serializes the shader data */
bool FSlateMaskingVS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << ViewProjection;
	Ar << SwitchVerticalAxisMultiplier;

	return bShaderHasOutdatedParameters;
}