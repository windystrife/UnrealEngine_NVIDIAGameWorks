// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_ReachedMoveGoal.generated.h"

/**
 * Reached Move Goal decorator node.
 * A decorator node that bases its condition on whether the AI controller's path following component returns that it has reached its goal.
 */
UCLASS(meta = (DeprecatedNode, DeprecationMessage = "Please use IsAtLocation decorator instead."))
class AIMODULE_API UBTDecorator_ReachedMoveGoal : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
