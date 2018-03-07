// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSetMaterialAttributes.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionSetMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FExpressionInput> Inputs;

	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TArray<FGuid> AttributeSetTypes;
#if WITH_EDITOR
	TArray<FGuid> PreEditAttributeSetTypes;
#endif

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs()override;
	virtual FExpressionInput* GetInput(int32 InputIndex)override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override {return true;}
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return true;}
	virtual uint32 GetInputType(int32 InputIndex) override {return InputIndex == 0 ? MCT_MaterialAttributes : MCT_Float3;}
#endif
	//~ End UMaterialExpression Interface
};
