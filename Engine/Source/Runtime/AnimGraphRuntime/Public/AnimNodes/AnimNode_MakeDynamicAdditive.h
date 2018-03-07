// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_MakeDynamicAdditive.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_MakeDynamicAdditive : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// Reference pose for additive delta calculation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Base;

	// Pose to make additive
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Additive;

	// Do additive delta calculation in mesh space
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	bool bMeshSpaceAdditive;

public:	
	FAnimNode_MakeDynamicAdditive();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

};
