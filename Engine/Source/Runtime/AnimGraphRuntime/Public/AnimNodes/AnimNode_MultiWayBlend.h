// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_MultiWayBlend.generated.h"

// This represents a baked transition
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_MultiWayBlend : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	friend struct FAnimSequencerInstanceProxy;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	TArray<FPoseLink> Poses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	TArray<float> DesiredAlphas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bAdditiveNode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bNormalizeAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FInputScaleBias AlphaScaleBias;

public:
	FAnimNode_MultiWayBlend()
		: bAdditiveNode(false)
		, bNormalizeAlpha(true)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	int32 AddPose()
	{
		Poses.AddZeroed();
		DesiredAlphas.AddZeroed();

		return Poses.Num();
	}

	void RemovePose(int32 PoseIndex)
	{
		Poses.RemoveAt(PoseIndex);
		DesiredAlphas.RemoveAt(PoseIndex);
	}

	void ResetPoses()
	{
		Poses.Reset();
		DesiredAlphas.Reset();
	}
private:
	float GetTotalAlpha()
	{
		float TotalAlpha = 0.f;

		for (float Alpha : DesiredAlphas)
		{
			TotalAlpha += Alpha;
		}

		return TotalAlpha;
	}

	// process new weights and then return out
	void UpdateCachedAlphas();

	TArray<float> CachedAlphas;
};

