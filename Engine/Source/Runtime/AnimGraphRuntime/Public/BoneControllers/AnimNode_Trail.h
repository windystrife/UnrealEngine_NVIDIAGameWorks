// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "Curves/CurveFloat.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_Trail.generated.h"

class USkeletalMeshComponent;

// in the future, we might use this for stretch set up as well
// for now this is unserializable, and transient only
struct FPerJointTrailSetup
{
	/** How quickly we 'relax' the bones to their animated positions. */
	float	TrailRelaxationSpeedPerSecond;
};

/**
 * Trail Controller
 */

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_Trail : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Reference to the active bone in the hierarchy to modify. */
	UPROPERTY(EditAnywhere, Category=Trail)
	FBoneReference TrailBone;

	/** Number of bones above the active one in the hierarchy to modify. ChainLength should be at least 2. */
	UPROPERTY(EditAnywhere, Category = Trail, meta = (ClampMin = "2", UIMin = "2"))
	int32	ChainLength;

	/** Axis of the bones to point along trail. */
	UPROPERTY(EditAnywhere, Category=Trail)
	TEnumAsByte<EAxis::Type>	ChainBoneAxis;

	/** Invert the direction specified in ChainBoneAxis. */
	UPROPERTY(EditAnywhere, Category=Trail)
	bool	bInvertChainBoneAxis;

	/** How quickly we 'relax' the bones to their animated positions. Deprecated. Replaced to TrailRelaxationCurve */
	UPROPERTY()
	float	TrailRelaxation_DEPRECATED;

	/** How quickly we 'relax' the bones to their animated positions. Time 0 will map to top root joint, time 1 will map to the bottom joint. */
	UPROPERTY(EditAnywhere, Category=Trail, meta=(CustomizeProperty))
	FRuntimeFloatCurve TrailRelaxationSpeed;

	/** Limit the amount that a bone can stretch from its ref-pose length. */
	UPROPERTY(EditAnywhere, Category=Limit)
	bool	bLimitStretch;

	/** If bLimitStretch is true, this indicates how long a bone can stretch beyond its length in the ref-pose. */
	UPROPERTY(EditAnywhere, Category=Limit)
	float	StretchLimit;

	/** 'Fake' velocity applied to bones. */
	UPROPERTY(EditAnywhere, Category=Velocity)
	FVector	FakeVelocity;

	/** Whether 'fake' velocity should be applied in actor or world space. */
	UPROPERTY(EditAnywhere,  Category=Velocity)
	bool	bActorSpaceFakeVel;

	/** Base Joint to calculate velocity from. If none, it will use Component's World Transform. . */
	UPROPERTY(EditAnywhere, Category=Velocity)
	FBoneReference BaseJoint;

	/** Internal use - we need the timestep to do the relaxation in CalculateNewBoneTransforms. */
	float	ThisTimstep;

	/** Did we have a non-zero ControlStrength last frame. */
	bool	bHadValidStrength;

	/** Component-space locations of the bones from last frame. Each frame these are moved towards their 'animated' locations. */
	TArray<FVector>	TrailBoneLocations;

	/** LocalToWorld used last frame, used for building transform between frames. */
	FTransform		OldBaseTransform;

	/** Per Joint Trail Set up*/
	TArray<FPerJointTrailSetup> PerJointTrailData;

	FAnimNode_Trail();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	void PostLoad();

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	FVector GetAlignVector(EAxis::Type AxisOption, bool bInvert);

	// skeleton index
	TArray<int32> ChainBoneIndices;
};
