// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionScalarParameter.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionScalarParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float DefaultValue;

	/** 
	 * Sets the lower bound for the slider on this parameter in the material instance editor. 
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMin;

	/** 
	 * Sets the upper bound for the slider on this parameter in the material instance editor. 
	 * The slider will be disabled if SliderMax <= SliderMin.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMax;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, float& OutValue) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};



