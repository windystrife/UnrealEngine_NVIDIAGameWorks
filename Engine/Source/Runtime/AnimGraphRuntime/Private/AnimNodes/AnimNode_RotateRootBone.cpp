// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RotateRootBone.h"

/////////////////////////////////////////////////////
// FAnimNode_RotateRootBone

void FAnimNode_RotateRootBone::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	BasePose.Initialize(Context);
}

void FAnimNode_RotateRootBone::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	BasePose.CacheBones(Context);
}

void FAnimNode_RotateRootBone::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	BasePose.Update(Context);
}

void FAnimNode_RotateRootBone::Evaluate_AnyThread(FPoseContext& Output)
{
	// Evaluate the input
	BasePose.Evaluate(Output);

	checkSlow(!FMath::IsNaN(Yaw) && FMath::IsFinite(Yaw));
	checkSlow(!FMath::IsNaN(Pitch) && FMath::IsFinite(Pitch));

	if (!FMath::IsNearlyZero(Pitch, KINDA_SMALL_NUMBER) || !FMath::IsNearlyZero(Yaw, KINDA_SMALL_NUMBER))
	{
		// Build our desired rotation
		const FRotator DeltaRotation(Pitch, Yaw, 0.f);
		const FQuat DeltaQuat(DeltaRotation);
		const FQuat MeshToComponentQuat(MeshToComponent);

		// Convert our rotation from Component Space to Mesh Space.
		const FQuat MeshSpaceDeltaQuat = MeshToComponentQuat.Inverse() * DeltaQuat * MeshToComponentQuat;

		// Apply rotation to root bone.
		FCompactPoseBoneIndex RootBoneIndex(0);
		Output.Pose[RootBoneIndex].SetRotation(Output.Pose[RootBoneIndex].GetRotation() * MeshSpaceDeltaQuat);
		Output.Pose[RootBoneIndex].NormalizeRotation();
	}
}


void FAnimNode_RotateRootBone::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("(Pitch: %.2f Yaw: %.2f)"), Pitch, Yaw);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData);
}

FAnimNode_RotateRootBone::FAnimNode_RotateRootBone()
	: Pitch(0.0f)
	, Yaw(0.0f)
	, MeshToComponent(FRotator::ZeroRotator)
{
}
