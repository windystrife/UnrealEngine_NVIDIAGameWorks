// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GameplayTaskBase.generated.h"

struct FBTGameplayTaskMemory
{
	TWeakObjectPtr<UAITask> Task;
	uint8 bObserverCanFinishTask : 1;
};

/**
 * Base class for managing gameplay tasks
 * Since AITask doesn't have any kind of success/failed results, default implemenation will only return EBTNode::Succeeded
 *
 * In your ExecuteTask:
 * - use NewBTAITask() helper to create task
 * - initialize task with values if needed
 * - use StartGameplayTask() helper to execute and get node result
 */
UCLASS(Abstract)
class AIMODULE_API UBTTask_GameplayTaskBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	virtual uint16 GetInstanceMemorySize() const override;

protected:

	/** if set, behavior tree task will wait until gameplay tasks finishes */
	UPROPERTY(EditAnywhere, Category = Task, AdvancedDisplay)
	uint32 bWaitForGameplayTask : 1;

	/** start task and initialize FBTGameplayTaskMemory memory block */
	EBTNodeResult::Type StartGameplayTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, UAITask& Task);

	/** get finish result from task */
	virtual EBTNodeResult::Type DetermineGameplayTaskResult(UAITask& Task) const;
};
