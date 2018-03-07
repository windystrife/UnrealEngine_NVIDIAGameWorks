// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_MakeDynamicAdditive.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_MakeDynamicAdditive

FAnimNode_MakeDynamicAdditive::FAnimNode_MakeDynamicAdditive()
{
}

void FAnimNode_MakeDynamicAdditive::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	Base.Initialize(Context);
	Additive.Initialize(Context);
}

void FAnimNode_MakeDynamicAdditive::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Base.CacheBones(Context);
	Additive.CacheBones(Context);
}

void FAnimNode_MakeDynamicAdditive::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Base.Update(Context.FractionalWeight(1.f));
	Additive.Update(Context.FractionalWeight(1.f));
}

void FAnimNode_MakeDynamicAdditive::Evaluate_AnyThread(FPoseContext& Output)
{
	FPoseContext BaseEvalContext(Output);

	Base.Evaluate(BaseEvalContext);
	Additive.Evaluate(Output);

	if (bMeshSpaceAdditive)
	{
		FAnimationRuntime::ConvertPoseToMeshRotation(Output.Pose);
		FAnimationRuntime::ConvertPoseToMeshRotation(BaseEvalContext.Pose);
	}

	FAnimationRuntime::ConvertPoseToAdditive(Output.Pose, BaseEvalContext.Pose);
	Output.Curve.ConvertToAdditive(BaseEvalContext.Curve);
}

void FAnimNode_MakeDynamicAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Mesh Space Additive: %s)"), bMeshSpaceAdditive ? TEXT("true") : TEXT("false"));

	DebugData.AddDebugItem(DebugLine);
	Base.GatherDebugData(DebugData.BranchFlow(1.f));
	Additive.GatherDebugData(DebugData.BranchFlow(1.f));
}
