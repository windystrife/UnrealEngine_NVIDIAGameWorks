// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "BoneContainer.h"
#include "AnimNode_BlendBoneByChannel.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FBlendBoneByChannelEntry
{
	GENERATED_USTRUCT_BODY()

	/** Bone to take Transform from */
	UPROPERTY(EditAnywhere, Category = Blend)
	FBoneReference SourceBone;
	
	/** Bone to apply Transform to */
	UPROPERTY(EditAnywhere, Category = Blend)
	FBoneReference TargetBone;

	/** Copy Translation from Source to Target */
	UPROPERTY(EditAnywhere, Category = Blend)
	bool bBlendTranslation;

	/** Copy Rotation from Source to Target */
	UPROPERTY(EditAnywhere, Category = Blend)
	bool bBlendRotation;

	/** Copy Scale from Source to Target */
	UPROPERTY(EditAnywhere, Category = Blend)
	bool bBlendScale;

	FBlendBoneByChannelEntry()
		: bBlendTranslation(true)
		, bBlendRotation(true)
		, bBlendScale(true)
	{
	}
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendBoneByChannel : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FInputScaleBias AlphaScaleBias;

	UPROPERTY(EditAnywhere, Category = Blend)
	TArray<FBlendBoneByChannelEntry> BoneDefinitions;

	/** Space to convert transforms into prior to copying channels */
	UPROPERTY(EditAnywhere, Category = Blend)
	TEnumAsByte<EBoneControlSpace> TransformsSpace;

protected:
	UPROPERTY(Transient)
	mutable float InternalBlendAlpha;

	UPROPERTY(Transient)
	mutable bool bBIsRelevant;

private:
	// Array of bone entries, that has been validated to be correct at runtime.
	// So we don't have to perform validation checks per frame.
	UPROPERTY(Transient)
	TArray<FBlendBoneByChannelEntry> ValidBoneEntries;

public:
	FAnimNode_BlendBoneByChannel()
		: Alpha(0.0f)
		, TransformsSpace(EBoneControlSpace::BCS_BoneSpace)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};

