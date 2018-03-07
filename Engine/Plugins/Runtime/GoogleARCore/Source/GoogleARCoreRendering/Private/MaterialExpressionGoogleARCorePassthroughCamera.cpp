// Copyright 2017 Google Inc.

#include "MaterialExpressionGoogleARCorePassthroughCamera.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"
#include "GoogleARCorePassthroughCameraExternalTextureGuid.h"

UMaterialExpressionGoogleARCorePassthroughCamera::UMaterialExpressionGoogleARCorePassthroughCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
int32 UMaterialExpressionGoogleARCorePassthroughCamera::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->TextureSample(
		Compiler->ExternalTexture(GoogleARCorePassthroughCameraExternalTextureGuid),
		Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false),
		EMaterialSamplerType::SAMPLERTYPE_Color);
}

int32 UMaterialExpressionGoogleARCorePassthroughCamera::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return INDEX_NONE;
}

void UMaterialExpressionGoogleARCorePassthroughCamera::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("GoogleARCore Passthrough Camera"));
}
#endif
