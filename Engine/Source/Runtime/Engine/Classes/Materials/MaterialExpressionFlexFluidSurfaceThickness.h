// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSceneColor.h"
#include "MaterialExpressionFlexFluidSurfaceThickness.generated.h"

UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionFlexFluidSurfaceThickness : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/**
	* Coordinates - UV coordinates to apply to the scene color lookup.
	* OffsetFraction - 	An offset to apply to the scene color lookup in a 2d fraction of the screen.
	*/
	UPROPERTY(EditAnywhere, Category = MaterialExpressionFlexFluidSurfaceThickness)
	TEnumAsByte<enum EMaterialSceneAttributeInputMode::Type> InputMode;

	/**
	* Based on the input mode the input will be treated as either:
	* UV coordinates to apply to the scene color lookup or
	* an offset to apply to the fluid thickness, in a 2d fraction of the screen.
	*/
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstInput' if not specified"))
	FExpressionInput Input;

	/** only used if Input is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionFlexFluidSurfaceThickness)
	FVector2D ConstInput;

	// Begin UMaterialExpression Interface
#if WITH_EDITOR	
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR

	virtual FString GetInputName(int32 InputIndex) const override;
	// End UMaterialExpression Interface
};



