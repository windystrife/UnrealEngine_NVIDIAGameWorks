// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTNode.h"
#include "BTTaskNode.generated.h"

class UBTService;

/** 
 * Task are leaf nodes of behavior tree, which perform actual actions
 *
 * Because some of them can be instanced for specific AI, following virtual functions are not marked as const:
 *  - ExecuteTask
 *  - AbortTask
 *  - TickTask
 *  - OnMessage
 *
 * If your node is not being instanced (default behavior), DO NOT change any properties of object within those functions!
 * Template nodes are shared across all behavior tree components using the same tree asset and must store
 * their runtime properties in provided NodeMemory block (allocation size determined by GetInstanceMemorySize() )
 *
 */

UCLASS(Abstract)
class AIMODULE_API UBTTaskNode : public UBTNode
{
	GENERATED_UCLASS_BODY()

	/** starts this task, should return Succeeded, Failed or InProgress
	 *  (use FinishLatentTask() when returning InProgress)
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

protected:
	/** aborts this task, should return Aborted or InProgress
	 *  (use FinishLatentAbort() when returning InProgress)
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

public:
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;

	/** message observer's hook */
	void ReceivedMessage(UBrainComponent* BrainComp, const FAIMessage& Message);

	/** wrapper for node instancing: ExecuteTask */
	EBTNodeResult::Type WrappedExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;

	/** wrapper for node instancing: AbortTask */
	EBTNodeResult::Type WrappedAbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;

	/** wrapper for node instancing: TickTask */
	void WrappedTickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) const;

	/** wrapper for node instancing: OnTaskFinished */
	void WrappedOnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) const;

	/** helper function: finish latent executing */
	void FinishLatentTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type TaskResult) const;

	/** helper function: finishes latent aborting */
	void FinishLatentAbort(UBehaviorTreeComponent& OwnerComp) const;

	/** @return true if task search should be discarded when this task is selected to execute but is already running */
	bool ShouldIgnoreRestartSelf() const;

	/** service nodes */
	UPROPERTY()
	TArray<UBTService*> Services;

protected:

	/** if set, task search will be discarded when this task is selected to execute but is already running */
	UPROPERTY(EditAnywhere, Category=Task)
	uint32 bIgnoreRestartSelf : 1;

	/** if set, TickTask will be called */
	uint32 bNotifyTick : 1;

	/** if set, OnTaskFinished will be called */
	uint32 bNotifyTaskFinished : 1;
	
	/** ticks this task 
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds);

	/** message handler, default implementation will finish latent execution/abortion
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess);

	/** called when task execution is finished
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult);

	/** register message observer */
	void WaitForMessage(UBehaviorTreeComponent& OwnerComp, FName MessageType) const;
	void WaitForMessage(UBehaviorTreeComponent& OwnerComp, FName MessageType, int32 RequestID) const;
	
	/** unregister message observers */
	void StopWaitingForMessages(UBehaviorTreeComponent& OwnerComp) const;
};

FORCEINLINE bool UBTTaskNode::ShouldIgnoreRestartSelf() const
{
	return bIgnoreRestartSelf;
}
