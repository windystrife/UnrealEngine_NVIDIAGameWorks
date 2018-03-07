// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AITypes.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveTo.generated.h"

class UAITask_MoveTo;
class UBlackboardComponent;

struct FBTMoveToTaskMemory
{
	/** Move request ID */
	FAIRequestID MoveRequestID;

	FDelegateHandle BBObserverDelegateHandle;
	FVector PreviousGoalLocation;

	TWeakObjectPtr<UAITask_MoveTo> Task;

	uint8 bWaitingForPath : 1;
	uint8 bObserverCanFinishTask : 1;
};

/**
 * Move To task node.
 * Moves the AI pawn toward the specified Actor or Location blackboard entry using the navigation system.
 */
UCLASS(config=Game)
class AIMODULE_API UBTTask_MoveTo : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** fixed distance added to threshold between AI and goal location in destination reach test */
	UPROPERTY(config, Category = Node, EditAnywhere, meta=(ClampMin = "0.0", UIMin="0.0"))
	float AcceptableRadius;

	/** "None" will result in default filter being used */
	UPROPERTY(Category = Node, EditAnywhere)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	/** if task is expected to react to changes to location represented by BB key 
	 *	this property can be used to tweak sensitivity of the mechanism. Value is 
	 *	recommended to be less then AcceptableRadius */
	UPROPERTY(Category=Blackboard, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "1", UIMin = "1", EditCondition="bObserveBlackboardValue"))
	float ObservedBlackboardValueTolerance;

	/** if move goal in BB changes the move will be redirected to new location */
	UPROPERTY()
	uint32 bObserveBlackboardValue : 1;

	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bAllowStrafe : 1;

	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY(Category = Node, EditAnywhere, AdvancedDisplay)
	uint32 bAllowPartialPath : 1;

	/** if set, path to goal actor will update itself when actor moves */
	UPROPERTY(Category = Node, EditAnywhere, AdvancedDisplay)
	uint32 bTrackMovingGoal : 1;

	/** if set, goal location will be projected on navigation data (navmesh) before using */
	UPROPERTY(Category = Node, EditAnywhere, AdvancedDisplay)
	uint32 bProjectGoalLocation : 1;

	/** if set, radius of AI's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bReachTestIncludesAgentRadius : 1;
	
	/** if set, radius of goal's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bReachTestIncludesGoalRadius : 1;

	/** DEPRECATED, please use combination of bReachTestIncludes*Radius instead */
	UPROPERTY(Category = Node, VisibleInstanceOnly)
	uint32 bStopOnOverlap : 1;

	UPROPERTY()
	uint32 bStopOnOverlapNeedsUpdate : 1;

	/** if set, move will use pathfinding. Not exposed on purpose, please use BTTask_MoveDirectlyToward */
	uint32 bUsePathfinding : 1;

	/** set automatically if move should use GameplayTasks */
	uint32 bUseGameplayTasks : 1;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void PostLoad() override;

	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	virtual void OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess) override;
	EBlackboardNotificationResult OnBlackboardValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
	virtual void OnNodeCreated() override;
#endif // WITH_EDITOR

protected:

	EBTNodeResult::Type PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
	
	/** prepares move task for activation */
	virtual UAITask_MoveTo* PrepareMoveTask(UBehaviorTreeComponent& OwnerComp, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest);
};
