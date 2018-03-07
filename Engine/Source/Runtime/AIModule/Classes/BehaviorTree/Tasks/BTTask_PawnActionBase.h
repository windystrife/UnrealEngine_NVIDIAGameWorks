// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PawnActionBase.generated.h"

class UPawnAction;

enum class EPawnActionTaskResult : uint8
{
	Unknown,
	TaskFinished,
	TaskAborted,
	ActionLost,
};

/**
 * Base class for managing pawn actions
 *
 * Task will set itself as action observer before pushing it to AI Controller,
 * override OnActionEvent if you need any special event handling.
 *
 * Please use result returned by PushAction for ExecuteTask function.
 */

UCLASS(Abstract)
class AIMODULE_API UBTTask_PawnActionBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:

	/** starts executing pawn action */
	EBTNodeResult::Type PushAction(UBehaviorTreeComponent& OwnerComp, UPawnAction& Action);

	/** action observer, updates state of task */
	virtual void OnActionEvent(UPawnAction& Action, EPawnActionEventType::Type Event);

	/** called when action is removed from stack (FinishedAborting) by some external event
	 *  default behavior: finish task as failed */
	virtual void OnActionLost(UPawnAction& Action);

public:

	/** helper functions, should be used when behavior tree task deals with pawn actions, but can't derive from this class */
	static EPawnActionTaskResult ActionEventHandler(UBTTaskNode* TaskNode, UPawnAction& Action, EPawnActionEventType::Type Event);
};
