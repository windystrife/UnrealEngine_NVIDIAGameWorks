// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_SubInput.h"

void FAnimNode_SubInput::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
}

void FAnimNode_SubInput::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_SubInput::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
}

void FAnimNode_SubInput::Evaluate_AnyThread(FPoseContext& Output)
{
	if(InputPose.IsValid() && InputCurve.IsValid())
	{
		Output.Pose.CopyBonesFrom(InputPose);
		Output.Curve.CopyFrom(InputCurve);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SubInput::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
}
