// NVCHANGE_BEGIN: Add VXGI

#pragma once
#include "MaterialExpressionVxgiTraceCone.generated.h"

UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionVxgiTraceCone : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Position of the cone vertex"))
	FExpressionInput StartPos;

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Direction of the cone, can be non-normalized"))
	FExpressionInput Direction;

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Double tangent of half cone angle, or sample size divided by distance"))
	FExpressionInput ConeFactor;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Distance between the cone vertex and the first sample, measured in voxels"))
	FExpressionInput InitialOffset;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Distance between consecutive samples, measured in fractions of cone width, default is 1"))
	FExpressionInput TracingStep;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionVxgiTraceCone, meta=(ToolTip = "Maximum number of samples to take for a cone"))
	float MaxSamples;

	// Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool NeedsRealtimePreview() override { return true; }
	virtual uint32 GetOutputType(int32 OutputIndex) { return MCT_Float3; }
#endif
	// End UMaterialExpression Interface
};

// NVCHANGE_END: Add VXGI
