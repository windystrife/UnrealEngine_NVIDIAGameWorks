// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseSnapshot.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_PoseSnapshot

FAnimNode_PoseSnapshot::FAnimNode_PoseSnapshot()
{
	MappedSourceMeshName = NAME_None;
	MappedTargetMeshName = NAME_None;
	TargetBoneNameMesh = NAME_None;
}

void FAnimNode_PoseSnapshot::PreUpdate(const UAnimInstance* InAnimInstance)
{
	// cache the currently used skeletal mesh's bone names
	USkeletalMesh* CurrentSkeletalMesh = nullptr;
	if (InAnimInstance->GetSkelMeshComponent() && InAnimInstance->GetSkelMeshComponent()->IsRegistered())
	{
		CurrentSkeletalMesh = InAnimInstance->GetSkelMeshComponent()->SkeletalMesh;
	}

	if (CurrentSkeletalMesh)
	{
		if (TargetBoneNameMesh != CurrentSkeletalMesh->GetFName())
		{
			// cache bone names for the target mesh
			TargetBoneNames.Reset();
			TargetBoneNames.AddDefaulted(CurrentSkeletalMesh->RefSkeleton.GetNum());

			for (int32 BoneIndex = 0; BoneIndex < CurrentSkeletalMesh->RefSkeleton.GetNum(); ++BoneIndex)
			{
				TargetBoneNames[BoneIndex] = CurrentSkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
			}
		}

		TargetBoneNameMesh = CurrentSkeletalMesh->GetFName();
	}
	else
	{
		TargetBoneNameMesh = NAME_Name;
	}
}

void FAnimNode_PoseSnapshot::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Evaluate any BP logic plugged into this node
	EvaluateGraphExposedInputs.Execute(Context);
}

void FAnimNode_PoseSnapshot::Evaluate_AnyThread(FPoseContext& Output)
{
	FCompactPose& OutPose = Output.Pose;
	OutPose.ResetToRefPose();

	switch (Mode)
	{
		case ESnapshotSourceMode::NamedSnapshot:
		{
			if (const FPoseSnapshot* PoseSnapshotData = Output.AnimInstanceProxy->GetPoseSnapshot(SnapshotName))
			{
				ApplyPose(*PoseSnapshotData, OutPose);
			}
		}
		break;
		case ESnapshotSourceMode::SnapshotPin:
		{
			ApplyPose(Snapshot, OutPose);
		}
		break;
	}
}

void FAnimNode_PoseSnapshot::ApplyPose(const FPoseSnapshot& PoseSnapshot, FCompactPose& OutPose)
{
	const TArray<FTransform>& LocalTMs = PoseSnapshot.LocalTransforms;
	const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();

	if (TargetBoneNameMesh == PoseSnapshot.SkeletalMeshName)
	{
		for (FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
		{
			const FMeshPoseBoneIndex MeshBoneIndex = RequiredBones.MakeMeshPoseIndex(PoseBoneIndex);
			const int32 Index = MeshBoneIndex.GetInt();

			if (LocalTMs.IsValidIndex(Index))
			{
				OutPose[PoseBoneIndex] = LocalTMs[Index];
			}
		}
	}
	else
	{
		// per-bone matching required
		CacheBoneMapping(PoseSnapshot.SkeletalMeshName, TargetBoneNameMesh, PoseSnapshot.BoneNames, TargetBoneNames);

		for (FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
		{
			const FMeshPoseBoneIndex MeshBoneIndex = RequiredBones.MakeMeshPoseIndex(PoseBoneIndex);
			const int32 Index = MeshBoneIndex.GetInt();

			if (SourceBoneMapping.IsValidIndex(Index))
			{
				const int32 SourceBoneIndex = SourceBoneMapping[Index];
				if(LocalTMs.IsValidIndex(SourceBoneIndex))
				{
					OutPose[PoseBoneIndex] = LocalTMs[SourceBoneIndex];
				}
			}
		}
	}
}

void FAnimNode_PoseSnapshot::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this) + " Snapshot Name:" + SnapshotName.ToString();
	DebugData.AddDebugItem(DebugLine, true);
}

void FAnimNode_PoseSnapshot::CacheBoneMapping(FName InSourceMeshName, FName InTargetMeshName, const TArray<FName>& InSourceBoneNames, const TArray<FName>& InTargetBoneNames)
{
	if (InSourceMeshName != MappedSourceMeshName || InTargetMeshName != MappedTargetMeshName)
	{
		SourceBoneMapping.Reset();

		for (int32 BoneIndex = 0; BoneIndex < InTargetBoneNames.Num(); ++BoneIndex)
		{
			SourceBoneMapping.Add(InSourceBoneNames.IndexOfByKey(InTargetBoneNames[BoneIndex]));
		}

		MappedSourceMeshName = InSourceMeshName;
		MappedTargetMeshName = InTargetMeshName;
	}
}
