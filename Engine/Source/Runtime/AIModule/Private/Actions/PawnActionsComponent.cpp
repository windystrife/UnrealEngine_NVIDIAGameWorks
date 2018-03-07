// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnActionsComponent.h"
#include "UObject/Package.h"
#include "GameFramework/Controller.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTNode.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "Actions/PawnAction_Sequence.h"

//----------------------------------------------------------------------//
// helpers
//----------------------------------------------------------------------//

namespace
{
	FString GetEventName(int64 Value)
	{
		static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPawnActionEventType"));
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}

	FString GetPriorityName(int64 Value)
	{
		static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAIRequestPriority"));
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}

	FString GetActionSignature(UPawnAction* Action)
	{
		if (Action == NULL)
		{
			return TEXT("NULL");
		}
		
		return FString::Printf(TEXT("[%s, %s]"), *Action->GetName(), *GetPriorityName(Action->GetPriority()));
	}
}

//----------------------------------------------------------------------//
// FPawnActionEvent
//----------------------------------------------------------------------//

namespace
{
	struct FPawnActionEvenSort
	{
		FORCEINLINE bool operator()(const FPawnActionEvent& A, const FPawnActionEvent& B) const
		{
			return A.Priority < B.Priority
				|| (A.Priority == B.Priority
					&& (A.EventType < B.EventType
						|| (A.EventType == B.EventType && A.Index < B.Index)));
		}
	};
}

FPawnActionEvent::FPawnActionEvent(UPawnAction& InAction, EPawnActionEventType::Type InEventType, uint32 InIndex)
	: Action(&InAction), EventType(InEventType), Index(InIndex)
{
	Priority = InAction.GetPriority();
}

//----------------------------------------------------------------------//
// FPawnActionStack
//----------------------------------------------------------------------//

void FPawnActionStack::Pause()
{
	if (TopAction != NULL)
	{
		TopAction->Pause(NULL);
	}
}

void FPawnActionStack::Resume()
{
	if (TopAction != NULL)
	{
		TopAction->Resume();
	}
}

void FPawnActionStack::PushAction(UPawnAction& NewTopAction)
{
	if (TopAction != NULL)
	{
		if (TopAction->IsPaused() == false && TopAction->HasBeenStarted() == true)
		{
			TopAction->Pause(&NewTopAction);
		}
		ensure(TopAction->ChildAction == NULL);
		TopAction->ChildAction = &NewTopAction;
		NewTopAction.ParentAction = TopAction;
	}

	TopAction = &NewTopAction;
	NewTopAction.OnPushed();
}

void FPawnActionStack::PopAction(UPawnAction& ActionToPop)
{
	// first check if it's there
	UPawnAction* CutPoint = TopAction;
	while (CutPoint != NULL && CutPoint != &ActionToPop)
	{
		CutPoint = CutPoint->ParentAction;
	}

	if (CutPoint == &ActionToPop)
	{
		UPawnAction* ActionBeingRemoved = TopAction;
		// note StopAction can be null
		UPawnAction* StopAction = ActionToPop.ParentAction;

		while (ActionBeingRemoved != StopAction && ActionBeingRemoved != nullptr)
		{
			checkSlow(ActionBeingRemoved);
			UPawnAction* NextAction = ActionBeingRemoved->ParentAction;

			if (ActionBeingRemoved->IsBeingAborted() == false && ActionBeingRemoved->IsFinished() == false)
			{
				// forcing abort to make sure it happens instantly. We don't have time for delayed finish here.
				ActionBeingRemoved->Abort(EAIForceParam::Force);
			}
			ActionBeingRemoved->OnPopped();
			if (ActionBeingRemoved->ParentAction)
			{
				ActionBeingRemoved->ParentAction->OnChildFinished(*ActionBeingRemoved, ActionBeingRemoved->FinishResult);
			}
			ActionBeingRemoved = NextAction;
		}

		TopAction = StopAction;
	}
}

int32 FPawnActionStack::GetStackSize() const
{
	int32 Size = 0;
	const UPawnAction* TempAction = TopAction;
	while (TempAction != nullptr)
	{
		TempAction = TempAction->GetParentAction();
		++Size;
	}
	return Size;
}

//----------------------------------------------------------------------//
// UPawnActionsComponent
//----------------------------------------------------------------------//

UPawnActionsComponent::UPawnActionsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bAutoActivate = true;
	bLockedAILogic = false;

	ActionEventIndex = 0;

	ActionStacks.AddZeroed(EAIRequestPriority::MAX);
}

void UPawnActionsComponent::OnUnregister()
{
	if ((ControlledPawn != nullptr) && !ControlledPawn->IsPendingKillPending())
	{
		// call for every regular priority 
		for (int32 PriorityIndex = 0; PriorityIndex < EAIRequestPriority::MAX; ++PriorityIndex)
		{
			UPawnAction* Action = ActionStacks[PriorityIndex].GetTop();
			while (Action)
			{
				Action->Abort(EAIForceParam::Force);
				Action = Action->ParentAction;
			}
		}
	}

	Super::OnUnregister();
}

void UPawnActionsComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ControlledPawn == NULL)
	{
		CacheControlledPawn();
	}

	if (ActionEvents.Num() > 1)
	{
		ActionEvents.Sort(FPawnActionEvenSort());
	}

	if (ActionEvents.Num() > 0)
	{
		for (int32 EventIndex = 0; EventIndex < ActionEvents.Num(); ++EventIndex)
		{
			FPawnActionEvent& Event = ActionEvents[EventIndex];

			if (Event.Action == nullptr)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("NULL action encountered during ActionEvents processing. May result in some notifies not being sent out."));
				continue;
			}

			switch (Event.EventType)
			{
			case EPawnActionEventType::InstantAbort:
				// can result in adding new ActionEvents (from child actions) and reallocating data in ActionEvents array
				// because of it, we need to operate on copy instead of reference to memory address
				{
					FPawnActionEvent EventCopy(Event);
					EventCopy.Action->Abort(EAIForceParam::Force);
					ActionStacks[EventCopy.Priority].PopAction(*EventCopy.Action);
				}
				break;
			case EPawnActionEventType::FinishedAborting:
			case EPawnActionEventType::FinishedExecution:
			case EPawnActionEventType::FailedToStart:
				ActionStacks[Event.Priority].PopAction(*Event.Action);
				break;
			case EPawnActionEventType::Push:
				ActionStacks[Event.Priority].PushAction(*Event.Action);
				break;
			default:
				break;
			}
		}

		ActionEvents.Reset();

		UpdateCurrentAction();
	}

	if (CurrentAction)
	{
		CurrentAction->TickAction(DeltaTime);
	}

	// it's possible we got new events with CurrentAction's tick
	if (ActionEvents.Num() == 0 && (CurrentAction == NULL || CurrentAction->WantsTick() == false))
	{
		SetComponentTickEnabled(false);
	}
}

bool UPawnActionsComponent::HasActiveActionOfType(EAIRequestPriority::Type Priority, TSubclassOf<UPawnAction> PawnActionClass) const
{
	TArray<UPawnAction*> ActionsToTest;
	ActionsToTest.Add(GetActiveAction(Priority));

	while (ActionsToTest.Num() > 0)
	{
		UPawnAction* ActiveActionIter = ActionsToTest[0];

		if (ActiveActionIter)
		{
			if (ActiveActionIter->GetClass()->IsChildOf(*PawnActionClass))
			{
				return true;
			}	
			else
			{
				UPawnAction_Sequence* PawnActionSequence = Cast<UPawnAction_Sequence>(ActiveActionIter);

				if (PawnActionSequence)
				{
					for (int32 PawnActionSequenceCount = 0; PawnActionSequenceCount < PawnActionSequence->ActionSequence.Num(); ++PawnActionSequenceCount)
					{
						ActionsToTest.Add(PawnActionSequence->ActionSequence[PawnActionSequenceCount]);
					}
				}
			}
		}

		ActionsToTest.RemoveAt(0);
	}

	// Didn't find one.
	return false;
}

void UPawnActionsComponent::UpdateCurrentAction()
{
	UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Picking new current actions. Old CurrentAction %s")
		, *GetActionSignature(CurrentAction));

	// find the highest priority action available
	UPawnAction* NewCurrentAction = NULL;
	int32 Priority = EAIRequestPriority::MAX - 1;
	do 
	{
		NewCurrentAction = ActionStacks[Priority].GetTop();

	} while (NewCurrentAction == NULL && --Priority >= 0);

	// if it's a new Action then enable it
	if (CurrentAction != NewCurrentAction)
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("New action: %s")
			, *GetActionSignature(NewCurrentAction));

		if (CurrentAction != NULL && CurrentAction->IsActive())
		{
			CurrentAction->Pause(NewCurrentAction);
		}
		CurrentAction = NewCurrentAction;
		bool bNewActionStartedSuccessfully = true;
		if (CurrentAction != NULL)
		{
			bNewActionStartedSuccessfully = CurrentAction->Activate();
		}

		if (bNewActionStartedSuccessfully == false)
		{
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("CurrentAction %s failed to activate. Removing and re-running action selection")
				, *GetActionSignature(NewCurrentAction));

			CurrentAction = NULL;			
		}
		// @HACK temporary solution to have actions and old BT tasks work together
		else if (CurrentAction == NULL || CurrentAction->GetPriority() != EAIRequestPriority::Logic)
		{
			UpdateAILogicLock();
		}
	}
	else
	{
		if (CurrentAction == NULL)
		{
			UpdateAILogicLock();
		}
		else if (CurrentAction->IsFinished())
		{
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Re-running same action"));
			CurrentAction->Activate();
		}
		else
		{ 
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Still doing the same action"));
		}
	}
}

