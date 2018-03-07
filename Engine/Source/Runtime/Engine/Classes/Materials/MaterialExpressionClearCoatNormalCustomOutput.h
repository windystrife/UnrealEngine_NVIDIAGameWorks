// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionClearCoatNormalCustomOutput.generated.h"

UCLASS()
class UMaterialExpressionClearCoatNormalCustomOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Input;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

	virtual FExpressionInput* GetInput(int32 InputIndex) override;


	// Begin UObject Interface
#if WITH_EDITOR

	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Float3; }
#endif


	virtual FString GetFunctionName() const override { return TEXT("ClearCoatBottomNormal"); }

};



