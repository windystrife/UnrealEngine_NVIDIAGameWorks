// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_AssetPlayerBase.generated.h"

/* Base class for any asset playing anim node */
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_AssetPlayerBase : public FAnimNode_Base
{
	GENERATED_BODY();

	/** If true, "Relevant anim" nodes that look for the highest weighted animation in a state will ignore
	 *  this node
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Relevancy, meta=(PinHiddenByDefault))
	bool bIgnoreForRelevancyTest;

	FAnimNode_AssetPlayerBase();

	/** Get the last encountered blend weight for this node */
	virtual float GetCachedBlendWeight();
	
	/** Set the cached blendweight to zero */
	void ClearCachedBlendWeight();

	/** Get the currently referenced time within the asset player node */
	virtual float GetAccumulatedTime();

	/** Override the currently accumulated time */
	virtual void SetAccumulatedTime(const float& NewTime);

	/** Get the animation asset associated with the node, derived classes should implement this */
	virtual UAnimationAsset* GetAnimAsset();

	/** Initialize function for setup purposes */
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;

	/** Update the node, marked final so we can always handle blendweight caching.
	 *  Derived classes should implement UpdateAssetPlayer
	 */

	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) final override;

	/** Update method for the asset player, to be implemented by derived classes */
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) {};

	// The group index, assigned at compile time based on the editoronly GroupName (or INDEX_NONE if it is not part of any group)
	UPROPERTY()
	int32 GroupIndex;

	// The role this player can assume within the group (ignored if GroupIndex is INDEX_NONE)
	UPROPERTY()
	TEnumAsByte<EAnimGroupRole::Type> GroupRole;

	// Create a tick record for this node
	void CreateTickRecordForNode(const FAnimationUpdateContext& Context, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate);

	// Functions to report data to getters, this is required for all asset players (but can't be pure abstract because of struct instantiation generated code).
	virtual float GetCurrentAssetLength() { return 0.0f; }
	virtual float GetCurrentAssetTime() { return 0.0f; }
	virtual float GetCurrentAssetTimePlayRateAdjusted() { return GetCurrentAssetTime(); }

protected:

	/** Last encountered blendweight for this node */
	UPROPERTY(BlueprintReadWrite, Transient, Category=DoNotEdit)
	float BlendWeight;

	/** Accumulated time used to reference the asset in this node */
	UPROPERTY(BlueprintReadWrite, Transient, Category=DoNotEdit)
	float InternalTimeAccumulator;

	/** Store data about current marker position when using marker based syncing*/
	FMarkerTickRecord MarkerTickRecord;

	/** Track whether we have been full weight previously. Reset when we reach 0 weight*/
	bool bHasBeenFullWeight;
};
