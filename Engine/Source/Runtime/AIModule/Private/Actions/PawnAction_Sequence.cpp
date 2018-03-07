// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Sequence.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"

UPawnAction_Sequence::UPawnAction_Sequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SubActionTriggeringPolicy(EPawnSubActionTriggeringPolicy::CopyBeforeTriggering)
{
}

UPawnAction_Sequence* UPawnAction_Sequence::CreateAction(UWorld& World, TArray<UPawnAction*>& ActionSequence, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy)
{
	ActionSequence.Remove(NULL);
	if (ActionSequence.Num() <= 0)
	{
		return NULL;
	}

	UPawnAction_Sequence* Action = UPawnAction::CreateActionInstance<UPawnAction_Sequence>(World);
	if (Action)
	{
		Action->ActionSequence = ActionSequence;

		for (const UPawnAction* SubAction : ActionSequence)
		{
			if (SubAction && SubAction->ShouldPauseMovement())
			{
				Action->bShouldPauseMovement = true;
				break;
			}
		}

		Action->SubActionTriggeringPolicy = InSubActionTriggeringPolicy;
	}

	return Action;
}

bool UPawnAction_Sequence::Start()
{
	bool bResult = Super::Start();

	if (bResult)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("Starting sequence. Items:"), *GetName());
		for (auto Action : ActionSequence)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("    %s"), *GetNameSafe(Action));
		}

		bResult = PushNextActionCopy();
	}

	return bResult;
}

bool UPawnAction_Sequence::Resume()
{
	bool bResult = Super::Resume();

	if (bResult)
	{
		bResult = PushNextActionCopy();
	}

	return bResult;
}

void UPawnAction_Sequence::OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult)
{
	if (RecentActionCopy == &Action)
	{
		if (WithResult == EPawnActionResult::Success || (WithResult == EPawnActionResult::Failed && ChildFailureHandlingMode == EPawnActionFailHandling::IgnoreFailure))
		{
			if (GetAbortState() == EPawnActionAbortState::NotBeingAborted)
			{
				PushNextActionCopy();
			}
		}
		else
		{
			Finish(EPawnActionResult::Failed);
		}
	}

	Super::OnChildFinished(Action, WithResult);
}

bool UPawnAction_Sequence::PushNextActionCopy()
{
	if (CurrentActionIndex >= uint32(ActionSequence.Num()))
	{
		Finish(EPawnActionResult::Success);
		return true;
	}

	UPawnAction* ActionCopy = SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering
		? Cast<UPawnAction>(StaticDuplicateObject(ActionSequence[CurrentActionIndex], this))
		: ActionSequence[CurrentActionIndex];

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> pushing action %s")
		, *GetName(), *GetNameSafe(ActionCopy));
	++CurrentActionIndex;	
	check(ActionCopy);
	RecentActionCopy = ActionCopy;
	return PushChildAction(*ActionCopy);
}
