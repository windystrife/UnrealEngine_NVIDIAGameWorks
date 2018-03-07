// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionBlendMaterialAttributes.generated.h"

UENUM()
namespace EMaterialAttributeBlend
{
	enum Type
	{
		Blend,
		UseA,
		UseB
	};
}

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionBlendMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
 	FMaterialAttributesInput A;

	UPROPERTY()
 	FMaterialAttributesInput B;

	UPROPERTY()
	FExpressionInput Alpha;

	// Optionally skip blending attributes of this type.
	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TEnumAsByte<EMaterialAttributeBlend::Type> PixelAttributeBlendType;

	// Optionally skip blending attributes of this type.
	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TEnumAsByte<EMaterialAttributeBlend::Type> VertexAttributeBlendType;

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
	virtual uint32 GetInputType(int32 InputIndex) override {return InputIndex == 2 ? MCT_Float1 : MCT_MaterialAttributes;}
#endif
	//~ End UMaterialExpression Interface
};
