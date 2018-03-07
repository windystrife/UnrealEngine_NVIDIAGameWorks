// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseByName.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

void FAnimNode_PoseByName::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize_AnyThread(Context);
}

void FAnimNode_PoseByName::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_PoseHandler::CacheBones_AnyThread(Context);
}

void FAnimNode_PoseByName::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);

	// for now we do this in every frame because we don't detect name change
	// this has to happen whenever name changes
	if (PoseName != NAME_None)
	{
		USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();
		PoseUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, PoseName);
	}
	else
	{
		PoseUID = SmartName::MaxUID;
	}
}

void FAnimNode_PoseByName::Evaluate_AnyThread(FPoseContext& Output)
{
	if ((CurrentPoseAsset.IsValid()) && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton())))
	{
		// Zero out all curve value
		FMemory::Memzero(PoseExtractContext.PoseCurves.GetData(), PoseExtractContext.PoseCurves.GetAllocatedSize());

		if (PoseUID != SmartName::MaxUID)
		{
			int32 Index = PoseUIDList.Find(PoseUID);
			if (Index!=INDEX_NONE)
			{
				PoseExtractContext.PoseCurves[Index] = PoseWeight;
			}
		}

		// only give pose curve, we don't set any more curve here
		CurrentPoseAsset->GetAnimationPose(Output.Pose, Output.Curve, PoseExtractContext);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_PoseByName::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' Pose: %s)"), CurrentPoseAsset.IsValid()? *CurrentPoseAsset.Get()->GetName() : TEXT("None"), *PoseName.ToString());
	DebugData.AddDebugItem(DebugLine, true);
}
