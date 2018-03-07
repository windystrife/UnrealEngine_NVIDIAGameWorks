// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BTTask_WaitBlackboardTime.generated.h"

class UBehaviorTree;

/**
 * Wait task node.
 * Wait for the time specified by a Blackboard key when executed.
 */
UCLASS(hidecategories=Wait)
class AIMODULE_API UBTTask_WaitBlackboardTime : public UBTTask_Wait
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	/** get name of selected blackboard key */
	FName GetSelectedBlackboardKey() const;


protected:

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	struct FBlackboardKeySelector BlackboardKey;
	
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE FName UBTTask_WaitBlackboardTime::GetSelectedBlackboardKey() const
{
	return BlackboardKey.SelectedKeyName;
}
