// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_Slot

void FAnimNode_Slot::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	Source.Initialize(Context);
	WeightData.Reset();

	// If this node has not already been registered with the AnimInstance, do it.
	if (!SlotNodeInitializationCounter.IsSynchronizedWith(Context.AnimInstanceProxy->GetSlotNodeInitializationCounter()))
	{
		SlotNodeInitializationCounter.SynchronizeWith(Context.AnimInstanceProxy->GetSlotNodeInitializationCounter());
		Context.AnimInstanceProxy->RegisterSlotNodeWithAnimInstance(SlotName);
	}
}

void FAnimNode_Slot::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	Source.CacheBones(Context);
}

void FAnimNode_Slot::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Update weights.
	Context.AnimInstanceProxy->GetSlotWeight(SlotName, WeightData.SlotNodeWeight, WeightData.SourceWeight, WeightData.TotalNodeWeight);

	// Update cache in AnimInstance.
	Context.AnimInstanceProxy->UpdateSlotNodeWeight(SlotName, WeightData.SlotNodeWeight, Context.GetFinalBlendWeight());

	const bool bUpdateSource = bAlwaysUpdateSourcePose || FAnimWeight::IsRelevant(WeightData.SourceWeight);
	if (bUpdateSource)
	{
		const float SourceWeight = bAlwaysUpdateSourcePose ? FAnimWeight::GetSmallestRelevantWeight() : WeightData.SourceWeight;
		Source.Update(Context.FractionalWeight(SourceWeight));
	}
}

void FAnimNode_Slot::Evaluate_AnyThread(FPoseContext & Output)
{
	// If not playing a montage, just pass through
	if (WeightData.SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		Source.Evaluate(Output);
	}
	else
	{
		FPoseContext SourceContext(Output);
		if (WeightData.SourceWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			Source.Evaluate(SourceContext);
		}

		Output.AnimInstanceProxy->SlotEvaluatePose(SlotName, SourceContext.Pose, SourceContext.Curve, WeightData.SourceWeight, Output.Pose, Output.Curve, WeightData.SlotNodeWeight, WeightData.TotalNodeWeight);

		checkSlow(!Output.ContainsNaN());
		checkSlow(Output.IsNormalized());
	}
}

void FAnimNode_Slot::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Slot Name: '%s' Weight:%.1f%%)"), *SlotName.ToString(), WeightData.SlotNodeWeight*100.f);
	
	bool const bIsPoseSource = (WeightData.SourceWeight <= ZERO_ANIMWEIGHT_THRESH);
	DebugData.AddDebugItem(DebugLine, bIsPoseSource);
	Source.GatherDebugData(DebugData.BranchFlow(WeightData.SourceWeight));

	for (FAnimMontageInstance* MontageInstance : DebugData.AnimInstance->MontageInstances)
	{
		if (MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotName))
		{
			if (const FAnimTrack* const Track = MontageInstance->Montage->GetAnimationData(SlotName))
			{
				if (const FAnimSegment* const Segment = Track->GetSegmentAtTime(MontageInstance->GetPosition()))
				{
					float CurrentAnimPos;
					if (UAnimSequenceBase* Anim = Segment->GetAnimationData(MontageInstance->GetPosition(), CurrentAnimPos))
					{
						FString MontageLine = FString::Printf(TEXT("Montage('%s') Anim('%s') P(%.2f) W(%.0f%%)"), *MontageInstance->Montage->GetName(), *Anim->GetName(), CurrentAnimPos, WeightData.SlotNodeWeight*100.f);
						DebugData.BranchFlow(1.0f).AddDebugItem(MontageLine, true);
						break;
					}
				}
			}
		}
	}
}

FAnimNode_Slot::FAnimNode_Slot()
	: SlotName(FAnimSlotGroup::DefaultSlotName)
{
}
