// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionBentNormalCustomOutput.generated.h"

UCLASS()
class UMaterialExpressionBentNormalCustomOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Input;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Float3; }
#endif

	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual int32 GetNumOutputs() const override { return 1; }
	virtual FString GetFunctionName() const override { return TEXT("GetBentNormal"); }
	virtual FString GetDisplayName() const override { return TEXT("BentNormal"); }
};



