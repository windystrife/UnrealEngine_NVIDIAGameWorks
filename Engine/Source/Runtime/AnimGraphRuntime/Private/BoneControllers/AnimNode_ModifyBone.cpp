// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_ModifyBone.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_ModifyBone

FAnimNode_ModifyBone::FAnimNode_ModifyBone()
	: Translation(FVector::ZeroVector)
	, Rotation(FRotator::ZeroRotator)
	, Scale(FVector(1.0f))
	, TranslationMode(BMM_Ignore)
	, RotationMode(BMM_Ignore)
	, ScaleMode(BMM_Ignore)
	, TranslationSpace(BCS_ComponentSpace)
	, RotationSpace(BCS_ComponentSpace)
	, ScaleSpace(BCS_ComponentSpace)
{
}

void FAnimNode_ModifyBone::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" Target: %s)"), *BoneToModify.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ModifyBone::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// the way we apply transform is same as FMatrix or FTransform
	// we apply scale first, and rotation, and translation
	// if you'd like to translate first, you'll need two nodes that first node does translate and second nodes to rotate.
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	FCompactPoseBoneIndex CompactPoseBoneToModify = BoneToModify.GetCompactPoseIndex(BoneContainer);
	FTransform NewBoneTM = Output.Pose.GetComponentSpaceTransform(CompactPoseBoneToModify);
	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	if (ScaleMode != BMM_Ignore)
	{
		// Convert to Bone Space.
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, ScaleSpace);

		if (ScaleMode == BMM_Additive)
		{
			NewBoneTM.SetScale3D(NewBoneTM.GetScale3D() * Scale);
		}
		else
		{
			NewBoneTM.SetScale3D(Scale);
		}

		// Convert back to Component Space.
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, ScaleSpace);
	}

	if (RotationMode != BMM_Ignore)
	{
		// Convert to Bone Space.
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, RotationSpace);

		const FQuat BoneQuat(Rotation);
		if (RotationMode == BMM_Additive)
		{	
			NewBoneTM.SetRotation(BoneQuat * NewBoneTM.GetRotation());
		}
		else
		{
			NewBoneTM.SetRotation(BoneQuat);
		}

		// Convert back to Component Space.
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, RotationSpace);
	}
	
	if (TranslationMode != BMM_Ignore)
	{
		// Convert to Bone Space.
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, TranslationSpace);

		if (TranslationMode == BMM_Additive)
		{
			NewBoneTM.AddToTranslation(Translation);
		}
		else
		{
			NewBoneTM.SetTranslation(Translation);
		}

		// Convert back to Component Space.
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, TranslationSpace);
	}
	
	OutBoneTransforms.Add( FBoneTransform(BoneToModify.GetCompactPoseIndex(BoneContainer), NewBoneTM) );
}

bool FAnimNode_ModifyBone::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	return (BoneToModify.IsValidToEvaluate(RequiredBones));
}

void FAnimNode_ModifyBone::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	BoneToModify.Initialize(RequiredBones);
}
