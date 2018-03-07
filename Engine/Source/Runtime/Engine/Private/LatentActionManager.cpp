// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/LatentActionManager.h"
#include "UObject/Class.h"
#include "LatentActions.h"

/////////////////////////////////////////////////////
// FPendingLatentAction

#if WITH_EDITOR
FString FPendingLatentAction::GetDescription() const
{
	return TEXT("Not implemented");
}
#endif

/////////////////////////////////////////////////////
// FLatentActionManager

void FLatentActionManager::AddNewAction(UObject* InActionObject, int32 UUID, FPendingLatentAction* NewAction)
{
	auto& ObjectActionListPtr = ObjectToActionListMap.FindOrAdd(InActionObject);
	if (!ObjectActionListPtr.Get())
	{
		ObjectActionListPtr = MakeShareable(new FActionList());
	}
	ObjectActionListPtr->Add(UUID, NewAction);
}

void FLatentActionManager::RemoveActionsForObject(TWeakObjectPtr<UObject> InObject)
{
	auto ObjectActionList = GetActionListForObject(InObject);
	if (ObjectActionList)
	{
		auto& ActionToRemoveListPtr = ActionsToRemoveMap.FindOrAdd(InObject);
		if (!ActionToRemoveListPtr.IsValid())
		{
			ActionToRemoveListPtr = MakeShareable(new TArray<FUuidAndAction>());
		}

		for (FActionList::TConstIterator It(*ObjectActionList); It; ++It)
		{
			ActionToRemoveListPtr->Add(*It);
		}
	}
}

int32 FLatentActionManager::GetNumActionsForObject(TWeakObjectPtr<UObject> InObject)
{
	auto ObjectActionList = GetActionListForObject(InObject);
	if (ObjectActionList)
	{
		return ObjectActionList->Num();
	}

	return 0;
}


DECLARE_CYCLE_STAT(TEXT("Blueprint Latent Actions"), STAT_TickLatentActions, STATGROUP_Game);

void FLatentActionManager::ProcessLatentActions(UObject* InObject, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickLatentActions);

	for (FActionsForObject::TIterator It(ActionsToRemoveMap); It; ++It)
	{
		auto ActionList = GetActionListForObject(It.Key());
		auto ActionToRemoveListPtr = It.Value();
		if (ActionToRemoveListPtr.IsValid() && ActionList)
		{
			for (auto PendingActionToKill : *ActionToRemoveListPtr)
			{
				const int32 RemovedNum = ActionList->RemoveSingle(PendingActionToKill.Key, PendingActionToKill.Value);
				auto Action = PendingActionToKill.Value;
				if (RemovedNum && Action)
				{
					Action->NotifyActionAborted();
					delete Action;
				}
			}
		}
	}
	ActionsToRemoveMap.Reset();

	//@TODO: K2: Very inefficient code right now
	if (InObject != NULL)
	{
		if (!ProcessedThisFrame.Contains(InObject))
		{
			if (FActionList* ObjectActionList = GetActionListForObject(InObject))
			{
				TickLatentActionForObject(DeltaTime, *ObjectActionList, InObject);
				ProcessedThisFrame.Add(InObject);
			}
		}
	}
	else 
	{
		for (FObjectToActionListMap::TIterator ObjIt(ObjectToActionListMap); ObjIt; ++ObjIt)
		{	
			TWeakObjectPtr<UObject> WeakPtr = ObjIt.Key();
			UObject* Object = WeakPtr.Get();
			auto ObjectActionListPtr = ObjIt.Value().Get();
			check(ObjectActionListPtr);
			FActionList& ObjectActionList = *ObjectActionListPtr;

			if (Object != NULL)
			{
				// Tick all outstanding actions for this object
				if (ObjectActionList.Num() > 0)
				{
					if (!ProcessedThisFrame.Contains(Object))
					{
						TickLatentActionForObject(DeltaTime, ObjectActionList, Object);
						ensure(ObjectActionListPtr == ObjIt.Value().Get());
						ProcessedThisFrame.Add(Object);
					}
				}
			}
			else
			{
				// Terminate all outstanding actions for this object, which has been GCed
				for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
				{
					FPendingLatentAction* Action = It.Value();
					Action->NotifyObjectDestroyed();
					delete Action;
				}
				ObjectActionList.Reset();
			}

			// Remove the entry if there are no pending actions remaining for this object (or if the object was NULLed and cleaned up)
			if (ObjectActionList.Num() == 0)
			{
				ObjIt.RemoveCurrent();
			}
		}
	}
}

