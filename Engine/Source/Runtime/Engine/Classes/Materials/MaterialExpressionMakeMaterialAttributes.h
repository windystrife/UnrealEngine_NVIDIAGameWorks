// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionMakeMaterialAttributes.generated.h"

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionMakeMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput BaseColor;

	UPROPERTY()
	FExpressionInput Metallic;

	UPROPERTY()
	FExpressionInput Specular;

	UPROPERTY()
	FExpressionInput Roughness;

	UPROPERTY()
	FExpressionInput EmissiveColor;

	UPROPERTY()
	FExpressionInput Opacity;

	UPROPERTY()
	FExpressionInput OpacityMask;

	UPROPERTY()
	FExpressionInput Normal;

	UPROPERTY()
	FExpressionInput WorldPositionOffset;

	UPROPERTY()
	FExpressionInput WorldDisplacement;

	UPROPERTY()
	FExpressionInput TessellationMultiplier;

	UPROPERTY()
	FExpressionInput SubsurfaceColor;

	UPROPERTY()
	FExpressionInput ClearCoat;

	UPROPERTY()
	FExpressionInput ClearCoatRoughness;

	UPROPERTY()
	FExpressionInput AmbientOcclusion;

	UPROPERTY()
	FExpressionInput Refraction;

	UPROPERTY()
	FExpressionInput CustomizedUVs[8];

	UPROPERTY()
	FExpressionInput PixelDepthOffset;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return true;}
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



