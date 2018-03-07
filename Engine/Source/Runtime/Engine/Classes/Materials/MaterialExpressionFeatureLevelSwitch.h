// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFeatureLevelSwitch.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionFeatureLevelSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Default connection, used when a certain feature level doesn't have an override. */
	UPROPERTY()
	FExpressionInput Default;

	UPROPERTY()
	FExpressionInput Inputs[ERHIFeatureLevel::Num];

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface.

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR

	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 OutputIndex) override {return MCT_Unknown;}
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};
