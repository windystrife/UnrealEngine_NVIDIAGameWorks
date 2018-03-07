// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_SubInstance.generated.h"

struct FAnimInstanceProxy;

USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_SubInstance : public FAnimNode_Base
{
	GENERATED_BODY()

public:

	FAnimNode_SubInstance();

	/** 
	 *  Input pose for the node, intentionally not accessible because if there's no input
	 *  Node in the target class we don't want to show this as a pin
	 */
	UPROPERTY()
	FPoseLink InPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TSubclassOf<UAnimInstance> InstanceClass;

	/** This is the actual instance allocated at runtime that will run */
	UPROPERTY(transient)
	UAnimInstance* InstanceToRun;

	/** List of properties on the calling instance to push from */
	UPROPERTY(transient)
	TArray<UProperty*> InstanceProperties;

	/** List of properties on the sub instance to push to, built from name list when initialised */
	UPROPERTY(transient)
	TArray<UProperty*> SubInstanceProperties;

	/** List of source properties to use, 1-1 with Dest names below, built by the compiler */
	UPROPERTY()
	TArray<FName> SourcePropertyNames;

	/** List of destination properties to use, 1-1 with Source names above, built by the compiler */
	UPROPERTY()
	TArray<FName> DestPropertyNames;

	// Temporary storage for the output of the subinstance, will be copied into output pose.
	// Declared at member level to avoid reallocating the objects
	TArray<FTransform> BoneTransforms;
	FBlendedHeapCurve BlendedCurve;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

protected:
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

	// Shutdown the currently running instance
	void TeardownInstance();

	// Makes sure the bone transforms array can contain the pose information from the provided anim instance
	void AllocateBoneTransforms(const UAnimInstance* InAnimInstance);
};
