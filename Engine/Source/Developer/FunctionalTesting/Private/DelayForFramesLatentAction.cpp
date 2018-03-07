// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "DelayForFramesLatentAction.h"
#include "Engine/LatentActionManager.h"

FDelayForFramesLatentAction::FDelayForFramesLatentAction(const FLatentActionInfo& LatentInfo, int32 NumFrames)
	: ExecutionFunction(LatentInfo.ExecutionFunction)
	, OutputLink(LatentInfo.Linkage)
	, CallbackTarget(LatentInfo.CallbackTarget)
	, FramesRemaining(NumFrames)
{
}

FDelayForFramesLatentAction::~FDelayForFramesLatentAction()
{
}

void FDelayForFramesLatentAction::UpdateOperation(FLatentResponse& Response)
{
	--FramesRemaining;
	Response.FinishAndTriggerIf(FramesRemaining <= 0, ExecutionFunction, OutputLink, CallbackTarget);
}

#if WITH_EDITOR
FString FDelayForFramesLatentAction::GetDescription() const
{
	return FString::Printf(TEXT("Delay (%d frames remaining)"), FramesRemaining);
}
#endif
