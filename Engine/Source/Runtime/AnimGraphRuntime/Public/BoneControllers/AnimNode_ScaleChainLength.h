// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ScaleChainLength.generated.h"

class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EScaleChainInitialLength : uint8
{
	/** Use the 'DefaultChainLength' input value. */
	FixedDefaultLengthValue,
	/** Use distance between 'ChainStartBone' and 'ChainEndBone' (in Component Space) */
	Distance,
	/* Use animated chain length (length in local space of every bone from 'ChainStartBone' to 'ChainEndBone' */
	ChainLength,
};

/**
 *	Scale the length of a chain of bones.
 */
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_ScaleChainLength : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink InputPose;

	/** Default chain length, as animated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScaleChainLength, meta = (PinHiddenByDefault))
	float DefaultChainLength;

	UPROPERTY(EditAnywhere, Category = ScaleChainLength)
	FBoneReference ChainStartBone;

	UPROPERTY(EditAnywhere, Category = ScaleChainLength)
	FBoneReference ChainEndBone;

	UPROPERTY(EditAnywhere, Category = ScaleChainLength)
	EScaleChainInitialLength ChainInitialLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ScaleChainLength, meta = (PinShownByDefault))
	FVector TargetLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(Transient)
	float ActualAlpha;

	UPROPERTY(EditAnywhere, Category = Settings)
	FInputScaleBias AlphaScaleBias;

	UPROPERTY(Transient)
	bool bBoneIndicesCached;

	TArray<FCompactPoseBoneIndex> ChainBoneIndices;

public:
	FAnimNode_ScaleChainLength();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

protected:
	float GetInitialChainLength(FCompactPose& InLSPose, FCSPose<FCompactPose>& InCSPose) const;
};
