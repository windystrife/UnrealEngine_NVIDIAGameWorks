// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_ModifyCurve.generated.h"

UENUM()
enum class EModifyCurveApplyMode : uint8
{
	/** Add new value to input curve value */
	Add,

	/** Scale input value by new value */
	Scale,

	/** Blend input with new curve value, using Alpha setting on the node */
	Blend
};

/** Easy way to modify curve values on a pose */
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_ModifyCurve : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModifyCurve)
	EModifyCurveApplyMode ApplyMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = ModifyCurve, meta = (PinShownByDefault))
	TArray<float> CurveValues;

	UPROPERTY()
	TArray<FName> CurveNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModifyCurve, meta = (PinShownByDefault))
	float Alpha;

	FAnimNode_ModifyCurve();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

#if WITH_EDITOR
	/** Add new curve being modified */
	void AddCurve(const FName& InName, float InValue);
	/** Remove a curve from being modified */
	void RemoveCurve(int32 PoseIndex);
#endif // WITH_EDITOR
};
