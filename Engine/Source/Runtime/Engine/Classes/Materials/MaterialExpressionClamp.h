// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionClamp.generated.h"

UENUM()
enum EClampMode
{
	CMODE_Clamp,
	CMODE_ClampMin,
	CMODE_ClampMax,
};

UCLASS(MinimalAPI)
class UMaterialExpressionClamp : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MinDefault' if not specified"))
	FExpressionInput Min;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MaxDefault' if not specified"))
	FExpressionInput Max;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionClamp)
	TEnumAsByte<enum EClampMode> ClampMode;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionClamp, meta=(OverridingInputProperty = "Min"))
	float MinDefault;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionClamp, meta=(OverridingInputProperty = "Max"))
	float MaxDefault;


	//~ Begin UObject Interface
	virtual void Serialize( FArchive& Ar ) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



