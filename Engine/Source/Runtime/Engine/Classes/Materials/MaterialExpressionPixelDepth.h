// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Gives the depth of the current pixel being drawn for use in a material
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPixelDepth.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionPixelDepth : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



