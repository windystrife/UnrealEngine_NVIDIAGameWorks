// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_Loop.generated.h"

struct FBTLoopDecoratorMemory
{
	int32 SearchId;
	uint8 RemainingExecutions;
	float TimeStarted;
};

/**
 * Loop decorator node.
 * A decorator node that bases its condition on whether its loop counter has been exceeded.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_Loop : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	/** number of executions */
	UPROPERTY(Category=Decorator, EditAnywhere, meta=(EditCondition="!bInfiniteLoop"))
	int32 NumLoops;

	/** infinite loop */
	UPROPERTY(Category = Decorator, EditAnywhere)
	bool bInfiniteLoop;

	/** timeout (when looping infinitely, when we finish a loop we will check whether we have spent this time looping, if we have we will stop looping). A negative value means loop forever. */
	UPROPERTY(Category = Decorator, EditAnywhere, meta = (EditCondition = "bInfiniteLoop"))
	float InfiniteLoopTimeoutTime;

	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;
};
