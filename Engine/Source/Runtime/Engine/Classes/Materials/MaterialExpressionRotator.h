// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionRotator.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionRotator : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinate;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to Game Time if not specified"))
	FExpressionInput Time;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionRotator)
	float CenterX;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionRotator)
	float CenterY;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionRotator)
	float Speed;

	/** only used if Coordinate is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionRotator)
	uint32 ConstCoordinate;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool NeedsRealtimePreview() override { return Time.Expression==NULL && Speed != 0.f; }
#endif
	//~ End UMaterialExpression Interface

};



