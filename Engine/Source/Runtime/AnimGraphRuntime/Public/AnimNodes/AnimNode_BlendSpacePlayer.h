// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "AnimNode_BlendSpacePlayer.generated.h"

class UBlendSpaceBase;

//@TODO: Comment
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendSpacePlayer : public FAnimNode_AssetPlayerBase
{
	GENERATED_USTRUCT_BODY()

public:
	// The X coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Coordinates, meta=(PinShownByDefault))
	mutable float X;

	// The Y coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinShownByDefault))
	mutable float Y;

	// The Z coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinHiddenByDefault))
	mutable float Z;

	// The play rate multiplier. Can be negative, which will cause the animation to play in reverse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "1.0", PinHiddenByDefault))
	mutable float PlayRate;

	// Should the animation continue looping when it reaches the end?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "true", PinHiddenByDefault))
	mutable bool bLoop;

	// The start up position in [0, 1], it only applies when reinitialized
	// if you loop, it will still start from 0.f after finishing the round
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta = (DefaultValue = "0.f", PinHiddenByDefault))
	mutable float StartPosition;

	// The blendspace asset to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	UBlendSpaceBase* BlendSpace;

	// Whether we should reset the current play time when the blend space changes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bResetPlayTimeWhenBlendSpaceChanges;

protected:

	UPROPERTY()
	FBlendFilter BlendFilter;

	UPROPERTY()
	TArray<FBlendSampleData> BlendSampleDataCache;

	UPROPERTY(transient)
	UBlendSpaceBase* PreviousBlendSpace;

public:	
	FAnimNode_BlendSpacePlayer();

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
	virtual void OverrideAsset(UAnimationAsset* NewAsset) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	float GetTimeFromEnd(float CurrentTime);

	UAnimationAsset* GetAnimAsset();

protected:
	void UpdateInternal(const FAnimationUpdateContext& Context);

private:
	void Reinitialize(bool bResetTime = true);

	const FBlendSampleData* GetHighestWeightedSample() const;
};
