// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction.h"
#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Actions/PawnActionsComponent.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY(LogPawnAction);

namespace
{
	FString GetActionResultName(int64 Value)
	{
		static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPawnActionResult"));
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}
}

UPawnAction::UPawnAction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RequiredResources(FAIResourcesSet::AllResources)
	, AbortState(EPawnActionAbortState::NeverStarted)
	, FinishResult(EPawnActionResult::NotStarted)
{
	// actions start their lives paused
	bPaused = true;
	bFailedToStart = false;
	IndexOnStack = INDEX_NONE;
}

UWorld* UPawnAction::GetWorld() const
{
	return OwnerComponent ? OwnerComponent->GetWorld() : Cast<UWorld>(GetOuter());
}

void UPawnAction::Tick(float DeltaTime)
{
}

EPawnActionAbortState::Type UPawnAction::Abort(EAIForceParam::Type ShouldForce)
{
	// if already aborting, and this request is not Forced, just skip it
	if (AbortState != EPawnActionAbortState::NotBeingAborted && ShouldForce != EAIForceParam::Force)
	{
		if (AbortState == EPawnActionAbortState::NeverStarted)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Discarding Abort request since the action has never been started yet"), *GetName());
		}
		else
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Discarding Abort request due to action being already in abort state"), *GetName());
		}
		
		return AbortState;
	}

	const bool bForce = ShouldForce == EAIForceParam::Force;
	EPawnActionAbortState::Type Result = EPawnActionAbortState::NotBeingAborted;
	EPawnActionAbortState::Type ChildResult = EPawnActionAbortState::AbortDone;

	SetAbortState(EPawnActionAbortState::MarkPendingAbort);

	if (ChildAction != NULL)
	{
		ChildResult = ChildAction->Abort(ShouldForce);

		if (ChildResult == EPawnActionAbortState::NotBeingAborted)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> ChildAction %s failed to carry out proper abortion! Things might get ugly..")
				, *GetName(), *ChildAction->GetName());

			// fake proper result and hope for the best!
			ChildResult = EPawnActionAbortState::AbortDone;
		}
	}

	if (bForce)
	{
		Result = PerformAbort(ShouldForce);
		if (Result != EPawnActionAbortState::AbortDone)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> failed to force-abort! Things might get ugly..")
				, *GetName());

			// fake proper result and hope for the best!
			Result = EPawnActionAbortState::AbortDone;
		}
	}
	else
	{
		switch (ChildResult)
		{
		case EPawnActionAbortState::MarkPendingAbort:
			// this means child is awaiting its abort, so should parent
		case EPawnActionAbortState::LatentAbortInProgress:
			// this means child is performing time-consuming abort. Parent should wait
			Result = EPawnActionAbortState::MarkPendingAbort;
			break;

		case EPawnActionAbortState::AbortDone:
			Result = IsPaused() ? EPawnActionAbortState::MarkPendingAbort : PerformAbort(ShouldForce);
			break;

		default:
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> Unhandled Abort State!")
				, *GetName());
			Result = EPawnActionAbortState::AbortDone;
			break;
		}
	}

	SetAbortState(Result);

	return Result;
}

APawn* UPawnAction::GetPawn() const
{
	return OwnerComponent ? OwnerComponent->GetControlledPawn() : NULL;
}

AController* UPawnAction::GetController() const
{
	return OwnerComponent ? OwnerComponent->GetController() : NULL; 
}

void UPawnAction::SetAbortState(EPawnActionAbortState::Type NewAbortState)
{
	// allowing only progression
	if (NewAbortState <= AbortState)
	{
		return;
	}

	AbortState = NewAbortState;
	if (AbortState == EPawnActionAbortState::AbortDone)
	{
		SendEvent(EPawnActionEventType::FinishedAborting);
	}
}

void UPawnAction::SendEvent(EPawnActionEventType::Type Event)
{
	if (OwnerComponent && OwnerComponent->IsPendingKill() == false)
	{
		// this will get communicated to parent action if needed, latently 
		OwnerComponent->OnEvent(*this, Event);
	}

	ActionObserver.ExecuteIfBound(*this, Event);
}

void UPawnAction::StopWaitingForMessages()
{
	MessageHandlers.Reset();
}

void UPawnAction::SetFinishResult(EPawnActionResult::Type Result)
{
	// once return value had been set it's no longer possible to back to InProgress
	if (Result <= EPawnActionResult::InProgress)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> UPawnAction::SetFinishResult setting FinishResult as EPawnActionResult::InProgress or EPawnActionResult::NotStarted - should not be happening"), *GetName());
		return;
	}

	if (FinishResult != Result)
	{
		FinishResult = Result;
	}
}

void UPawnAction::SetOwnerComponent(UPawnActionsComponent* Component)
{
	if (OwnerComponent != NULL && OwnerComponent != Component)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> UPawnAction::SetOwnerComponent called to change already set valid owner component"), *GetName());
	}

	OwnerComponent = Component;
	if (Component != NULL)
	{
		AAIController* AIController = Cast<AAIController>(Component->GetController());
		if (AIController != NULL)
		{
			BrainComp = AIController->FindComponentByClass<UBrainComponent>();
		}
	}
}

void UPawnAction::SetInstigator(UObject* const InInstigator)
{ 
	if (Instigator && Instigator != InInstigator)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> setting Instigator to %s when already has instigator set to %s")
			, *GetName(), *Instigator->GetName(), InInstigator ? *InInstigator->GetName() : TEXT("<null>"));
	}
	Instigator = InInstigator; 
}

