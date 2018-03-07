// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;
class UBehaviorTree;
class UBehaviorTreeComponent;

struct AIMODULE_API FBehaviorTreeDelegates
{
	/** delegate type for tree execution events (Params: const UBehaviorTreeComponent* OwnerComp, const UBehaviorTree* TreeAsset) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTreeStarted, const UBehaviorTreeComponent&, const UBehaviorTree& );

	/** delegate type for locking AI debugging tool on pawn */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTreeDebugTool, const APawn*);

	/** Called when the behavior tree starts execution. */
	static FOnTreeStarted OnTreeStarted;

	/** Called when the AI debug tool highlights a pawn. */
	static FOnTreeDebugTool OnDebugSelected;

	/** Called when the AI debug tool locks on pawn. */
	static FOnTreeDebugTool OnDebugLocked;
};
