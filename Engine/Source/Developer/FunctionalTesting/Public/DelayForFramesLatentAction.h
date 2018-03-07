// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "LatentActions.h"

struct FLatentActionInfo;

class FDelayForFramesLatentAction : public FPendingLatentAction
{
public:
	FDelayForFramesLatentAction(const FLatentActionInfo& LatentInfo, int32 NumFrames);
	virtual ~FDelayForFramesLatentAction();

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif

private:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	int32 FramesRemaining;
};