void UPawnActionsComponent::UpdateAILogicLock()
{
	if (ControlledPawn && ControlledPawn->GetController())
	{
		UBrainComponent* BrainComp = ControlledPawn->GetController()->FindComponentByClass<UBrainComponent>();
		if (BrainComp)
		{
			if (CurrentAction != NULL && CurrentAction->GetPriority() > EAIRequestPriority::Logic)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Locking AI logic"));
				BrainComp->LockResource(EAIRequestPriority::HardScript);
				bLockedAILogic = true;
			}
			else if (bLockedAILogic)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Clearing AI logic lock"));
				bLockedAILogic = false;
				BrainComp->ClearResourceLock(EAIRequestPriority::HardScript);
				if (BrainComp->IsResourceLocked() == false)
				{
					UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Reseting AI logic"));
					BrainComp->RestartLogic();
				}
				// @todo consider checking if lock priority is < Logic
				else
				{
					UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("AI logic still locked with other priority"));
					BrainComp->RequestLogicRestartOnUnlock();
				}
			}
		}
	}
}

EPawnActionAbortState::Type UPawnActionsComponent::K2_AbortAction(UPawnAction* ActionToAbort)
{
	if (ActionToAbort != NULL)
	{
		return AbortAction(*ActionToAbort);
	}
	return EPawnActionAbortState::NeverStarted;
}

EPawnActionAbortState::Type UPawnActionsComponent::AbortAction(UPawnAction& ActionToAbort)
{
	const EPawnActionAbortState::Type AbortState = ActionToAbort.Abort(EAIForceParam::DoNotForce);
	if (AbortState == EPawnActionAbortState::NeverStarted)
	{
		// this is a special case. It's possible someone tried to abort an action that 
		// has just requested to be pushes and the push event has not been processed yet.
		// in such a case we'll look through the awaiting action events and remove a push event 
		// for given ActionToAbort
		RemoveEventsForAction(ActionToAbort);
	}
	return AbortState;
}

void UPawnActionsComponent::RemoveEventsForAction(UPawnAction& PawnAction)
{
	for (int32 ActionIndex = ActionEvents.Num() - 1; ActionIndex >= 0; --ActionIndex)
	{
		if (ActionEvents[ActionIndex].Action == &PawnAction)
		{
			ActionEvents.RemoveAtSwap(ActionIndex, /*Count=*/1, /*bAllowShrinking=*/false);
		}
	}
}

EPawnActionAbortState::Type UPawnActionsComponent::K2_ForceAbortAction(UPawnAction* ActionToAbort)
{
	if (ActionToAbort)
	{
		return ForceAbortAction(*ActionToAbort);
	}
	return EPawnActionAbortState::NeverStarted;
}

EPawnActionAbortState::Type UPawnActionsComponent::ForceAbortAction(UPawnAction& ActionToAbort)
{
	return ActionToAbort.Abort(EAIForceParam::Force);
}

uint32 UPawnActionsComponent::AbortActionsInstigatedBy(UObject* const Instigator, EAIRequestPriority::Type Priority)
{
	uint32 AbortedActionsCount = 0;

	if (Priority == EAIRequestPriority::MAX)
	{
		// call for every regular priority 
		for (int32 PriorityIndex = 0; PriorityIndex < EAIRequestPriority::MAX; ++PriorityIndex)
		{
			AbortedActionsCount += AbortActionsInstigatedBy(Instigator, EAIRequestPriority::Type(PriorityIndex));
		}
	}
	else
	{
		UPawnAction* Action = ActionStacks[Priority].GetTop();
		while (Action)
		{
			if (Action->GetInstigator() == Instigator)
			{
				OnEvent(*Action, EPawnActionEventType::InstantAbort);
				++AbortedActionsCount;
			}
			Action = Action->ParentAction;
		}

		for (int32 ActionIndex = ActionEvents.Num() - 1; ActionIndex >= 0; --ActionIndex)
		{
			const FPawnActionEvent& Event = ActionEvents[ActionIndex];
			if (Event.Priority == Priority &&
				Event.EventType == EPawnActionEventType::Push &&
				Event.Action && Event.Action->GetInstigator() == Instigator)
			{
				ActionEvents.RemoveAtSwap(ActionIndex, /*Count=*/1, /*bAllowShrinking=*/false);
				AbortedActionsCount++;
			}
		}
	}

	return AbortedActionsCount;
}

bool UPawnActionsComponent::K2_PushAction(UPawnAction* NewAction, EAIRequestPriority::Type Priority, UObject* Instigator)
{
	if (NewAction)
	{
		return PushAction(*NewAction, Priority, Instigator);
	}
	return false;
}

