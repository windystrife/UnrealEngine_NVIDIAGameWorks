// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionGetMaterialAttributes.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionGetMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

 	UPROPERTY()
 	FMaterialAttributesInput MaterialAttributes;

	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TArray<FGuid> AttributeGetTypes;
#if WITH_EDITOR
	TArray<FGuid> PreEditAttributeGetTypes;
#endif

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
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
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_MaterialAttributes;}
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return OutputIndex == 0;}
#endif
	//~ End UMaterialExpression Interface
};
