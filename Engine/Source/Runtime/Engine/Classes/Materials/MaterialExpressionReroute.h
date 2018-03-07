// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Absolute value material expression for user-defined materials
 *
 */

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionReroute.generated.h"

UCLASS(collapsecategories, hidecategories=Object, DisplayName = "Reroute")
class ENGINE_API UMaterialExpressionReroute : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Link to the input expression to be evaluated */
	UPROPERTY()
	FExpressionInput Input;

	/**
	 * Trace through the graph to find the first non Reroute node connected to this input. If there is a loop for some reason, we will bail out and return nullptr.
	 *
	 * @param OutputIndex The output index of the connection that was traced back.
	 * @return The final traced material expression.
	*/
	UMaterialExpression* TraceInputsToRealExpression(int32& OutputIndex) const;

	FExpressionInput TraceInputsToRealInput() const;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual FText GetCreationDescription() const override;
	virtual FText GetCreationName() const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface

private:
	FExpressionInput TraceInputsToRealExpressionInternal(TSet<FMaterialExpressionKey>& VisitedExpressions) const;

};



