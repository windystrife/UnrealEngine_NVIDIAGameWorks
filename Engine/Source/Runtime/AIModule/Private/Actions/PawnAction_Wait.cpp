// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Wait.h"
#include "TimerManager.h"
#include "Engine/World.h"

UPawnAction_Wait::UPawnAction_Wait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimeToWait(0.f)
	, FinishTimeStamp(0.f)
{

}

UPawnAction_Wait* UPawnAction_Wait::CreateAction(UWorld& World, float InTimeToWait)
{
	UPawnAction_Wait* Action = UPawnAction::CreateActionInstance<UPawnAction_Wait>(World);
	
	if (Action != NULL)
	{
		Action->TimeToWait = InTimeToWait;
	}

	return Action;
}

bool UPawnAction_Wait::Start()
{
	if (Super::Start())
	{
		if (TimeToWait >= 0)
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UPawnAction_Wait::TimerDone, TimeToWait);
		}
		// else hang in there for ever!

		return true;
	}

	return false;
}

bool UPawnAction_Wait::Pause(const UPawnAction* PausedBy)
{
	GetWorld()->GetTimerManager().PauseTimer(TimerHandle);
	return true;
}

bool UPawnAction_Wait::Resume()
{
	GetWorld()->GetTimerManager().UnPauseTimer(TimerHandle);
	return true;
}

EPawnActionAbortState::Type UPawnAction_Wait::PerformAbort(EAIForceParam::Type ShouldForce)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	return EPawnActionAbortState::AbortDone;
}

void UPawnAction_Wait::TimerDone()
{
	Finish(EPawnActionResult::Success);
}
