// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "AnimNode_SequencePlayer.generated.h"

#pragma once

// Sequence player node
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_SequencePlayer : public FAnimNode_AssetPlayerBase
{
	GENERATED_USTRUCT_BODY()
public:
	// The animation sequence asset to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	UAnimSequenceBase* Sequence;

	// Should the animation continue looping when it reaches the end?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	mutable bool bLoopAnimation;

	// The play rate multiplier. Can be negative, which will cause the animation to play in reverse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	mutable float PlayRate;

	// The start up position, it only applies when reinitialized
	// if you loop, it will still start from 0.f after finishing the round
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	mutable float StartPosition;
public:	
	FAnimNode_SequencePlayer()
		: Sequence(NULL)
		, bLoopAnimation(true)
		, PlayRate(1.0f)
		, StartPosition(0.f)
	{
	}

	// FAnimNode_AssetPlayerBase interface
	virtual float GetCurrentAssetTime();
	virtual float GetCurrentAssetTimePlayRateAdjusted();
	virtual float GetCurrentAssetLength();
	// End of FAnimNode_AssetPlayerBase interface

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output, bool bExpectsAdditivePose) override;
	virtual void OverrideAsset(UAnimationAsset* NewAsset) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	UAnimationAsset* GetAnimAsset() {return Sequence;}

	float GetTimeFromEnd(float CurrentNodeTime);
};
