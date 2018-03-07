// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_TwoWayBlend.generated.h"

// This represents a baked transition
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FInputScaleBias AlphaScaleBias;

protected:
	UPROPERTY(Transient)
	mutable float InternalBlendAlpha;

	UPROPERTY(Transient)
	mutable bool bAIsRelevant;

	UPROPERTY(Transient)
	mutable bool bBIsRelevant;

	/** This reinitializes child pose when re-activated. For example, when active child changes */
	UPROPERTY(EditAnywhere, Category = Option)
	bool bResetChildOnActivation;

public:
	FAnimNode_TwoWayBlend()
		: Alpha(0.0f)
		, bResetChildOnActivation(false)
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

