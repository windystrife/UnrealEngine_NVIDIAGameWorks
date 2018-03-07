// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_PoseHandler.h"
#include "RBF/RBFSolver.h"
#include "AnimNode_PoseDriver.generated.h"

// Deprecated
UENUM()
enum class EPoseDriverType : uint8
{
	SwingAndTwist,
	SwingOnly,
	Translation
};

/** Transform aspect used to drive interpolation */
UENUM()
enum class EPoseDriverSource : uint8
{
	/** Drive using rotation */
	Rotation,

	/** Driver using translation */
	Translation
};

/** Options for what PoseDriver should be driving */
UENUM()
enum class EPoseDriverOutput : uint8
{
	/** Use target's DrivenName to drive poses from the assigned PoseAsset */
	DrivePoses,

	/** Use the target's DrivenName to drive curves */
	DriveCurves
};

/** Translation and rotation for a particular bone at a particular target */
USTRUCT()
struct FPoseDriverTransform
{
	GENERATED_BODY()

	/** Translation of this target */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FVector TargetTranslation;

	/** Rotation of this target */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FRotator TargetRotation;

	FPoseDriverTransform()
	: TargetTranslation(FVector::ZeroVector)
	, TargetRotation(FRotator::ZeroRotator)
	{}
};

/** Information about each target in the PoseDriver */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FPoseDriverTarget
{
	GENERATED_BODY()
		
	/** Translation of this target */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	TArray<FPoseDriverTransform> BoneTransforms;

	/** Rotation of this target */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FRotator TargetRotation;

	/** Scale applied to this target's function - a larger value will activate this target sooner */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	float TargetScale;

	/** If we should apply a custom curve mapping to how this target activates */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	bool bApplyCustomCurve;

	/** Custom curve mapping to apply if bApplyCustomCurve is true */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FRichCurve CustomCurve;

	/** 
	 *	Name of item to drive - depends on DriveOutput setting.  
	 *	If DriveOutput is DrivePoses, this should be the name of a pose in the assigned PoseAsset
	 *	If DriveOutput is DriveCurves, this is the name of the curve (morph target, material param etc) to drive
	 */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FName DrivenName;

	/** Cached curve UID, if DriveOutput is set to DriveCurves */
	int32 DrivenUID;

	FPoseDriverTarget()
		: TargetScale(1.f)
		, bApplyCustomCurve(false)
		, DrivenName(NAME_None)
		, DrivenUID(INDEX_NONE)
	{}
};

/** RBF based orientation driver */
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_PoseDriver : public FAnimNode_PoseHandler
{
	GENERATED_BODY()

	/** Bones to use for driving parameters based on their transform */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	/** Bone to use for driving parameters based on its orientation */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	TArray<FBoneReference> SourceBones;

	/** If we should filter bones to be driven using the DrivenBonesFilter array */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	bool bOnlyDriveSelectedBones;

	/** If bFilterDrivenBones is specified, only these bones will be modified by this node */
	UPROPERTY(EditAnywhere, Category = PoseDriver, meta = (EditCondition = "bOnlyDriveSelectedBones"))
	TArray<FBoneReference> OnlyDriveBones;

	/** 
	 *	Optional other bone space to use when reading SourceBone transform.
	 *	If not specified, we just use local space of SourceBone (ie relative to parent bone) 
	 */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FBoneReference EvalSpaceBone;

	/** Parameters used by RBF solver */
	UPROPERTY(EditAnywhere, Category = PoseDriver, meta=(ShowOnlyInnerProperties))
	FRBFParams RBFParams;

	/** Which part of the transform is read */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	EPoseDriverSource DriveSource;

	/** Whether we should drive poses or curves */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	EPoseDriverOutput DriveOutput;

	/** Targets used to compare with current pose and drive morphs/poses */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	TArray<FPoseDriverTarget> PoseTargets;

	// Deprecated
	UPROPERTY()
	FBoneReference SourceBone_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<EBoneAxis> TwistAxis_DEPRECATED;
	UPROPERTY()
	EPoseDriverType Type_DEPRECATED;
	UPROPERTY()
	float RadialScaling_DEPRECATED;
	//

	/** Last set of output weights from RBF solve */
	TArray<FRBFOutputWeight> OutputWeights;

	/** Input source bone TM, used for debug drawing */
	TArray<FTransform> SourceBoneTMs;

	/** If bFilterDrivenBones, this array lists bones that we should filter out (ie have a track in the PoseAsset, but are not listed in OnlyDriveBones */
	TArray<FCompactPoseBoneIndex> BonesToFilter;

	/** If true, will recalculate DrivenUID values in PoseTargets array on next eval */
	bool bCachedDrivenIDsAreDirty;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	FAnimNode_PoseDriver();

	/** Util for seeing if BoneName is in the list of driven bones (and bFilterDrivenBones is true) */
	bool IsBoneDriven(FName BoneName) const;

	/** Return array of FRBFTarget structs, derived from PoseTargets array and DriveSource setting */
	void GetRBFTargets(TArray<FRBFTarget>& OutTargets) const;

	/** Update all DrivenUID properties in PoseTargets array */
	void CacheDrivenIDs(USkeleton* Skeleton);
};
