// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_HandIKRetargeting.h"

/////////////////////////////////////////////////////
// FAnimNode_HandIKRetargeting

FAnimNode_HandIKRetargeting::FAnimNode_HandIKRetargeting()
: HandFKWeight(0.5f)
{
}

void FAnimNode_HandIKRetargeting::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" HandFKWeight: %f)"), HandFKWeight);
	for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
	{
		DebugLine += FString::Printf(TEXT(", %s)"), *IKBonesToMove[BoneIndex].BoneName.ToString());
	}
	DebugLine += FString::Printf(TEXT(")"));
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_HandIKRetargeting::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	checkSlow(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	// Get component space transforms for all of our IK and FK bones.
	const FTransform& RightHandFKTM = Output.Pose.GetComponentSpaceTransform(RightHandFK.GetCompactPoseIndex(BoneContainer));
	const FTransform& LeftHandFKTM = Output.Pose.GetComponentSpaceTransform(LeftHandFK.GetCompactPoseIndex(BoneContainer));
	const FTransform& RightHandIKTM = Output.Pose.GetComponentSpaceTransform(RightHandIK.GetCompactPoseIndex(BoneContainer));
	const FTransform& LeftHandIKTM = Output.Pose.GetComponentSpaceTransform(LeftHandIK.GetCompactPoseIndex(BoneContainer));
	
	// Compute weight FK and IK hand location. And translation from IK to FK.
	FVector const FKLocation = FMath::Lerp<FVector>(LeftHandFKTM.GetTranslation(), RightHandFKTM.GetTranslation(), HandFKWeight);
	FVector const IKLocation = FMath::Lerp<FVector>(LeftHandIKTM.GetTranslation(), RightHandIKTM.GetTranslation(), HandFKWeight);
	FVector const IK_To_FK_Translation = FKLocation - IKLocation;

	// If we're not translating, don't send any bones to update.
	if (!IK_To_FK_Translation.IsNearlyZero())
	{
		// Move desired bones
		for (const FBoneReference& BoneReference : IKBonesToMove)
		{
			if (BoneReference.IsValidToEvaluate(BoneContainer))
			{
				FCompactPoseBoneIndex BoneIndex = BoneReference.GetCompactPoseIndex(BoneContainer);
				FTransform BoneTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);
				BoneTransform.AddToTranslation(IK_To_FK_Translation);

				OutBoneTransforms.Add(FBoneTransform(BoneIndex, BoneTransform));
			}
		}
	}

	if (OutBoneTransforms.Num() > 0)
	{
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());
	}
}

bool FAnimNode_HandIKRetargeting::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (RightHandFK.IsValidToEvaluate(RequiredBones)
		&& LeftHandFK.IsValidToEvaluate(RequiredBones)
		&& RightHandIK.IsValidToEvaluate(RequiredBones)
		&& LeftHandIK.IsValidToEvaluate(RequiredBones))
	{
		// we need at least one bone to move valid.
		for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
		{
			if (IKBonesToMove[BoneIndex].IsValidToEvaluate(RequiredBones))
			{
				return true;
			}
		}
	}

	return false;
}

void FAnimNode_HandIKRetargeting::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	RightHandFK.Initialize(RequiredBones);
	LeftHandFK.Initialize(RequiredBones);
	RightHandIK.Initialize(RequiredBones);
	LeftHandIK.Initialize(RequiredBones);

	for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
	{
		IKBonesToMove[BoneIndex].Initialize(RequiredBones);
	}
}
