// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_BlendSpaceEvaluator.h"
#include "Animation/BlendSpaceBase.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendSpaceEvaluator

FAnimNode_BlendSpaceEvaluator::FAnimNode_BlendSpaceEvaluator()
	: FAnimNode_BlendSpacePlayer()
	, NormalizedTime(0.f)
{
}

void FAnimNode_BlendSpaceEvaluator::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	InternalTimeAccumulator = FMath::Clamp(NormalizedTime, 0.f, 1.f);
	PlayRate = 0.f;

	UpdateInternal(Context);
}


void FAnimNode_BlendSpaceEvaluator::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *BlendSpace->GetName(), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine, true);
}
