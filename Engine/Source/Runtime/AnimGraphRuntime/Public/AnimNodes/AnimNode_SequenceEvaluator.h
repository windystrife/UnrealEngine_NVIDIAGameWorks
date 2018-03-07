// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "AnimNode_SequenceEvaluator.generated.h"

UENUM(BlueprintType)
namespace ESequenceEvalReinit
{
	enum Type
	{
		/** Do not reset InternalTimeAccumulator */
		NoReset,
		/** Reset InternalTimeAccumulator to StartPosition */
		StartPosition,
		/** Reset InternalTimeAccumulator to ExplicitTime */
		ExplicitTime,
	};
}

// Evaluates a point in an anim sequence, using a specific time input rather than advancing time internally.
// Typically the playback position of the animation for this node will represent something other than time, like jump height.
// This node will not trigger any notifies present in the associated sequence.
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_SequenceEvaluator : public FAnimNode_AssetPlayerBase
{
	GENERATED_USTRUCT_BODY()
public:
	// The animation sequence asset to evaluate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	UAnimSequenceBase* Sequence;

	// The time at which to evaluate the associated sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float ExplicitTime;

	/** This only works if bTeleportToExplicitTime is false OR this node is set to use SyncGroup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bShouldLoop;

	/** If true, teleport to explicit time, does NOT advance time (does not trigger notifies, does not extract Root Motion, etc.)
	If false, will advance time (will trigger notifies, extract root motion if applicable, etc.)
	Note: using a sync group forces advancing time regardless of what this option is set to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bTeleportToExplicitTime;

	// The start up position, it only applies when ReinitializationBehavior == StartPosition. Only used when bTeleportToExplicitTime is false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	mutable float StartPosition;

	/** What to do when SequenceEvaluator is reinitialized */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TEnumAsByte<ESequenceEvalReinit::Type> ReinitializationBehavior;

	UPROPERTY(Transient)
	bool bReinitialized;

public:	
	FAnimNode_SequenceEvaluator()
		: ExplicitTime(0.0f)
		, bTeleportToExplicitTime(true)
		, StartPosition(0.f)
		, ReinitializationBehavior(ESequenceEvalReinit::ExplicitTime)
	{
	}

	// FAnimNode_AssetPlayerBase interface
	virtual float GetCurrentAssetTime();
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

	// FAnimNode_AssetPlayerBase Interface
	virtual float GetAccumulatedTime() {return ExplicitTime;}
	virtual void SetAccumulatedTime(const float& NewTime) {ExplicitTime = NewTime;}
	virtual UAnimationAsset* GetAnimAsset() {return Sequence;}
	// End of FAnimNode_AssetPlayerBase Interface
};
