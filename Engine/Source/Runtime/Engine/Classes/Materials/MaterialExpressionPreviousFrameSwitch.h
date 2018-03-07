// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPreviousFrameSwitch.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionPreviousFrameSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput CurrentFrame;

	UPROPERTY()
	FExpressionInput PreviousFrame;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif // WITH_EDITOR
	virtual FString GetInputName(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 OutputIndex) override {return MCT_Unknown;}
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



