// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerSample
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerSample::UMaterialExpressionLandscapeLayerSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Landscape;
		FConstructorStatics()
			: NAME_Landscape(LOCTEXT("Landscape", "Landscape"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bIsParameterExpression = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Landscape);
#endif
}


FGuid& UMaterialExpressionLandscapeLayerSample::GetParameterExpressionId()
{
	return ExpressionGUID;
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeLayerSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const int32 WeightCode = Compiler->StaticTerrainLayerWeight(ParameterName, Compiler->Constant(PreviewWeight));
	if (WeightCode == INDEX_NONE)
	{
		// layer is not used in this component, sample value is 0.
		return Compiler->Constant(0.f);
	}
	else
	{
		return WeightCode;
	}
}
#endif // WITH_EDITOR

UTexture* UMaterialExpressionLandscapeLayerSample::GetReferencedTexture()
{
	return GEngine->WeightMapPlaceholderTexture;
}

#if WITH_EDITOR
void UMaterialExpressionLandscapeLayerSample::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Sample '%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

void UMaterialExpressionLandscapeLayerSample::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	if (!OutParameterNames.Contains(ParameterName))
	{
		OutParameterNames.Add(ParameterName);
		OutParameterIds.Add(ExpressionGUID);
	}
}

bool UMaterialExpressionLandscapeLayerSample::NeedsLoadForClient() const
{
	return ParameterName != NAME_None;
}

#undef LOCTEXT_NAMESPACE
