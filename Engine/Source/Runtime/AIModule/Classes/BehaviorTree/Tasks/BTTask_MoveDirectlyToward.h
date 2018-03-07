// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BTTask_MoveDirectlyToward.generated.h"

class UBehaviorTree;

/**
 * Move Directly Toward task node.
 * Moves the AI pawn toward the specified Actor or Location (Vector) blackboard entry in a straight line, without regard to any navigation system. If you need the AI to navigate, use the "Move To" node instead.
 */
UCLASS(config=Game)
class AIMODULE_API UBTTask_MoveDirectlyToward : public UBTTask_MoveTo
{
	GENERATED_UCLASS_BODY()

	virtual void PostLoad() override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	DEPRECATED_FORGAME(4.12, "This property is now deprecated, use UBTTask_MoveTo::bTrackMovingGoal instead.")
	UPROPERTY()
	uint32 bDisablePathUpdateOnGoalLocationChange : 1;

	DEPRECATED_FORGAME(4.12, "This property is now deprecated, use UBTTask_MoveTo::bProjectGoalLocation instead.")
	UPROPERTY()
	uint32 bProjectVectorGoalToNavigation : 1;

private:

	UPROPERTY()
	uint32 bUpdatedDeprecatedProperties : 1;
};
