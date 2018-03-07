// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_PawnActionBase.h"
#include "Actions/PawnActionsComponent.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"

UBTTask_PawnActionBase::UBTTask_PawnActionBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Action Base";
}

EBTNodeResult::Type UBTTask_PawnActionBase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return (OwnerComp.GetAIOwner() != nullptr
		&& OwnerComp.GetAIOwner()->GetActionsComp() != nullptr
		&& OwnerComp.GetAIOwner()->GetActionsComp()->AbortActionsInstigatedBy(this, EAIRequestPriority::Logic) > 0) 
		? EBTNodeResult::InProgress : EBTNodeResult::Aborted;
}

EBTNodeResult::Type UBTTask_PawnActionBase::PushAction(UBehaviorTreeComponent& OwnerComp, UPawnAction& Action)
{
	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;

	AAIController* AIOwner = Cast<AAIController>(OwnerComp.GetOwner());
	if (AIOwner)
	{
		if (Action.HasActionObserver())
		{
			UE_VLOG(AIOwner, LogBehaviorTree, Warning, TEXT("Action %s already had an observer, it will be overwritten!"), *Action.GetName());
		}

		Action.SetActionObserver(FPawnActionEventDelegate::CreateUObject(this, &UBTTask_PawnActionBase::OnActionEvent));

		const bool bResult = AIOwner->PerformAction(Action, EAIRequestPriority::Logic, this);		
		if (bResult)
		{
			// don't bother with handling action events, it will be processed in next tick
			NodeResult = EBTNodeResult::InProgress;
		}
	}

	return NodeResult;
}

void UBTTask_PawnActionBase::OnActionEvent(UPawnAction& Action, EPawnActionEventType::Type Event)
{
	EPawnActionTaskResult Result = ActionEventHandler(this, Action, Event);
	if (Result == EPawnActionTaskResult::ActionLost)
	{
		OnActionLost(Action);
	}
}

EPawnActionTaskResult UBTTask_PawnActionBase::ActionEventHandler(UBTTaskNode* TaskNode, UPawnAction& Action, EPawnActionEventType::Type Event)
{
	EPawnActionTaskResult Result = EPawnActionTaskResult::Unknown;

	AAIController* AIOwner = Cast<AAIController>(Action.GetController());
	UBehaviorTreeComponent* OwnerComp = AIOwner ? Cast<UBehaviorTreeComponent>(AIOwner->BrainComponent) : nullptr;
	if (OwnerComp == nullptr)
	{
		UE_VLOG(Action.GetController(), LogBehaviorTree, Warning, TEXT("%s: Unhandled PawnAction event: %s, can't find owning component!"),
			*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode),
			*UPawnActionsComponent::DescribeEventType(Event));

		return Result;
	}

	const EBTTaskStatus::Type TaskStatus = OwnerComp->GetTaskStatus(TaskNode);
	if (TaskStatus == EBTTaskStatus::Active)
	{
		if (Event == EPawnActionEventType::FinishedExecution || Event == EPawnActionEventType::FailedToStart)
		{
			const bool bSucceeded = (Action.GetResult() == EPawnActionResult::Success);
			TaskNode->FinishLatentTask(*OwnerComp, bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
			
			Result = EPawnActionTaskResult::TaskFinished;
		}
		else if (Event == EPawnActionEventType::FinishedAborting)
		{
			Result = EPawnActionTaskResult::ActionLost;
		}
	}
	else if (TaskStatus == EBTTaskStatus::Aborting)
	{
		if (Event == EPawnActionEventType::FinishedAborting ||
			Event == EPawnActionEventType::FinishedExecution || Event == EPawnActionEventType::FailedToStart)
		{
			TaskNode->FinishLatentAbort(*OwnerComp);
			Result = EPawnActionTaskResult::TaskAborted;
		}
	}

	if (Result == EPawnActionTaskResult::Unknown)
	{
		UE_VLOG(Action.GetController(), LogBehaviorTree, Verbose, TEXT("%s: Unhandled PawnAction event: %s TaskStatus: %s"),
			*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode),
			*UPawnActionsComponent::DescribeEventType(Event),
			*UBehaviorTreeTypes::DescribeTaskStatus(TaskStatus));
	}

	return Result;
}

void UBTTask_PawnActionBase::OnActionLost(UPawnAction& Action)
{
	AAIController* AIOwner = Cast<AAIController>(Action.GetController());
	UBehaviorTreeComponent* OwnerComp = AIOwner ? Cast<UBehaviorTreeComponent>(AIOwner->BrainComponent) : nullptr;
	if (OwnerComp)
	{
		UE_VLOG(AIOwner, LogBehaviorTree, Verbose, TEXT("%s: action lost, finishing task as failed"), *UBehaviorTreeTypes::DescribeNodeHelper(this));
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
	}
}
