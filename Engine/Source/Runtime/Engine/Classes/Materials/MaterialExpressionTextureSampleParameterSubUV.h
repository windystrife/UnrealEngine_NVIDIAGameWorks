// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionTextureSampleParameterSubUV.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTextureSampleParameterSubUV : public UMaterialExpressionTextureSampleParameter2D
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSampleParameterSubUV)
	uint32 bBlend:1;


	//~ Begin UMaterialExpressionTextureSampleParameter Interface
	virtual bool TextureIsValid( UTexture* InTexture ) override;
	virtual const TCHAR* GetRequirements() override;
	//~ End UMaterialExpressionTextureSampleParameter Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



