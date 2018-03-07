// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BlueprintBase.generated.h"

class AActor;
class AAIController;
class APawn;
class UBehaviorTree;

/**
 *  Base class for blueprint based task nodes. Do NOT use it for creating native c++ classes!
 *
 *  When task receives Abort event, all latent actions associated this instance are being removed.
 *  This prevents from resuming activity started by Execute, but does not handle external events.
 *  Please use them safely (unregister at abort) and call IsTaskExecuting() when in doubt.
 */

UCLASS(Abstract, Blueprintable)
class AIMODULE_API UBTTask_BlueprintBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual void OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual void SetOwner(AActor* ActorOwner) override;

#if WITH_EDITOR
	virtual bool UsesBlueprint() const override;
#endif

protected:
	/** Cached AIController owner of BehaviorTreeComponent. */
	UPROPERTY(Transient)
	AAIController* AIOwner;

	/** Cached actor owner of BehaviorTreeComponent. */
	UPROPERTY(Transient)
	AActor* ActorOwner;

	/** temporary variable for ReceiveExecute(Abort)-FinishExecute(Abort) chain */
	mutable TEnumAsByte<EBTNodeResult::Type> CurrentCallResult;

	/** properties that should be copied */
	TArray<UProperty*> PropertyData;

	/** show detailed information about properties */
	UPROPERTY(EditInstanceOnly, Category=Description)
	uint32 bShowPropertyDetails : 1;

	/** set if ReceiveTick is implemented by blueprint */
	uint32 ReceiveTickImplementations : 2;

	/** set if ReceiveExecute is implemented by blueprint */
	uint32 ReceiveExecuteImplementations : 2;

	/** set if ReceiveAbort is implemented by blueprint */
	uint32 ReceiveAbortImplementations : 2;

	/** set when task enters Aborting state */
	uint32 bIsAborting : 1;

	/** if set, execution is inside blueprint's ReceiveExecute(Abort) event
	  * FinishExecute(Abort) function should store their result in CurrentCallResult variable */
	mutable uint32 bStoreFinishResult : 1;

	/** entry point, task will stay active until FinishExecute is called.
	 *	@Note that if both generic and AI event versions are implemented only the more 
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveExecute(AActor* OwnerActor);

	/** if blueprint graph contains this event, task will stay active until FinishAbort is called
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveAbort(AActor* OwnerActor);

	/** tick function
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveTick(AActor* OwnerActor, float DeltaSeconds);

	/** Alternative AI version of ReceiveExecute
	*	@see ReceiveExecute for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveExecuteAI(AAIController* OwnerController, APawn* ControlledPawn);

	/** Alternative AI version of ReceiveAbort
	 *	@see ReceiveAbort for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveAbortAI(AAIController* OwnerController, APawn* ControlledPawn);

	/** Alternative AI version of tick function.
	 *	@see ReceiveTick for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveTickAI(AAIController* OwnerController, APawn* ControlledPawn, float DeltaSeconds);

	/** finishes task execution with Success or Fail result */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	void FinishExecute(bool bSuccess);

	/** aborts task execution */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	void FinishAbort();

	/** task execution will be finished (with result 'Success') after receiving specified message */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	void SetFinishOnMessage(FName MessageName);

	/** task execution will be finished (with result 'Success') after receiving specified message with indicated ID */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	void SetFinishOnMessageWithId(FName MessageName, int32 RequestID = -1);

	/** check if task is currently being executed */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	bool IsTaskExecuting() const;
	
	/** check if task is currently being aborted */
	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree")
	bool IsTaskAborting() const;

	/** ticks this task */
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
