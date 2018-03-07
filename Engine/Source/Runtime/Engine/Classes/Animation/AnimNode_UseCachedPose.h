// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_UseCachedPose.generated.h"

USTRUCT()
struct ENGINE_API FAnimNode_UseCachedPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// Note: This link is intentionally not public; it's wired up during compilation
	UPROPERTY()
	FPoseLink LinkToCachingNode;

	/** Intentionally not exposed, set by AnimBlueprintCompiler */
	UPROPERTY()
	FName CachePoseName;

public:	
	FAnimNode_UseCachedPose();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
