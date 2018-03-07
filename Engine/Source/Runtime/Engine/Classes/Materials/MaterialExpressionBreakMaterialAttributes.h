// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionBreakMaterialAttributes.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionBreakMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

 	UPROPERTY()
 	FMaterialAttributesInput MaterialAttributes;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
 
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs()override;
	virtual FExpressionInput* GetInput(int32 InputIndex)override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_MaterialAttributes;}
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



