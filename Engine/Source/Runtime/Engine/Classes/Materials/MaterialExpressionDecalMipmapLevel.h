// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDecalMipmapLevel.generated.h"

UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionDecalMipmapLevel : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The texture's size */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to '(Const Width, Const Height)' if not specified"))
	FExpressionInput TextureSize;

	/** only used if TextureSize is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionDecalMipmapLevel, meta=(OverridingInputProperty = "TextureSize"))
	float ConstWidth;
	UPROPERTY(EditAnywhere, Category = MaterialExpressionDecalMipmapLevel, meta=(OverridingInputProperty = "TextureSize"))
	float ConstHeight;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};
