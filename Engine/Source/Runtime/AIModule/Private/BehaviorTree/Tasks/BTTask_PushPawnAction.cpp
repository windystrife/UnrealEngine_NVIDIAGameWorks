// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_PushPawnAction.h"
#include "Actions/PawnAction.h"

UBTTask_PushPawnAction::UBTTask_PushPawnAction(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Push PawnAction";
}

EBTNodeResult::Type UBTTask_PushPawnAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UPawnAction* ActionCopy = Action ? DuplicateObject<UPawnAction>(Action, &OwnerComp) : nullptr;
	if (ActionCopy == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	return PushAction(OwnerComp, *ActionCopy);
}

FString UBTTask_PushPawnAction::GetStaticDescription() const
{
	//return FString::Printf(TEXT("Push Action: %s"), Action ? *Action->GetDisplayName() : TEXT("None"));
	return FString::Printf(TEXT("Push Action: %s"), *GetNameSafe(Action));
}
