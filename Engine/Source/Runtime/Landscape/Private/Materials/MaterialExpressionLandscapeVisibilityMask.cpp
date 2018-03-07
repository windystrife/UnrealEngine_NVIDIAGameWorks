// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeVisibilityMask
///////////////////////////////////////////////////////////////////////////////

FName UMaterialExpressionLandscapeVisibilityMask::ParameterName = FName("__LANDSCAPE_VISIBILITY__");

UMaterialExpressionLandscapeVisibilityMask::UMaterialExpressionLandscapeVisibilityMask(const FObjectInitializer& ObjectInitializer)
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

FGuid& UMaterialExpressionLandscapeVisibilityMask::GetParameterExpressionId()
{
	return ExpressionGUID;
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeVisibilityMask::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 MaskLayerCode = Compiler->StaticTerrainLayerWeight(ParameterName, Compiler->Constant(0.f));
	return MaskLayerCode == INDEX_NONE ? Compiler->Constant(1.f) : Compiler->Sub(Compiler->Constant(1.f), MaskLayerCode);
}
#endif // WITH_EDITOR

UTexture* UMaterialExpressionLandscapeVisibilityMask::GetReferencedTexture()
{
	return GEngine->WeightMapPlaceholderTexture;
}

void UMaterialExpressionLandscapeVisibilityMask::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if (CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}

#if WITH_EDITOR
void UMaterialExpressionLandscapeVisibilityMask::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Landscape Visibility Mask")));
}
#endif // WITH_EDITOR

bool UMaterialExpressionLandscapeVisibilityMask::NeedsLoadForClient() const
{
	return ParameterName != NAME_None;
}

#undef LOCTEXT_NAMESPACE
