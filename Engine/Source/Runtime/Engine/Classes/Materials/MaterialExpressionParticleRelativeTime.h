// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MaterialExpressionParticleRelativeTime: Exposes the relative time of a
		particle to the material editor.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionParticleRelativeTime.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionParticleRelativeTime : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



