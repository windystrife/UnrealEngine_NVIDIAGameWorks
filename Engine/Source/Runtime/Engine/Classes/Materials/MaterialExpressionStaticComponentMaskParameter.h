// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionStaticComponentMaskParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionStaticComponentMaskParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultR:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultG:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultB:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultA:1;


public:

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, bool& OutR, bool& OutG, bool& OutB, bool& OutA, FGuid&OutExpressionGuid) const;
};



