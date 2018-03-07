// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/LatentActionManager.h"
#include "LatentActions.h"

// FDelayAction
// A simple delay action; counts down and triggers it's output link when the time remaining falls to zero
class FDelayAction : public FPendingLatentAction
{
public:
	float TimeRemaining;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FDelayAction(float Duration, const FLatentActionInfo& LatentInfo)
		: TimeRemaining(Duration)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		TimeRemaining -= Response.ElapsedTime();
		Response.FinishAndTriggerIf(TimeRemaining <= 0.0f, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override
	{
		static const FNumberFormattingOptions DelayTimeFormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(3)
			.SetMaximumFractionalDigits(3);
		return FText::Format(NSLOCTEXT("DelayAction", "DelayActionTimeFmt", "Delay ({0} seconds left)"), FText::AsNumber(TimeRemaining, &DelayTimeFormatOptions)).ToString();
	}
#endif
};



