// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_CopyPoseFromMesh

FAnimNode_CopyPoseFromMesh::FAnimNode_CopyPoseFromMesh()
	: SourceMeshComponent(nullptr)
	, bUseAttachedParent (false)
{
}

void FAnimNode_CopyPoseFromMesh::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	RefreshMeshComponent(Context.AnimInstanceProxy);
}

void FAnimNode_CopyPoseFromMesh::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_CopyPoseFromMesh::RefreshMeshComponent(FAnimInstanceProxy* AnimInstanceProxy)
{
	auto ResetMeshComponent = [this](USkeletalMeshComponent* InMeshComponent, FAnimInstanceProxy* InAnimInstanceProxy)
	{
		if (CurrentlyUsedSourceMeshComponent.IsValid() && CurrentlyUsedSourceMeshComponent.Get() != InMeshComponent)
		{
			ReinitializeMeshComponent(InMeshComponent, InAnimInstanceProxy);
		}
		else if (!CurrentlyUsedSourceMeshComponent.IsValid() && InMeshComponent)
		{
			ReinitializeMeshComponent(InMeshComponent, InAnimInstanceProxy);
		}
	};

	if (SourceMeshComponent.IsValid())
	{
		ResetMeshComponent(SourceMeshComponent.Get(), AnimInstanceProxy);
	}
	else if (bUseAttachedParent)
	{
		USkeletalMeshComponent* Component = AnimInstanceProxy->GetSkelMeshComponent();
		if (Component)
		{
			USkeletalMeshComponent* ParentComponent = Cast<USkeletalMeshComponent>(Component->GetAttachParent());
			if (ParentComponent)
			{
				ResetMeshComponent(ParentComponent, AnimInstanceProxy);
			}
			else
			{
				CurrentlyUsedSourceMeshComponent.Reset();
			}
		}
		else
		{
			CurrentlyUsedSourceMeshComponent.Reset();
		}
	}
	else
	{
		CurrentlyUsedSourceMeshComponent.Reset();
	}
}

void FAnimNode_CopyPoseFromMesh::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	RefreshMeshComponent(Context.AnimInstanceProxy);
}

void FAnimNode_CopyPoseFromMesh::Evaluate_AnyThread(FPoseContext& Output)
{
	FCompactPose& OutPose = Output.Pose;
	OutPose.ResetToRefPose();

	USkeletalMeshComponent* CurrentMeshComponent = CurrentlyUsedSourceMeshComponent.IsValid()? CurrentlyUsedSourceMeshComponent.Get() : nullptr;

	if (CurrentMeshComponent && CurrentMeshComponent->SkeletalMesh && CurrentMeshComponent->IsRegistered())
	{
		const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();
		for(FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
		{
			const int32& SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(PoseBoneIndex);
			const int32& MeshBoneIndex = RequiredBones.GetSkeletonToPoseBoneIndexArray()[SkeletonBoneIndex];
			const int32* Value = BoneMapToSource.Find(MeshBoneIndex);
			if(Value && *Value!=INDEX_NONE)
			{
				const int32 SourceBoneIndex = *Value;
				const int32 ParentIndex = CurrentMeshComponent->SkeletalMesh->RefSkeleton.GetParentIndex(SourceBoneIndex);
				const FCompactPoseBoneIndex MyParentIndex = RequiredBones.GetParentBoneIndex(PoseBoneIndex);
				// only apply if I also have parent, otherwise, it should apply the space bases
				if (ParentIndex != INDEX_NONE && MyParentIndex != INDEX_NONE)
				{
					const FTransform& ParentTransform = CurrentMeshComponent->GetComponentSpaceTransforms()[ParentIndex];
					const FTransform& ChildTransform = CurrentMeshComponent->GetComponentSpaceTransforms()[SourceBoneIndex];
					OutPose[PoseBoneIndex] = ChildTransform.GetRelativeTransform(ParentTransform);
				}
				else
				{
					OutPose[PoseBoneIndex] = CurrentMeshComponent->GetComponentSpaceTransforms()[SourceBoneIndex];
				}
			}
		}
	}
}

void FAnimNode_CopyPoseFromMesh::GatherDebugData(FNodeDebugData& DebugData)
{
}

void FAnimNode_CopyPoseFromMesh::ReinitializeMeshComponent(USkeletalMeshComponent* NewSourceMeshComponent, FAnimInstanceProxy* AnimInstanceProxy)
{
	CurrentlyUsedSourceMeshComponent = NewSourceMeshComponent;
	BoneMapToSource.Reset();
	if (NewSourceMeshComponent && NewSourceMeshComponent->SkeletalMesh && !NewSourceMeshComponent->IsPendingKill())
	{
		USkeletalMeshComponent* TargetMeshComponent = AnimInstanceProxy->GetSkelMeshComponent();
		if (TargetMeshComponent)
		{
			USkeletalMesh* SourceSkelMesh = NewSourceMeshComponent->SkeletalMesh;
			USkeletalMesh* TargetSkelMesh = TargetMeshComponent->SkeletalMesh;

			if (SourceSkelMesh == TargetSkelMesh)
			{
				for(int32 ComponentSpaceBoneId = 0; ComponentSpaceBoneId < SourceSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
				{
					BoneMapToSource.Add(ComponentSpaceBoneId, ComponentSpaceBoneId);
				}
			}
			else
			{
				for (int32 ComponentSpaceBoneId=0; ComponentSpaceBoneId<TargetSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
				{
					FName BoneName = TargetSkelMesh->RefSkeleton.GetBoneName(ComponentSpaceBoneId);
					BoneMapToSource.Add(ComponentSpaceBoneId, SourceSkelMesh->RefSkeleton.FindBoneIndex(BoneName));
				}
			}
		}
	}
}
