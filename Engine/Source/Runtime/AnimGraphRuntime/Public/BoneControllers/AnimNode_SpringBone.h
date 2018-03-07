// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_SpringBone.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;

/**
 *	Simple controller that replaces or adds to the translation/rotation of a single bone.
 */

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_SpringBone : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category=Spring) 
	FBoneReference SpringBone;

	/** Limit the amount that a bone can stretch from its ref-pose length. */
	UPROPERTY(EditAnywhere, Category=Spring)
	bool bLimitDisplacement;

	/** If bLimitDisplacement is true, this indicates how long a bone can stretch beyond its length in the ref-pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring, meta=(EditCondition="bLimitDisplacement"))
	float MaxDisplacement;

	/** Stiffness of spring */
	UPROPERTY(EditAnywhere, Category=Spring)
	float SpringStiffness;

	/** Damping of spring */
	UPROPERTY(EditAnywhere, Category=Spring)
	float SpringDamping;

	/** If spring stretches more than this, reset it. Useful for catching teleports etc */
	UPROPERTY(EditAnywhere, Category=Spring)
	float ErrorResetThresh;

	/** If true, Z position is always correct, no spring applied */
	UPROPERTY()
	bool bNoZSpring_DEPRECATED;

	/** If true take the spring calculation for translation in X */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bTranslateX;

	/** If true take the spring calculation for translation in Y */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bTranslateY;

	/** If true take the spring calculation for translation in Z */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bTranslateZ;

	/** If true take the spring calculation for rotation in X */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bRotateX;

	/** If true take the spring calculation for rotation in Y */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bRotateY;

	/** If true take the spring calculation for rotation in Z */
	UPROPERTY(EditAnywhere, Category=FilterChannels)
	bool bRotateZ;

	/** Internal use - Amount of time we need to simulate. */
	float RemainingTime;

	/** Internal use - Current timestep */
	float FixedTimeStep;

	/** Internal use - Current time dilation */
	float TimeDilation;

	/** Did we have a non-zero ControlStrength last frame. */
	bool bHadValidStrength;

	/** World-space location of the bone. */
	FVector BoneLocation;

	/** World-space velocity of the bone. */
	FVector BoneVelocity;

	/** Velocity of the owning actor */
	FVector OwnerVelocity;

public:
	FAnimNode_SpringBone();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface
};
