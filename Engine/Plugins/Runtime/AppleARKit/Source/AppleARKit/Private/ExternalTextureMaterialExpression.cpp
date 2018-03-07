// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExternalTextureMaterialExpression.h"
#include "MaterialCompiler.h"
#include "ExternalTextureGuid.h"

UMaterialExpressionARKitPassthroughCamera::UMaterialExpressionARKitPassthroughCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

#if WITH_EDITOR
int32 UMaterialExpressionARKitPassthroughCamera::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->TextureSample(
		Compiler->ExternalTexture((TextureType == TextureY) ? ARKitPassthroughCameraExternalTextureYGuid : ARKitPassthroughCameraExternalTextureCbCrGuid),
		Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false),
		EMaterialSamplerType::SAMPLERTYPE_Color);
}

int32 UMaterialExpressionARKitPassthroughCamera::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return INDEX_NONE;
}

void UMaterialExpressionARKitPassthroughCamera::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ARKit Passthrough Camera"));
}
#endif