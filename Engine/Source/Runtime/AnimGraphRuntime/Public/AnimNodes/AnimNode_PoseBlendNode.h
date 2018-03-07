// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNode_PoseHandler.h"
#include "AlphaBlend.h"
#include "AnimNode_PoseBlendNode.generated.h"

// Evaluates a point in an anim sequence, using a specific time input rather than advancing time internally.
// Typically the playback position of the animation for this node will represent something other than time, like jump height.
// This node will not trigger any notifies present in the associated sequence.
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_PoseBlendNode : public FAnimNode_PoseHandler
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	/** Type of blending used (Linear, Cubic, etc.) */
	UPROPERTY(EditAnywhere, Category = "Blend")
	EAlphaBlendOption BlendOption;

	/** If you're using Custom BlendOption, you can specify curve */
	UPROPERTY(EditAnywhere, Category = "Blend")
	UCurveFloat* CustomCurve;

public:	
	FAnimNode_PoseBlendNode();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

};

