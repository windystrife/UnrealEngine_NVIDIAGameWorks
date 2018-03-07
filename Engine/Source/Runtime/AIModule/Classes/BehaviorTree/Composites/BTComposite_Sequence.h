// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_Sequence.generated.h"

/**
 * Sequence composite node.
 * Sequence Nodes execute their children from left to right, and will stop executing its children when one of their children fails.
 * If a child fails, then the Sequence fails. If all the Sequence's children succeed, then the Sequence succeeds.
 */
UCLASS()
class AIMODULE_API UBTComposite_Sequence : public UBTCompositeNode
{
	GENERATED_UCLASS_BODY()

	int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const;

#if WITH_EDITOR
	virtual bool CanAbortLowerPriority() const override;
	virtual FName GetNodeIconName() const override;
#endif
};
