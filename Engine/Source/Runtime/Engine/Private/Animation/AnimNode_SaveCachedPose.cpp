// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_SaveCachedPose.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_SaveCachedPose

FAnimNode_SaveCachedPose::FAnimNode_SaveCachedPose()
{
}

void FAnimNode_SaveCachedPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	// StateMachines cause reinitialization on state changes.
	// we only want to let them through if we're not relevant as to not create a pop.
	if (!InitializationCounter.IsSynchronizedWith(Context.AnimInstanceProxy->GetInitializationCounter())
		|| ((UpdateCounter.Get() != INDEX_NONE) && !UpdateCounter.WasSynchronizedInTheLastFrame(Context.AnimInstanceProxy->GetUpdateCounter())))
	{
		InitializationCounter.SynchronizeWith(Context.AnimInstanceProxy->GetInitializationCounter());

		FAnimNode_Base::Initialize_AnyThread(Context);

		// Initialize the subgraph
		Pose.Initialize(Context);
	}
}

void FAnimNode_SaveCachedPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if (!CachedBonesCounter.IsSynchronizedWith(Context.AnimInstanceProxy->GetCachedBonesCounter()))
	{
		CachedBonesCounter.SynchronizeWith(Context.AnimInstanceProxy->GetCachedBonesCounter());

		// Cache bones in the subgraph
		Pose.CacheBones(Context);
	}
}

void FAnimNode_SaveCachedPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Store this context for the post update
	CachedUpdateContexts.Add(Context);
}

void FAnimNode_SaveCachedPose::Evaluate_AnyThread(FPoseContext& Output)
{
	if (!EvaluationCounter.IsSynchronizedWith(Output.AnimInstanceProxy->GetEvaluationCounter()))
	{
		EvaluationCounter.SynchronizeWith(Output.AnimInstanceProxy->GetEvaluationCounter());

		FPoseContext CachingContext(Output);
		Pose.Evaluate(CachingContext);
		CachedPose.CopyBonesFrom(CachingContext.Pose);
		CachedCurve.CopyFrom(CachingContext.Curve);
	}

	// Return the cached result
	Output.Pose.CopyBonesFrom(CachedPose);
	Output.Curve.CopyFrom(CachedCurve);
}

void FAnimNode_SaveCachedPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("%s:"), *CachePoseName.ToString());

	if (FNodeDebugData* SaveCachePoseDebugDataPtr = DebugData.GetCachePoseDebugData(GlobalWeight))
	{
		SaveCachePoseDebugDataPtr->AddDebugItem(DebugLine);
		Pose.GatherDebugData(*SaveCachePoseDebugDataPtr);
	}
}

void FAnimNode_SaveCachedPose::PostGraphUpdate()
{
	GlobalWeight = 0.f;

	// Update GlobalWeight based on highest weight calling us.
	const int32 NumContexts = CachedUpdateContexts.Num();
	if(NumContexts > 0)
	{
		GlobalWeight = CachedUpdateContexts[0].GetFinalBlendWeight();
		int32 MaxWeightIdx = 0;
		for(int32 CurrIdx = 1; CurrIdx < NumContexts; ++CurrIdx)
		{
			const float BlendWeight = CachedUpdateContexts[CurrIdx].GetFinalBlendWeight();
			if(BlendWeight > GlobalWeight)
			{
				GlobalWeight = BlendWeight;
				MaxWeightIdx = CurrIdx;
			}
		}

		Pose.Update(CachedUpdateContexts[MaxWeightIdx]);
	}

	CachedUpdateContexts.Reset();
}