void UPawnAction::Finish(TEnumAsByte<EPawnActionResult::Type> WithResult)
{
	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> finishing with result %s")
		, *GetName(), *GetActionResultName(WithResult));

	SetFinishResult(WithResult);

	StopWaitingForMessages();

	SendEvent(EPawnActionEventType::FinishedExecution);
}

bool UPawnAction::Activate()
{
	bool bResult = false; 

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Activating at priority %s! First start? %s Paused? %s")
		, *GetName()
		, *GetPriorityName()
		, HasBeenStarted() ? TEXT("NO") : TEXT("YES")
		, IsPaused() ? TEXT("YES") : TEXT("NO"));

	if (HasBeenStarted() && IsPaused())
	{
		bResult = Resume();
		if (bResult == false)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Failed to RESUME.")
				, *GetName());
			bFailedToStart = true;
			SetFinishResult(EPawnActionResult::Failed);
			SendEvent(EPawnActionEventType::FailedToStart);
		}
	}
	else 
	{
		bResult = Start();
		if (bResult == false)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Failed to START.")
				, *GetName());
			bFailedToStart = true;
			SetFinishResult(EPawnActionResult::Failed);
			SendEvent(EPawnActionEventType::FailedToStart);
		}
	}

	return bResult;
}

void UPawnAction::OnPopped()
{
	// not calling OnFinish if action haven't actually started
	if (!bFailedToStart || bAlwaysNotifyOnFinished)
	{
		OnFinished(FinishResult);
	}
}

bool UPawnAction::Start()
{
	AbortState = EPawnActionAbortState::NotBeingAborted;
	FinishResult = EPawnActionResult::InProgress;
	bPaused = false;
	return true;
}


bool UPawnAction::Pause(const UPawnAction* PausedBy)
{
	// parent should be paused anyway
	ensure(ParentAction == NULL || ParentAction->IsPaused() == true);

	// don't pause twice, this should be guaranteed by the PawnActionsComponent
	ensure(bPaused == false);
	
	if (AbortState == EPawnActionAbortState::LatentAbortInProgress || AbortState == EPawnActionAbortState::AbortDone)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> Not pausing due to being in unpausable aborting state"), *GetName());
		return false;
	}

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Pausing..."), *GetName());
	
	bPaused = true;

	if (ChildAction)
	{
		ChildAction->Pause(PausedBy);
	}

	return bPaused;
}

bool UPawnAction::Resume()
{
	// parent should be paused anyway
	ensure(ParentAction == NULL || ParentAction->IsPaused() == true);

	// do not unpause twice
	if (bPaused == false)
	{
		return false;
	}
	
	ensure(ChildAction == NULL);

	if (ChildAction)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Resuming child, %s"), *GetName(), *ChildAction->GetName());
		ChildAction->Resume();
	}
	else
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Resuming."), *GetName());
		bPaused = false;
	}

	return !bPaused;
}

void UPawnAction::OnFinished(EPawnActionResult::Type WithResult)
{
}

void UPawnAction::OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult)
{
	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Child \'%s\' finished with result %s")
		, *GetName(), *Action.GetName(), *GetActionResultName(WithResult));

	ensure(Action.ParentAction == this);
	ensure(ChildAction == &Action);
	Action.ParentAction = NULL;
	ChildAction = NULL;
}

void UPawnAction::OnPushed()
{
	IndexOnStack = 0;
	UPawnAction* PrevAction = ParentAction;
	while (PrevAction)
	{
		++IndexOnStack;
		PrevAction = PrevAction->ParentAction;
	}

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Pushed with priority %s, IndexOnStack: %d, instigator %s")
		, *GetName(), *GetPriorityName(), IndexOnStack, *GetNameSafe(Instigator));
}

bool UPawnAction::PushChildAction(UPawnAction& Action)
{
	bool bResult = false;
	
	if (OwnerComponent != NULL)
	{
		UE_CVLOG( ChildAction != NULL
			, GetPawn(), LogPawnAction, Log, TEXT("%s> Pushing child action %s while already having ChildAction set to %s")
			, *GetName(), *Action.GetName(), *ChildAction->GetName());
		
		// copy runtime data
		// note that priority and instigator will get assigned as part of PushAction.

		bResult = OwnerComponent->PushAction(Action, GetPriority(), Instigator);

		// adding a check to make sure important data has been set 
		ensure(Action.GetPriority() == GetPriority() && Action.GetInstigator() == GetInstigator());
	}

	return bResult;
}

//----------------------------------------------------------------------//
// messaging
//----------------------------------------------------------------------//

void UPawnAction::WaitForMessage(FName MessageType, FAIRequestID InRequestID)
{
	MessageHandlers.Add(FAIMessageObserver::Create(BrainComp, MessageType, InRequestID.GetID(), FOnAIMessage::CreateUObject(this, &UPawnAction::HandleAIMessage)));
}

//----------------------------------------------------------------------//
// blueprint interface
//----------------------------------------------------------------------//

UPawnAction* UPawnAction::CreateActionInstance(UObject* WorldContextObject, TSubclassOf<UPawnAction> ActionClass)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World && ActionClass)
	{
		return NewObject<UPawnAction>(World, ActionClass);
	}
	return NULL;
}

//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//

FString UPawnAction::GetStateDescription() const
{
	static const UEnum* AbortStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPawnActionAbortState")); 
		
	if (AbortState != EPawnActionAbortState::NotBeingAborted)
	{
		return *AbortStateEnum->GetDisplayNameTextByValue(AbortState).ToString();
	}
	return IsPaused() ? TEXT("Paused") : TEXT("Active");
}

FString UPawnAction::GetPriorityName() const
{
	static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAIRequestPriority"));
	check(Enum);
	return Enum->GetNameStringByValue(GetPriority());
}

FString UPawnAction::GetDisplayName() const
{
	return GetClass()->GetName();
}
