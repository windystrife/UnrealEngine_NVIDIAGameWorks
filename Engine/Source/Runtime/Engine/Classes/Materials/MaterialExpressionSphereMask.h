// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSphereMask.generated.h"

UCLASS()
class UMaterialExpressionSphereMask : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** 1 to 4 dimensional vector, should be the same type as B */
	UPROPERTY()
	FExpressionInput A;

	/** 1 to 4 dimensional vector, should be the same type as A */
	UPROPERTY()
	FExpressionInput B;

	/** in the units that A and B are measured, if not hooked up the internal constant is used */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'AttenuationRadius' if not specified"))
	FExpressionInput Radius;

	/** 0..1 for the range of 0\% to 100\%, if not hooked up the internal constant is used */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'HardnessPercent' if not specified"))
	FExpressionInput Hardness;

	/** in the unit that A and B are measured */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionSphereMask, meta=(OverridingInputProperty = "Radius"), DisplayName = "Radius")
	float AttenuationRadius;

	/** in percent 0%=soft .. 100%=hard */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionSphereMask, meta=(UIMin = "0.0", UIMax = "100.0", ClampMin = "0.0", ClampMax = "100.0", OverridingInputProperty = "Hardness", DisplayName = "Hardness"))
	float HardnessPercent;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

};



