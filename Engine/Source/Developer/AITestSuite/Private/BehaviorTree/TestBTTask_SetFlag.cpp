// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTTask_SetFlag.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/BlackboardComponent.h"

UTestBTTask_SetFlag::UTestBTTask_SetFlag(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Log";
	TaskResult = EBTNodeResult::Succeeded;
	KeyName = TEXT("Bool1");
	bValue = true;
}

EBTNodeResult::Type UTestBTTask_SetFlag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	OwnerComp.GetBlackboardComponent()->SetValue<UBlackboardKeyType_Bool>(KeyName, bValue);
	return TaskResult;
}
