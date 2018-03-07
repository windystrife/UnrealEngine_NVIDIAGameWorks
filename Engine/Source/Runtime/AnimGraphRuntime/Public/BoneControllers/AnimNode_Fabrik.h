// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_Fabrik.generated.h"

class FPrimitiveDrawInterface;
class USkeletalMeshComponent;

/**
*	Controller which implements the FABRIK IK approximation algorithm -  see http://www.academia.edu/9165835/FABRIK_A_fast_iterative_solver_for_the_Inverse_Kinematics_problem for details
*/

/** Transient structure for FABRIK node evaluation */
struct FABRIKChainLink
{
public:
	/** Position of bone in component space. */
	FVector Position;

	/** Distance to its parent link. */
	float Length;

	/** Bone Index in SkeletalMesh */
	FCompactPoseBoneIndex BoneIndex;

	/** Transform Index that this control will output */
	int32 TransformIndex;

	/** Child bones which are overlapping this bone. 
	 * They have a zero length distance, so they will inherit this bone's transformation. */
	TArray<int32> ChildZeroLengthTransformIndices;

	FABRIKChainLink()
		: Position(FVector::ZeroVector)
		, Length(0.f)
		, BoneIndex(INDEX_NONE)
		, TransformIndex(INDEX_NONE)
	{
	}

	FABRIKChainLink(const FVector& InPosition, const float& InLength, const FCompactPoseBoneIndex& InBoneIndex, const int32& InTransformIndex)
		: Position(InPosition)
		, Length(InLength)
		, BoneIndex(InBoneIndex)
		, TransformIndex(InTransformIndex)
	{
	}
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_Fabrik : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Coordinates for target location of tip bone - if EffectorLocationSpace is bone, this is the offset from Target Bone to use as target location*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EndEffector, meta = (PinShownByDefault))
	FTransform EffectorTransform;

	/** Reference frame of Effector Transform. */
	UPROPERTY(EditAnywhere, Category = EndEffector)
	TEnumAsByte<enum EBoneControlSpace> EffectorTransformSpace;

	/** If EffectorTransformSpace is a bone, this is the bone to use. **/
	UPROPERTY()
	FBoneReference EffectorTransformBone_DEPRECATED;

	/** If EffectorTransformSpace is a bone, this is the bone to use. **/
	UPROPERTY(EditAnywhere, Category = EndEffector)
	FBoneSocketTarget EffectorTarget;

	UPROPERTY(EditAnywhere, Category = EndEffector)
	TEnumAsByte<enum EBoneRotationSource> EffectorRotationSource;

	/** Name of tip bone */
	UPROPERTY(EditAnywhere, Category = Solver)
	FBoneReference TipBone;

	/** Name of the root bone*/
	UPROPERTY(EditAnywhere, Category = Solver)
	FBoneReference RootBone;

	/** Tolerance for final tip location delta from EffectorLocation*/
	UPROPERTY(EditAnywhere, Category = Solver)
	float Precision;

	/** Maximum number of iterations allowed, to control performance. */
	UPROPERTY(EditAnywhere, Category = Solver)
	int32 MaxIterations;

	/** Toggle drawing of axes to debug joint rotation*/
	UPROPERTY(EditAnywhere, Category = Solver)
	bool bEnableDebugDraw;

public:
	FAnimNode_Fabrik();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	virtual void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const;

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	// Convenience function to get current (pre-translation iteration) component space location of bone by bone index
	FVector GetCurrentLocation(FCSPose<FCompactPose>& MeshBases, const FCompactPoseBoneIndex& BoneIndex);
	static FTransform GetTargetTransform(const FTransform& InComponentTransform, FCSPose<FCompactPose>& MeshBases, FBoneSocketTarget& InTarget, EBoneControlSpace Space, const FTransform& InOffset);
#if WITH_EDITOR
	// Cached CS location when in editor for debug drawing
	FTransform CachedEffectorCSTransform;
#endif
};
