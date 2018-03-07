// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerWeight
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerWeight::UMaterialExpressionLandscapeLayerWeight(const FObjectInitializer& ObjectInitializer)
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

	PreviewWeight = 0.0f;
	ConstBase = FVector(0.f, 0.f, 0.f);
}


FGuid& UMaterialExpressionLandscapeLayerWeight::GetParameterExpressionId()
{
	return ExpressionGUID;
}


void UMaterialExpressionLandscapeLayerWeight::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_FIXUP_TERRAIN_LAYER_NODES)
	{
		UpdateParameterGuid(true, true);
	}
}

#if WITH_EDITOR
bool UMaterialExpressionLandscapeLayerWeight::IsResultMaterialAttributes(int32 OutputIndex)
{
	if (ContainsInputLoop())
	{
		// If there is a loop anywhere in this expression's inputs then we can't risk checking them
		return false;
	}
	bool bLayerIsMaterialAttributes = Layer.Expression != nullptr && Layer.Expression->IsResultMaterialAttributes(Layer.OutputIndex);
	bool bBaseIsMaterialAttributes = Base.Expression != nullptr && Base.Expression->IsResultMaterialAttributes(Base.OutputIndex);
	return bLayerIsMaterialAttributes || bBaseIsMaterialAttributes;
}

int32 UMaterialExpressionLandscapeLayerWeight::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const int32 BaseCode = Base.Expression ? Base.Compile(Compiler) : Compiler->Constant3(ConstBase.X, ConstBase.Y, ConstBase.Z);
	const int32 WeightCode = Compiler->StaticTerrainLayerWeight(ParameterName, Compiler->Constant(PreviewWeight));

	int32 ReturnCode = INDEX_NONE;
	if (WeightCode == INDEX_NONE)
	{
		ReturnCode = BaseCode;
	}
	else
	{
		const int32 LayerCode = Layer.Compile(Compiler);
		ReturnCode = Compiler->Add(BaseCode, Compiler->Mul(LayerCode, WeightCode));
	}

	if (ReturnCode != INDEX_NONE && //If we've already failed for some other reason don't bother with this check. It could have been the reentrant check causing this to loop infinitely!
		Layer.Expression != nullptr && Base.Expression != nullptr &&
		Layer.Expression->IsResultMaterialAttributes(Layer.OutputIndex) != Base.Expression->IsResultMaterialAttributes(Base.OutputIndex))
	{
		Compiler->Error(TEXT("Cannot mix MaterialAttributes and non MaterialAttributes nodes"));
	}

	return ReturnCode;
}
#endif // WITH_EDITOR

UTexture* UMaterialExpressionLandscapeLayerWeight::GetReferencedTexture()
{
	return GEngine->WeightMapPlaceholderTexture;
}

#if WITH_EDITOR
void UMaterialExpressionLandscapeLayerWeight::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Layer '%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

void UMaterialExpressionLandscapeLayerWeight::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if (CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}

bool UMaterialExpressionLandscapeLayerWeight::NeedsLoadForClient() const
{
	return ParameterName != NAME_None;
}

#undef LOCTEXT_NAMESPACE
