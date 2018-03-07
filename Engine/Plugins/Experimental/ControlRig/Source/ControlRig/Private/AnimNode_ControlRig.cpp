// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig.h"
#include "HierarchicalRig.h"
#include "ControlRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimInstanceProxy.h"
#include "GameFramework/Actor.h"
#include "AnimationRuntime.h"

FAnimNode_ControlRig::FAnimNode_ControlRig()
	: CachedControlRig(nullptr)
	, bAdditive(false)
{
}

void FAnimNode_ControlRig::SetControlRig(UControlRig* InControlRig)
{
	CachedControlRig = InControlRig;
}

UControlRig* FAnimNode_ControlRig::GetControlRig() const
{
	return CachedControlRig.Get();
}

void FAnimNode_ControlRig::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	if (UControlRig* ControlRig = CachedControlRig.Get())
	{
		if (UHierarchicalRig* HierControlRig = Cast<UHierarchicalRig>(ControlRig))
		{
			USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(HierControlRig->GetBoundObject());
			if (Component && Component->SkeletalMesh)
			{
				UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(ControlRig->GetClass());
				if (BlueprintClass)
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
					HierControlRig->NodeMappingContainer = Component->SkeletalMesh->GetNodeMappingContainer(Blueprint);
				}
			}
		}

		// Initialize AFTER setting node mapping, so that we can cache correct mapped transform values
		// i.e. IK limb lengths. 
		ControlRig->Initialize();
	}
}

void FAnimNode_ControlRig::GatherDebugData(FNodeDebugData& DebugData)
{

}

void FAnimNode_ControlRig::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	if (UControlRig* ControlRig = CachedControlRig.Get())
	{
		ControlRig->PreEvaluate();
		ControlRig->Evaluate();
		ControlRig->PostEvaluate();
	}
}

void FAnimNode_ControlRig::Evaluate_AnyThread(FPoseContext& Output)
{
	if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(CachedControlRig.Get()))
	{
		USkeletalMeshComponent* SkelComp = Output.AnimInstanceProxy->GetSkelMeshComponent();
		const FBoneContainer& RequiredBones = Output.Pose.GetBoneContainer();

		// we initialize to ref pose for MeshPose
		Output.ResetToRefPose();
		// get component pose from control rig
		FCSPose<FCompactPose> MeshPoses;
		MeshPoses.InitPose(Output.Pose);

		const int32 NumNodes = NodeNames.Num();
		for (int32 Index = 0; Index < NumNodes; ++Index)
		{
			if (NodeNames[Index] != NAME_None)
			{
				FCompactPoseBoneIndex CompactPoseIndex(Index);
				FTransform ComponentTransform = HierarchicalRig->GetMappedGlobalTransform(NodeNames[Index]);
				MeshPoses.SetComponentSpaceTransform(CompactPoseIndex, ComponentTransform);
			}
		}

		// now convert to what you'd like
		if (bAdditive)
		{
			// for additive, initialize with identity
			Output.ResetToAdditiveIdentity();

			for (int32 Index = 0; Index < NumNodes; ++Index)
			{
				if (NodeNames[Index] != NAME_None)
				{
					FCompactPoseBoneIndex CompactPoseIndex(Index);
					FTransform LocalTransform = MeshPoses.GetLocalSpaceTransform(CompactPoseIndex);

					// use refpose for now
					const FTransform& RefTransform = SkelComp->SkeletalMesh->RefSkeleton.GetRawRefBonePose()[Index];
					FAnimationRuntime::ConvertTransformToAdditive(LocalTransform, RefTransform);
					Output.Pose[CompactPoseIndex] = LocalTransform;
				}
			}
		}
		else
		{
			// initialize with refpose
			Output.ResetToRefPose();
			for (int32 Index = 0; Index < NumNodes; ++Index)
			{
				if (NodeNames[Index] != NAME_None)
				{
					FCompactPoseBoneIndex CompactPoseIndex(Index);
					Output.Pose[CompactPoseIndex] = MeshPoses.GetLocalSpaceTransform(CompactPoseIndex);
				}
			}
		}
	}
	else
	{
		// apply refpose
		Output.ResetToRefPose();
	}
}

void FAnimNode_ControlRig::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(CachedControlRig.Get()))
	{
		// @todo: thread-safe? probably not in editor, but it may not be a big issue in editor
		const UNodeMappingContainer* Mapper = HierarchicalRig->NodeMappingContainer;

		// fill up node names
		FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
		const TArray<FBoneIndexType>& RequiredBonesArray = RequiredBones.GetBoneIndicesArray();
		const int32 NumBones = RequiredBonesArray.Num();
		NodeNames.Reset(NumBones);
		NodeNames.AddDefaulted(NumBones);

		const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();
		// now fill up node name
		if (Mapper)
		{
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				// get bone name, and find reverse mapping
				FName TargetNodeName = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
				FName SourceName = Mapper->GetSourceName(TargetNodeName);
				NodeNames[Index] = SourceName;
			}
		}
		else
		{
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				NodeNames[Index] = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
			}
		}
	}

	UE_LOG(LogAnimation, Log, TEXT("%s : %d"), *GetNameSafe(CachedControlRig.Get()), NodeNames.Num())
}
