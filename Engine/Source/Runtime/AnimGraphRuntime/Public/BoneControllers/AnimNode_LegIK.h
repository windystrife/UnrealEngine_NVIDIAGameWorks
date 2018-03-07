// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_LegIK.generated.h"

class USkeletalMeshComponent;

/** Per foot definitions */
USTRUCT()
struct FAnimLegIKDefinition
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference IKFootBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference FKFootBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 NumBonesInLimb;

	/** Forward Axis for Foot bone. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TEnumAsByte<EAxis::Type> FootBoneForwardAxis;

	/** If enabled, we prevent the leg from bending backwards and enforce a min compression angle */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bEnableRotationLimit;

	/** Only used if bEnableRotationLimit is enabled. Prevents the leg from folding onto itself,
	* and forces at least this angle between Parent and Child bone. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MinRotationAngle;

	/** Enable Knee Twist correction, by comparing Foot FK with Foot IK orientation. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bEnableKneeTwistCorrection;

	FAnimLegIKDefinition()
		: NumBonesInLimb(2)
		, FootBoneForwardAxis(EAxis::Y)
		, bEnableRotationLimit(false)
		, MinRotationAngle(15.f)
		, bEnableKneeTwistCorrection(true)
	{}
};

/** Runtime foot data after validation, we guarantee these bones to exist */
USTRUCT()
struct FAnimLegIKData
{
	GENERATED_USTRUCT_BODY()

public:
	FCompactPoseBoneIndex IKFootBoneIndex;
	FTransform IKFootTransform;

	FAnimLegIKDefinition* LegDefPtr;

	int32 NumBones;
	TArray<FCompactPoseBoneIndex> FKLegBoneIndices;
	TArray<FTransform> FKLegBoneTransforms;

public:
	void InitializeTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases);

	FAnimLegIKData()
		: IKFootBoneIndex(INDEX_NONE)
		, LegDefPtr(nullptr)
		, NumBones(INDEX_NONE)
	{}
};

USTRUCT()
struct FIKChainLink
{
	GENERATED_USTRUCT_BODY()

		FVector Location;
	float Length;
	FVector LinkAxisZ;

	FIKChainLink()
		: Location(FVector::ZeroVector)
		, Length(0.f)
		, LinkAxisZ(FVector::ZeroVector)
	{}

	FIKChainLink(FVector InLocation, float InLength)
		: Location(InLocation)
		, Length(InLength)
		, LinkAxisZ(FVector::ZeroVector)
	{}
};

USTRUCT()
struct FIKChain
{
	GENERATED_USTRUCT_BODY()

public:
	TArray<FIKChainLink> Links;
	float MinRotationAngleRadians;

private:
	bool bInitialized;
	float MaximumReach;
	int32 NumLinks;
	bool bEnableRotationLimit;
	USkeletalMeshComponent* SkelMeshComp;

public:
	FIKChain()
		: bInitialized(false)
		, MaximumReach(0.f)
	{}

	void InitializeFromLegData(const FAnimLegIKData& InLegData, USkeletalMeshComponent* InSkelMeshComp);
	void ReachTarget(const FVector& InTargetLocation, float InReachPrecision, int32 InMaxIterations);

	float GetMaximumReach() const
	{
		return MaximumReach;
	}

private:
	void OrientAllLinksToDirection(const FVector& InDirection);
	void SolveFABRIK(const FVector& InTargetLocation, float InReachPrecision, int32 InMaxIterations);

	static void FABRIK_ForwardReach(const FVector& InTargetLocation, FIKChain& IKChain);
	static void FABRIK_BackwardReach(const FVector& InRootTargetLocation, FIKChain& IKChain);
	static void FABRIK_ApplyLinkConstraints_Forward(FIKChain& IKChain, int32 LinkIndex);
	static void FABRIK_ApplyLinkConstraints_Backward(FIKChain& IKChain, int32 LinkIndex);

	static void DrawDebugIKChain(const FIKChain& IKChain, const FColor& InColor);
};

USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_LegIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	FAnimNode_LegIK();

	/** Tolerance for reaching IK Target, in unreal units. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ReachPrecision;

	/** Max Number of Iterations. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 MaxIterations;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FAnimLegIKDefinition> LegsDefinition;

	UPROPERTY(Transient)
	TArray<FAnimLegIKData> LegsData;

public:
	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	void OrientLegTowardsIK(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp);
	void DoLegReachIK(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp);
	void AdjustKneeTwist(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp);

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface
};
