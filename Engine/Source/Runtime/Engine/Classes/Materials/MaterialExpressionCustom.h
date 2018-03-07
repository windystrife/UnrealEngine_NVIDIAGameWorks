// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionCustom.generated.h"

struct FPropertyChangedEvent;

UENUM()
enum ECustomMaterialOutputType
{
	CMOT_Float1,
	CMOT_Float2,
	CMOT_Float3,
	CMOT_Float4,
	CMOT_MAX,
};

USTRUCT()
struct FCustomInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CustomInput)
	FString InputName;

	UPROPERTY()
	FExpressionInput Input;

};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionCustom : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom, meta=(MultiLine=true))
	FString Code;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TEnumAsByte<enum ECustomMaterialOutputType> OutputType;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	FString Description;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TArray<struct FCustomInput> Inputs;


	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



