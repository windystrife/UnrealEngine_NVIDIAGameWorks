// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MaterialExpressionTextureSampleParameterCube.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTextureSampleParameterCube : public UMaterialExpressionTextureSampleParameter
{
	GENERATED_UCLASS_BODY()




	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface

	//~ Begin UMaterialExpressionTextureSampleParameter Interface
	virtual bool TextureIsValid( UTexture* InTexture ) override;
	virtual const TCHAR* GetRequirements() override;
	virtual void SetDefaultTexture() override;
	//~ End UMaterialExpressionTextureSampleParameter Interface
};



