// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseBlendNode.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

FAnimNode_PoseBlendNode::FAnimNode_PoseBlendNode()
{
	BlendOption = EAlphaBlendOption::Linear;
}

void FAnimNode_PoseBlendNode::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize_AnyThread(Context);

	SourcePose.Initialize(Context);
}

void FAnimNode_PoseBlendNode::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_PoseHandler::CacheBones_AnyThread(Context);
	SourcePose.CacheBones(Context);
}

void FAnimNode_PoseBlendNode::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);
	SourcePose.Update(Context);
}

void FAnimNode_PoseBlendNode::Evaluate_AnyThread(FPoseContext& Output)
{
	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	bool bValidPose = false;

	if (CurrentPoseAsset.IsValid() && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton())))
	{
		FPoseContext CurrentPose(Output);
		check(PoseExtractContext.PoseCurves.Num() == PoseUIDList.Num());
		// only give pose curve, we don't set any more curve here
		for (int32 PoseIdx = 0; PoseIdx < PoseUIDList.Num(); ++PoseIdx)
		{
			// Get value of input curve
			float InputValue = SourceData.Curve.Get(PoseUIDList[PoseIdx]);
			// Remap using chosen BlendOption
			float RemappedValue = FAlphaBlend::AlphaToBlendOption(InputValue, BlendOption, CustomCurve);

			PoseExtractContext.PoseCurves[PoseIdx] = RemappedValue;
		}

		if (CurrentPoseAsset.Get()->GetAnimationPose(CurrentPose.Pose, CurrentPose.Curve, PoseExtractContext))
		{
			// once we get it, we have to blend by weight
			if (CurrentPoseAsset->IsValidAdditive())
			{
				Output = SourceData;
				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, CurrentPose.Pose, Output.Curve, CurrentPose.Curve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
			}
			else
			{
				FAnimationRuntime::BlendTwoPosesTogetherPerBone(SourceData.Pose, CurrentPose.Pose, SourceData.Curve, CurrentPose.Curve, BoneBlendWeights, Output.Pose, Output.Curve);
			}

			bValidPose = true;
		}
	}

	// If we didn't create a valid pose, just copy SourcePose to output (pass through)
	if(!bValidPose)
	{
		Output = SourceData;
	}
}

void FAnimNode_PoseBlendNode::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_PoseHandler::GatherDebugData(DebugData);
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.f));
}

