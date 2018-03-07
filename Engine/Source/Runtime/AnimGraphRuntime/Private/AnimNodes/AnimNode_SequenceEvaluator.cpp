// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "Animation/AnimInstanceProxy.h"

float FAnimNode_SequenceEvaluator::GetCurrentAssetTime()
{
	return ExplicitTime;
}

float FAnimNode_SequenceEvaluator::GetCurrentAssetLength()
{
	return Sequence ? Sequence->SequenceLength : 0.0f;
}

/////////////////////////////////////////////////////
// FAnimSequenceEvaluatorNode

void FAnimNode_SequenceEvaluator::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	bReinitialized = true;
}

void FAnimNode_SequenceEvaluator::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
}

void FAnimNode_SequenceEvaluator::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if (Sequence)
	{
		// Clamp input to a valid position on this sequence's time line.
		ExplicitTime = FMath::Clamp(ExplicitTime, 0.f, Sequence->SequenceLength);

		if ((!bTeleportToExplicitTime || (GroupIndex != INDEX_NONE)) && (Context.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
		{
			if (bReinitialized)
			{
				switch (ReinitializationBehavior)
				{
					case ESequenceEvalReinit::StartPosition: InternalTimeAccumulator = StartPosition; break;
					case ESequenceEvalReinit::ExplicitTime: InternalTimeAccumulator = ExplicitTime; break;
				}
			}

			InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, Sequence->SequenceLength);

			float TimeJump = ExplicitTime - InternalTimeAccumulator;
			if (bShouldLoop)
			{
				if (FMath::Abs(TimeJump) > (Sequence->SequenceLength * 0.5f))
				{
					if (TimeJump > 0.f)
					{
						TimeJump -= Sequence->SequenceLength;
					}
					else
					{
						TimeJump += Sequence->SequenceLength;
					}
				}
			}

			const float DeltaTime = Context.GetDeltaTime();
			const float PlayRate = FMath::IsNearlyZero(DeltaTime) ? 0.f : (TimeJump / DeltaTime);
			CreateTickRecordForNode(Context, Sequence, bShouldLoop, PlayRate);
		}
		else
		{
			InternalTimeAccumulator = ExplicitTime;
		}
	}

	bReinitialized = false;
}

void FAnimNode_SequenceEvaluator::Evaluate_AnyThread(FPoseContext& Output)
{
	check(Output.AnimInstanceProxy != nullptr);
	if ((Sequence != nullptr) && (Output.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
	{
		Sequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(InternalTimeAccumulator, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SequenceEvaluator::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UAnimSequenceBase* NewSequence = Cast<UAnimSequenceBase>(NewAsset))
	{
		Sequence = NewSequence;
	}
}

void FAnimNode_SequenceEvaluator::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' InputTime: %.3f, Time: %.3f)"), *GetNameSafe(Sequence), ExplicitTime, InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine, true);
}