bool UPawnActionsComponent::PushAction(UPawnAction& NewAction, EAIRequestPriority::Type Priority, UObject* Instigator)
{
	if (NewAction.HasBeenStarted() == false || NewAction.IsFinished() == true)
	{
		NewAction.ExecutionPriority = Priority;
		NewAction.SetOwnerComponent(this);
		NewAction.SetInstigator(Instigator);
		return OnEvent(NewAction, EPawnActionEventType::Push);
	}

	return false;
}

bool UPawnActionsComponent::OnEvent(UPawnAction& Action, EPawnActionEventType::Type Event)
{
	bool bResult = false;
	const FPawnActionEvent ActionEvent(Action, Event, ActionEventIndex++);

	if (Event != EPawnActionEventType::Invalid && ActionEvents.Find(ActionEvent) == INDEX_NONE)
	{
		ActionEvents.Add(ActionEvent);

		// if it's a first even enable tick
		if (ActionEvents.Num() == 1)
		{
			SetComponentTickEnabled(true);
		}

		bResult = true;
	}
	else if (Event == EPawnActionEventType::Invalid)
	{
		// ignore
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Ignoring Action Event: Action %s Event %s")
			, *Action.GetName(), *GetEventName(Event));
	}
	else
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Ignoring duplicate Action Event: Action %s Event %s")
			, *Action.GetName(), *GetEventName(Event));
	}

	return bResult;
}

void UPawnActionsComponent::SetControlledPawn(APawn* NewPawn)
{
	if (ControlledPawn != NULL && ControlledPawn != NewPawn)
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Trying to set ControlledPawn to new value while ControlledPawn is already set!"));
	}
	else
	{
		ControlledPawn = NewPawn;
	}
}

APawn* UPawnActionsComponent::CacheControlledPawn()
{
	if (ControlledPawn == NULL)
	{
		AActor* ActorOwner = GetOwner();
		if (ActorOwner)
		{
			ControlledPawn = Cast<APawn>(ActorOwner);
			if (ControlledPawn == NULL)
			{
				AController* Controller = Cast<AController>(ActorOwner);
				if (Controller != NULL)
				{
					ControlledPawn = Controller->GetPawn();
				}
			}
		}
	}

	return ControlledPawn;
}


//----------------------------------------------------------------------//
// blueprint interface
//----------------------------------------------------------------------//
bool UPawnActionsComponent::K2_PerformAction(APawn* Pawn, UPawnAction* Action, TEnumAsByte<EAIRequestPriority::Type> Priority)
{
	if (Pawn && Action)
	{
		return PerformAction(*Pawn, *Action, Priority);
	}
	return false;
}

bool UPawnActionsComponent::PerformAction(APawn& Pawn, UPawnAction& Action, TEnumAsByte<EAIRequestPriority::Type> Priority)
{
	bool bSuccess = false;

	ensure(Priority < EAIRequestPriority::MAX);

	if (Pawn.GetController())
	{
		UPawnActionsComponent* ActionComp = Pawn.GetController()->FindComponentByClass<UPawnActionsComponent>();
		if (ActionComp)
		{
			ActionComp->PushAction(Action, Priority);
			bSuccess = true;
		}
	}

	return bSuccess;
}

//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
#if ENABLE_VISUAL_LOG
void UPawnActionsComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	static const FString Category = TEXT("PawnActions");

	if (IsPendingKill())
	{
		return;
	}

	for (int32 PriorityIndex = 0; PriorityIndex < ActionStacks.Num(); ++PriorityIndex)
	{
		const UPawnAction* Action = ActionStacks[PriorityIndex].GetTop();
		if (Action == NULL)
		{
			continue;
		}

		FVisualLogStatusCategory StatusCategory;
		StatusCategory.Category = Category + TEXT(": ") + GetPriorityName(PriorityIndex);

		while (Action)
		{
			FString InstigatorDesc;
			const UObject* InstigatorOb = Action->GetInstigator();
			const UBTNode* InstigatorBT = Cast<const UBTNode>(InstigatorOb);
			InstigatorDesc = InstigatorBT ?
				FString::Printf(TEXT("%s = %s"), *UBehaviorTreeTypes::DescribeNodeHelper(InstigatorBT), *InstigatorBT->GetName()) :
				GetNameSafe(InstigatorOb);

			StatusCategory.Add(Action->GetName(), FString::Printf(TEXT("%s, Instigator:%s"), *Action->GetStateDescription(), *InstigatorDesc));
			Action = Action->GetParentAction();
		}

		Snapshot->Status.Add(StatusCategory);
	}
}
#endif // ENABLE_VISUAL_LOG

FString UPawnActionsComponent::DescribeEventType(EPawnActionEventType::Type EventType)
{
	return GetEventName(EventType);
}
