// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_AssetPlayerBase::FAnimNode_AssetPlayerBase()
	: bIgnoreForRelevancyTest(false)
	, GroupIndex(INDEX_NONE)
	, BlendWeight(0.0f)
	, InternalTimeAccumulator(0.0f)
	, bHasBeenFullWeight(false)
{

}

void FAnimNode_AssetPlayerBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	MarkerTickRecord.Reset();
	bHasBeenFullWeight = false;
}

void FAnimNode_AssetPlayerBase::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Cache the current weight and update the node
	BlendWeight = Context.GetFinalBlendWeight();
	bHasBeenFullWeight = bHasBeenFullWeight || (BlendWeight >= (1.0f - ZERO_ANIMWEIGHT_THRESH));

	UpdateAssetPlayer(Context);
}

void FAnimNode_AssetPlayerBase::CreateTickRecordForNode(const FAnimationUpdateContext& Context, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate)
{
	// Create a tick record and fill it out
	const float FinalBlendWeight = Context.GetFinalBlendWeight();

	FAnimGroupInstance* SyncGroup;
	const int32 GroupIndexToUse = ((GroupRole < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? GroupIndex : INDEX_NONE;

	FAnimTickRecord& TickRecord = Context.AnimInstanceProxy->CreateUninitializedTickRecord(GroupIndexToUse, /*out*/ SyncGroup);

	Context.AnimInstanceProxy->MakeSequenceTickRecord(TickRecord, Sequence, bLooping, PlayRate, FinalBlendWeight, /*inout*/ InternalTimeAccumulator, MarkerTickRecord);
	TickRecord.RootMotionWeightModifier = Context.GetRootMotionWeightModifier();

	// Update the sync group if it exists
	if (SyncGroup != NULL)
	{
		SyncGroup->TestTickRecordForLeadership(GroupRole);
	}
}

float FAnimNode_AssetPlayerBase::GetCachedBlendWeight()
{
	return BlendWeight;
}

float FAnimNode_AssetPlayerBase::GetAccumulatedTime()
{
	return InternalTimeAccumulator;
}

void FAnimNode_AssetPlayerBase::SetAccumulatedTime(const float& NewTime)
{
	InternalTimeAccumulator = NewTime;
}

UAnimationAsset* FAnimNode_AssetPlayerBase::GetAnimAsset()
{
	return nullptr;
}

void FAnimNode_AssetPlayerBase::ClearCachedBlendWeight()
{
	BlendWeight = 0.0f;
}

