// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionVertexInterpolator.generated.h"

UCLASS()
class UMaterialExpressionVertexInterpolator : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Input;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	int32 CompileInput(class FMaterialCompiler* Compiler, int32 AssignedInterpolatorIndex);
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Float4; }
	virtual bool HasCustomSourceOutput() override { return true; }
#endif

	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetFunctionName() const override { return TEXT("VertexInterpolator"); }
	virtual FString GetInputName(int32 InputIndex) const override { return TEXT("VS"); }

	int32				InterpolatorIndex;
	EMaterialValueType	InterpolatedType;
	int32				InterpolatorOffset;
};



