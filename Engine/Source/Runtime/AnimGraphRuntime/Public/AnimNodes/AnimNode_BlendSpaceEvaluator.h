// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNode_BlendSpaceEvaluator.generated.h"

// Evaluates a point in a blendspace, using a specific time input rather than advancing time internally.
// Typically the playback position of the animation for this node will represent something other than time, like jump height.
// This node will not trigger any notifies present in the associated sequence.
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendSpaceEvaluator : public FAnimNode_BlendSpacePlayer
{
	GENERATED_USTRUCT_BODY()

public:
	/** Normalized time between [0,1]. The actual length of a blendspace is dynamic based on the coordinate, so it is exposed as a normalized value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float NormalizedTime;

public:	
	FAnimNode_BlendSpaceEvaluator();

	// FAnimNode_Base interface
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
