// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Abstract base class of animation made of multiple sequences.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimCompositeBase.h"
#include "AnimComposite.generated.h"

class UAnimSequence;
struct FCompactPose;

UCLASS(config=Engine, hidecategories=UObject, MinimalAPI, BlueprintType)
class UAnimComposite : public UAnimCompositeBase
{
	GENERATED_UCLASS_BODY()

public:
	/** Serializable data that stores section/anim pairing **/
	UPROPERTY()
	struct FAnimTrack AnimationTrack;

#if WITH_EDITORONLY_DATA
	/** Preview Base pose for additive BlendSpace **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	UAnimSequence* PreviewBasePose;
#endif // WITH_EDITORONLY_DATA

	//~ Begin UAnimSequenceBase Interface
	ENGINE_API virtual void HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const override;
	virtual void GetAnimationPose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const override;	
	virtual EAdditiveAnimationType GetAdditiveAnimType() const override;
	virtual bool IsValidAdditive() const override { return GetAdditiveAnimType() != AAT_None; }
	virtual void EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock) override;
	virtual bool HasRootMotion() const override;
	virtual void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const override;
	virtual bool IsNotifyAvailable() const override;
	//~ End UAnimSequenceBase Interface
	//~ Begin UAnimSequence Interface
#if WITH_EDITOR
	virtual class UAnimSequence* GetAdditiveBasePose() const override;
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive = true) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
#endif
	//~ End UAnimSequence Interface

	//~ Begin UAnimCompositeBase Interface
	virtual void InvalidateRecursiveAsset() override;
	virtual bool ContainRecursive(TArray<UAnimCompositeBase*>& CurrentAccumulatedList) override;
	//~End UAnimCompositeBase Interface
};