void FLatentActionManager::TickLatentActionForObject(float DeltaTime, FActionList& ObjectActionList, UObject* InObject)
{
	typedef TPair<int32, FPendingLatentAction*> FActionListPair;
	TArray<FActionListPair, TInlineAllocator<4>> ItemsToRemove;
	
	FLatentResponse Response(DeltaTime);
	for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
	{
		FPendingLatentAction* Action = It.Value();

		Response.bRemoveAction = false;

		Action->UpdateOperation(Response);

		if (Response.bRemoveAction)
		{
			ItemsToRemove.Emplace(It.Key(), Action);
		}
	}

	// Remove any items that were deleted
	for (int32 i = 0; i < ItemsToRemove.Num(); ++i)
	{
		const FActionListPair& ItemPair = ItemsToRemove[i];
		const int32 ItemIndex = ItemPair.Key;
		FPendingLatentAction* DyingAction = ItemPair.Value;
		ObjectActionList.Remove(ItemIndex, DyingAction);
		delete DyingAction;
	}

	// Trigger any pending execution links
	for (int32 i = 0; i < Response.LinksToExecute.Num(); ++i)
	{
		FLatentResponse::FExecutionInfo& LinkInfo = Response.LinksToExecute[i];
		if (LinkInfo.LinkID != INDEX_NONE)
		{
			if (UObject* CallbackTarget = LinkInfo.CallbackTarget.Get())
			{
				check(CallbackTarget == InObject);

				if (UFunction* ExecutionFunction = CallbackTarget->FindFunction(LinkInfo.ExecutionFunction))
				{
					CallbackTarget->ProcessEvent(ExecutionFunction, &(LinkInfo.LinkID));
				}
				else
				{
					UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: Could not find latent action resume point named '%s' on '%s' called by '%s'"),
						*LinkInfo.ExecutionFunction.ToString(), *(CallbackTarget->GetPathName()), *(InObject->GetPathName()));
				}
			}
			else
			{
				UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: CallbackTarget is None."));
			}
		}
	}
}

#if WITH_EDITOR


FString FLatentActionManager::GetDescription(UObject* InObject, int32 UUID) const
{
	check(InObject);

	FString Description = *NSLOCTEXT("LatentActionManager", "NoPendingActions", "No Pending Actions").ToString();

	const FLatentActionManager::FActionList* ObjectActionList = GetActionListForObject(InObject);
	if ((ObjectActionList != NULL) && (ObjectActionList->Num() > 0))
	{	
		TArray<FPendingLatentAction*> Actions;
		ObjectActionList->MultiFind(UUID, Actions);

		const int32 PendingActions = Actions.Num();

		// See if there are pending actions
		if (PendingActions > 0)
		{
			FPendingLatentAction* PrimaryAction = Actions[0];
			FString ActionDesc = PrimaryAction->GetDescription();

			Description = (PendingActions > 1)
				? FText::Format(NSLOCTEXT("LatentActionManager", "NumPendingActionsFwd", "{0} Pending Actions: {1}"), PendingActions, FText::FromString(ActionDesc)).ToString()
				: ActionDesc;
		}
	}
	return Description;
}

void FLatentActionManager::GetActiveUUIDs(UObject* InObject, TSet<int32>& UUIDList) const
{
	check(InObject);

	const FLatentActionManager::FActionList* ObjectActionList = GetActionListForObject(InObject);
	if ((ObjectActionList != NULL) && (ObjectActionList->Num() > 0))
	{
		for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(*ObjectActionList); It; ++It)
		{
			UUIDList.Add(It.Key());
		}
	}
}

#endif

FLatentActionManager::~FLatentActionManager()
{
	for (auto& ObjectActionListIterator : ObjectToActionListMap)
	{
		TSharedPtr<FActionList>& ActionList = ObjectActionListIterator.Value;
		if (ActionList.IsValid())
		{
			for (auto& ActionIterator : *ActionList.Get())
			{
				FPendingLatentAction* Action = ActionIterator.Value;
				ActionIterator.Value = nullptr;
				delete Action;
			}
			ActionList->Reset();
		}
	}
}
