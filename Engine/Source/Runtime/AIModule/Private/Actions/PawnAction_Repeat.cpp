// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Repeat.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"

UPawnAction_Repeat::UPawnAction_Repeat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SubActionTriggeringPolicy(EPawnSubActionTriggeringPolicy::CopyBeforeTriggering)
{
	ChildFailureHandlingMode = EPawnActionFailHandling::IgnoreFailure;
}

UPawnAction_Repeat* UPawnAction_Repeat::CreateAction(UWorld& World, UPawnAction* ActionToRepeat, int32 NumberOfRepeats, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy)
{
	if (ActionToRepeat == NULL || !(NumberOfRepeats > 0 || NumberOfRepeats == UPawnAction_Repeat::LoopForever))
	{
		return NULL;
	}

	UPawnAction_Repeat* Action = UPawnAction::CreateActionInstance<UPawnAction_Repeat>(World);
	if (Action)
	{
		Action->ActionToRepeat = ActionToRepeat;
		Action->RepeatsLeft = NumberOfRepeats;
		Action->SubActionTriggeringPolicy = InSubActionTriggeringPolicy;

		Action->bShouldPauseMovement = ActionToRepeat->ShouldPauseMovement();
	}

	return Action;
}

bool UPawnAction_Repeat::Start()
{
	bool bResult = Super::Start();
	
	if (bResult)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("Starting repeating action: %s. Requested repeats: %d")
			, *GetNameSafe(ActionToRepeat), RepeatsLeft);
		bResult = PushSubAction();
	}

	return bResult;
}

bool UPawnAction_Repeat::Resume()
{
	bool bResult = Super::Resume();

	if (bResult)
	{
		bResult = PushSubAction();
	}

	return bResult;
}

void UPawnAction_Repeat::OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult)
{
	if (RecentActionCopy == &Action)
	{
		if (WithResult == EPawnActionResult::Success || (WithResult == EPawnActionResult::Failed && ChildFailureHandlingMode == EPawnActionFailHandling::IgnoreFailure))
		{
			PushSubAction();
		}
		else
		{
			Finish(EPawnActionResult::Failed);
		}
	}

	Super::OnChildFinished(Action, WithResult);
}

bool UPawnAction_Repeat::PushSubAction()
{
	if (ActionToRepeat == NULL)
	{
		Finish(EPawnActionResult::Failed);
		return false;
	}
	else if (RepeatsLeft == 0)
	{
		Finish(EPawnActionResult::Success);
		return true;
	}

	if (RepeatsLeft > 0)
	{
		--RepeatsLeft;
	}

	UPawnAction* ActionCopy = SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering 
		? Cast<UPawnAction>(StaticDuplicateObject(ActionToRepeat, this))
		: ActionToRepeat;

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> pushing repeted action %s %s, repeats left: %d")
		, *GetName(), SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering ? TEXT("copy") : TEXT("instance")
		, *GetNameSafe(ActionCopy), RepeatsLeft);
	check(ActionCopy);
	RecentActionCopy = ActionCopy;
	return PushChildAction(*ActionCopy); 
}
