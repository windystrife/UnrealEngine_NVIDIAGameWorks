// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeLayerSwitch.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerSwitch
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerSwitch::UMaterialExpressionLandscapeLayerSwitch(const FObjectInitializer& ObjectInitializer)
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

	PreviewUsed = true;
	bCollapsed = false;
}

#if WITH_EDITOR
bool UMaterialExpressionLandscapeLayerSwitch::IsResultMaterialAttributes(int32 OutputIndex)
{
	if (ContainsInputLoop())
	{
		// If there is a loop anywhere in this expression's inputs then we can't risk checking them
		return false;
	}
	bool bLayerUsedIsMaterialAttributes = LayerUsed.Expression != nullptr && LayerUsed.Expression->IsResultMaterialAttributes(LayerUsed.OutputIndex);
	bool bLayerNotUsedIsMaterialAttributes = LayerNotUsed.Expression != nullptr && LayerNotUsed.Expression->IsResultMaterialAttributes(LayerNotUsed.OutputIndex);
	return bLayerUsedIsMaterialAttributes || bLayerNotUsedIsMaterialAttributes;
}

int32 UMaterialExpressionLandscapeLayerSwitch::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const int32 WeightCode = Compiler->StaticTerrainLayerWeight(
		ParameterName,
		PreviewUsed ? Compiler->Constant(1.0f) : INDEX_NONE
		);

	int32 ReturnCode = INDEX_NONE;
	if (WeightCode != INDEX_NONE)
	{
		ReturnCode = LayerUsed.Compile(Compiler);
	}
	else
	{
		ReturnCode = LayerNotUsed.Compile(Compiler);
	}

	if (ReturnCode != INDEX_NONE && //If we've already failed for some other reason don't bother with this check. It could have been the reentrant check causing this to loop infinitely!
		LayerUsed.Expression != nullptr && LayerNotUsed.Expression != nullptr &&
		LayerUsed.Expression->IsResultMaterialAttributes(LayerUsed.OutputIndex) != LayerNotUsed.Expression->IsResultMaterialAttributes(LayerNotUsed.OutputIndex))
	{
		Compiler->Error(TEXT("Cannot mix MaterialAttributes and non MaterialAttributes nodes"));
	}

	return ReturnCode;
}
#endif // WITH_EDITOR

UTexture* UMaterialExpressionLandscapeLayerSwitch::GetReferencedTexture()
{
	return GEngine->WeightMapPlaceholderTexture;
}

#if WITH_EDITOR
void UMaterialExpressionLandscapeLayerSwitch::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Layer Switch"));
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

void UMaterialExpressionLandscapeLayerSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_FIX_TERRAIN_LAYER_SWITCH_ORDER)
	{
		Swap(LayerUsed, LayerNotUsed);
	}
}


void UMaterialExpressionLandscapeLayerSwitch::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_FIXUP_TERRAIN_LAYER_NODES)
	{
		UpdateParameterGuid(true, true);
	}
}


FGuid& UMaterialExpressionLandscapeLayerSwitch::GetParameterExpressionId()
{
	return ExpressionGUID;
}


void UMaterialExpressionLandscapeLayerSwitch::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if (CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}

bool UMaterialExpressionLandscapeLayerSwitch::NeedsLoadForClient() const
{
	return ParameterName != NAME_None;
}

#undef LOCTEXT_NAMESPACE
