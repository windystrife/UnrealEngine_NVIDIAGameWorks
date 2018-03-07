// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_MultiWayBlend.h"
#include "AnimationRuntime.h"


struct FMultiBlendData : public TThreadSingleton<FMultiBlendData>
{
	TArray<FCompactPose, TInlineAllocator<8>> SourcePoses;
	TArray<float, TInlineAllocator<8>> SourceWeights;
	TArray<FBlendedCurve, TInlineAllocator<8>> SourceCurves;
};

/////////////////////////////////////////////////////
// FAnimNode_MultiWayBlend

void FAnimNode_MultiWayBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	// this should be consistent all the time by editor node
	if (!ensure(Poses.Num() == DesiredAlphas.Num()))
	{
		DesiredAlphas.Init(0.f, Poses.Num());
	}

	UpdateCachedAlphas();

	for (FPoseLink& Pose : Poses)
	{
		Pose.Initialize(Context);
	}
}

void FAnimNode_MultiWayBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	for (FPoseLink& Pose : Poses)
	{
		Pose.CacheBones(Context);
	}
}

void FAnimNode_MultiWayBlend::UpdateCachedAlphas()
{
	float TotalAlpha = GetTotalAlpha();

	if (DesiredAlphas.Num() > 0)
	{
		if (DesiredAlphas.Num() != CachedAlphas.Num())
		{
			CachedAlphas.Init(0.f, DesiredAlphas.Num());
		}
		else
		{
			FMemory::Memzero(CachedAlphas.GetData(), CachedAlphas.GetAllocatedSize());
		}
	}
	else
	{
		CachedAlphas.Reset();
	}

	const float ActualTotalAlpha = AlphaScaleBias.ApplyTo(TotalAlpha);
	if (ActualTotalAlpha > ZERO_ANIMWEIGHT_THRESH)
	{
		if (bNormalizeAlpha)
		{
			// normalize by total alpha
			for (int32 AlphaIndex = 0; AlphaIndex < DesiredAlphas.Num(); ++AlphaIndex)
			{
				// total alpha shouldn't be zero
				CachedAlphas[AlphaIndex] = AlphaScaleBias.ApplyTo(DesiredAlphas[AlphaIndex] / TotalAlpha);
			}
		}
		else
		{
			for (int32 AlphaIndex = 0; AlphaIndex < DesiredAlphas.Num(); ++AlphaIndex)
			{
				// total alpha shouldn't be zero
				CachedAlphas[AlphaIndex] = AlphaScaleBias.ApplyTo(DesiredAlphas[AlphaIndex]);
			}
		}
	}
}

void FAnimNode_MultiWayBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimationNode_MultiWayBlend_Update);
	EvaluateGraphExposedInputs.Execute(Context);
	UpdateCachedAlphas();

	for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		// total alpha shouldn't be zero
		float CurrentAlpha = CachedAlphas[PoseIndex];
		if (CurrentAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			Poses[PoseIndex].Update(Context.FractionalWeight(CurrentAlpha));
		}
	}
}

void FAnimNode_MultiWayBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	FMultiBlendData& BlendData = FMultiBlendData::Get();
	TArray<FCompactPose, TInlineAllocator<8>>& SourcePoses = BlendData.SourcePoses;
	TArray<float, TInlineAllocator<8>>& SourceWeights = BlendData.SourceWeights;
	TArray<FBlendedCurve, TInlineAllocator<8>>& SourceCurves = BlendData.SourceCurves;

	SourcePoses.Reset();
	SourceWeights.Reset();
	SourceCurves.Reset();

	if (ensure(Poses.Num() == CachedAlphas.Num()))
	{
		for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
		{
			const float CurrentAlpha = CachedAlphas[PoseIndex];
			if (CurrentAlpha > ZERO_ANIMWEIGHT_THRESH)
			{
				FPoseContext PoseContext(Output);
				// total alpha shouldn't be zero
				Poses[PoseIndex].Evaluate(PoseContext);

				SourcePoses.Add(PoseContext.Pose);
				SourceCurves.Add(PoseContext.Curve);
				SourceWeights.Add(CurrentAlpha);
			}
		}
	}

	if (SourcePoses.Num() > 0)
	{
		FAnimationRuntime::BlendPosesTogether(SourcePoses, SourceCurves, SourceWeights, Output.Pose, Output.Curve);
	}
	else
	{
		if (bAdditiveNode)
		{
			Output.ResetToAdditiveIdentity();
		}
		else
		{
			Output.ResetToRefPose();
		}
	}
}

void FAnimNode_MultiWayBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	for (int32 PoseIndex=0; PoseIndex <Poses.Num(); ++PoseIndex)
	{
		Poses[PoseIndex].GatherDebugData(DebugData.BranchFlow(CachedAlphas[PoseIndex]));
	}
}