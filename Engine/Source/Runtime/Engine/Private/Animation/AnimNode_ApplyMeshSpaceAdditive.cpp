// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_ApplyMeshSpaceAdditive.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_ApplyMeshSpaceAdditive

void FAnimNode_ApplyMeshSpaceAdditive::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	Base.Initialize(Context);
	Additive.Initialize(Context);
}

void FAnimNode_ApplyMeshSpaceAdditive::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Base.CacheBones(Context);
	Additive.CacheBones(Context);
}

void FAnimNode_ApplyMeshSpaceAdditive::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Base.Update(Context);

	ActualAlpha = 0.f;
	if (IsLODEnabled(Context.AnimInstanceProxy, LODThreshold))
	{
		// @note: If you derive this class, and if you have input that you rely on for base
		// this is not going to work	
		EvaluateGraphExposedInputs.Execute(Context);
		ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (FAnimWeight::IsRelevant(ActualAlpha))
		{
			Additive.Update(Context.FractionalWeight(ActualAlpha));
		}
	}
}

void FAnimNode_ApplyMeshSpaceAdditive::Evaluate_AnyThread(FPoseContext& Output)
{
	//@TODO: Could evaluate Base into Output and save a copy
	if (FAnimWeight::IsRelevant(ActualAlpha))
	{
		FPoseContext AdditiveEvalContext(Output);

		Base.Evaluate(Output);
		Additive.Evaluate(AdditiveEvalContext, true);

		FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditiveEvalContext.Pose, Output.Curve, AdditiveEvalContext.Curve, ActualAlpha, AAT_RotationOffsetMeshSpace);
		Output.Pose.NormalizeRotations();
	}
	else
	{
		Base.Evaluate(Output);
	}
}

FAnimNode_ApplyMeshSpaceAdditive::FAnimNode_ApplyMeshSpaceAdditive()
	: Alpha(1.0f)
	, LODThreshold(INDEX_NONE)
	, ActualAlpha(0.f)
{
}

void FAnimNode_ApplyMeshSpaceAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), ActualAlpha*100.f);

	DebugData.AddDebugItem(DebugLine);
	Base.GatherDebugData(DebugData.BranchFlow(1.f));
	Additive.GatherDebugData(DebugData.BranchFlow(ActualAlpha));
}
