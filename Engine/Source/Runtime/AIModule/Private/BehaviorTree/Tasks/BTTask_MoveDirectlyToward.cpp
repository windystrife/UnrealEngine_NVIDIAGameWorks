// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"

UBTTask_MoveDirectlyToward::UBTTask_MoveDirectlyToward(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "MoveDirectlyToward";
	bUsePathfinding = false;

	bProjectVectorGoalToNavigation = true;
	bDisablePathUpdateOnGoalLocationChange = false;
	bUpdatedDeprecatedProperties = false;
}

void UBTTask_MoveDirectlyToward::InitializeFromAsset(UBehaviorTree& Asset)
{
	bUpdatedDeprecatedProperties = true;
	Super::InitializeFromAsset(Asset);
}

void UBTTask_MoveDirectlyToward::PostLoad()
{
	Super::PostLoad();

	if (!bUpdatedDeprecatedProperties)
	{
		bProjectGoalLocation = bProjectVectorGoalToNavigation;
		bTrackMovingGoal = bDisablePathUpdateOnGoalLocationChange;
		bUpdatedDeprecatedProperties = true;
	}
}

#if WITH_EDITOR

FName UBTTask_MoveDirectlyToward::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MoveDirectlyToward.Icon");
}

#endif	// WITH_EDITOR
