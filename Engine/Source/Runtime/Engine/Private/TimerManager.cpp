// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TickTaskManager.cpp: Manager for ticking tasks
=============================================================================*/

#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Engine/World.h"
#include "UnrealEngine.h"

DECLARE_CYCLE_STAT(TEXT("SetTimer"), STAT_SetTimer, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("ClearTimer"), STAT_ClearTimer, STATGROUP_Engine);

/** Track the last assigned handle globally */
uint64 FTimerManager::LastAssignedHandle = 0;

namespace
{
	void DescribeFTImerDataSafely(const FTimerData& Data)
	{
		UE_LOG(LogEngine, Log, TEXT("TimerData %p : bLoop=%s, bRequiresDelegate=%s, Status=%d, Rate=%f, ExpireTime=%f"),
			&Data,
			Data.bLoop ? TEXT("true") : TEXT("false"),
			Data.bRequiresDelegate ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Data.Status),
			Data.Rate,
			Data.ExpireTime
			)
	}
}

FTimerManager::FTimerManager()
	: InternalTime(0.0)
	, LastTickedFrame(static_cast<uint64>(-1))
	, OwningGameInstance(nullptr)
{
	if (IsRunningDedicatedServer())
	{
		// Off by default, renable if needed
		//FCoreDelegates::OnHandleSystemError.AddRaw(this, &FTimerManager::OnCrash);
	}
}

FTimerManager::~FTimerManager()
{
	if (IsRunningDedicatedServer())
	{
		FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	}
}

void FTimerManager::OnCrash()
{
	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p on crashing delegate called, dumping extra information"), this);

	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers -------"), ActiveTimerHeap.Num());
	for(const FTimerData& Data : ActiveTimerHeap)
	{
		DescribeFTImerDataSafely(Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerList.Num());
	for (const FTimerData& Data : PausedTimerList)
	{
		DescribeFTImerDataSafely(Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerList.Num());
	for (const FTimerData& Data : PendingTimerList)
	{
		DescribeFTImerDataSafely(Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerList.Num() + PausedTimerList.Num() + ActiveTimerHeap.Num());

	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p dump ended"), this);
}

void FTimerHandle::MakeValid()
{
	FTimerManager::ValidateHandle(*this);
}


FString FTimerUnifiedDelegate::ToString() const
{
	const UObject* Object = nullptr;
	FName FunctionName = NAME_None;
	bool bDynDelegate = false;

	if (FuncDelegate.IsBound())
	{
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
		FunctionName = FuncDelegate.TryGetBoundFunctionName();
#endif
	}
	else if (FuncDynDelegate.IsBound())
	{
		Object = FuncDynDelegate.GetUObject();
		FunctionName = FuncDynDelegate.GetFunctionName();
		bDynDelegate = true;
	}
	else
	{
		static FName NotBoundName(TEXT("NotBound!"));
		FunctionName = NotBoundName;
	}

	return FString::Printf(TEXT("%s,%s,%s"), bDynDelegate ? TEXT("DELEGATE") : TEXT("DYN DELEGATE"), Object == nullptr ? TEXT("NO OBJ") : *Object->GetPathName(), *FunctionName.ToString());
}

// ---------------------------------
// Private members
// ---------------------------------

FTimerData const* FTimerManager::FindTimer(FTimerHandle const& InHandle, int32* OutTimerIndex) const
{
	if (InHandle.IsValid())
	{
		if (CurrentlyExecutingTimer.TimerHandle == InHandle)
		{
			// found it currently executing
			if (OutTimerIndex)
			{
				*OutTimerIndex = -1;
			}
			return &CurrentlyExecutingTimer;
		}

		int32 ActiveTimerIdx = FindTimerInList(ActiveTimerHeap, InHandle);
		if (ActiveTimerIdx != INDEX_NONE)
		{
			// found it in the active heap
			if (OutTimerIndex)
			{
				*OutTimerIndex = ActiveTimerIdx;
			}
			return &ActiveTimerHeap[ActiveTimerIdx];
		}

		int32 PausedTimerIdx = FindTimerInList(PausedTimerList, InHandle);
		if (PausedTimerIdx != INDEX_NONE)
		{
			// found it in the paused list
			if (OutTimerIndex)
			{
				*OutTimerIndex = PausedTimerIdx;
			}
			return &PausedTimerList[PausedTimerIdx];
		}

		int32 PendingTimerIdx = FindTimerInList(PendingTimerList, InHandle);
		if (PendingTimerIdx != INDEX_NONE)
		{
			// found it in the pending list
			if (OutTimerIndex)
			{
				*OutTimerIndex = PendingTimerIdx;
			}
			return &PendingTimerList[PendingTimerIdx];
		}
	}

	return nullptr;
}

/** Will find the given timer in the given TArray and return its index. */
int32 FTimerManager::FindTimerInList(const TArray<FTimerData> &SearchArray, FTimerHandle const& InHandle) const
{
	if (InHandle.IsValid())
	{
		for (int32 Idx = 0; Idx < SearchArray.Num(); ++Idx)
		{
			if (SearchArray[Idx].TimerHandle == InHandle)
			{
				return Idx;
			}
		}
	}

	return INDEX_NONE;
}

/** Finds a handle to a dynamic timer bound to a particular pointer and function name. */
FTimerHandle FTimerManager::K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const
{
	if (CurrentlyExecutingTimer.TimerDelegate.FuncDynDelegate == InDynamicDelegate)
	{
		return CurrentlyExecutingTimer.TimerHandle;
	}

	const FTimerData* Timer = ActiveTimerHeap.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; });
	if (Timer)
	{
		return Timer->TimerHandle;
	}

	Timer = PausedTimerList.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; });
	if (Timer)
	{
		return Timer->TimerHandle;
	}

	Timer = PendingTimerList.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; });
	if (Timer)
	{
		return Timer->TimerHandle;
	}

	return FTimerHandle();
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	SCOPE_CYCLE_COUNTER(STAT_SetTimer);

	// not currently threadsafe
	check(IsInGameThread());

	if (InOutHandle.IsValid())
	{
		// if the timer is already set, just clear it and we'll re-add it, since 
		// there's no data to maintain.
		InternalClearTimer(InOutHandle);
	}

	if (InRate > 0.f)
	{
		ValidateHandle(InOutHandle);

		// set up the new timer
		FTimerData NewTimerData;
		NewTimerData.TimerHandle = InOutHandle;
		NewTimerData.TimerDelegate = InDelegate;

		InternalSetTimer(NewTimerData, InRate, InbLoop, InFirstDelay);
	}
}

void FTimerManager::InternalSetTimer(FTimerData& NewTimerData, float InRate, bool InbLoop, float InFirstDelay)
{
	if (NewTimerData.TimerHandle.IsValid() || NewTimerData.TimerDelegate.IsBound())
	{
		NewTimerData.Rate = InRate;
		NewTimerData.bLoop = InbLoop;
		NewTimerData.bRequiresDelegate = NewTimerData.TimerDelegate.IsBound();

		// Set level collection
		const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
		if (OwningWorld && OwningWorld->GetActiveLevelCollection())
		{
			NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
		}

		const float FirstDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;

		if (HasBeenTickedThisFrame())
		{
			NewTimerData.ExpireTime = InternalTime + FirstDelay;
			NewTimerData.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush(NewTimerData);
		}
		else
		{
			// Store time remaining in ExpireTime while pending
			NewTimerData.ExpireTime = FirstDelay;
			NewTimerData.Status = ETimerStatus::Pending;
			PendingTimerList.Add(NewTimerData);
		}
	}
}

void FTimerManager::InternalSetTimerForNextTick(FTimerUnifiedDelegate const& InDelegate)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData NewTimerData;
	NewTimerData.Rate = 0.f;
	NewTimerData.bLoop = false;
	NewTimerData.bRequiresDelegate = true;
	NewTimerData.TimerDelegate = InDelegate;
	NewTimerData.ExpireTime = InternalTime;
	NewTimerData.Status = ETimerStatus::Active;

	// Set level collection
	const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
	if (OwningWorld && OwningWorld->GetActiveLevelCollection())
	{
		NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
	}


	ActiveTimerHeap.HeapPush(NewTimerData);
}

void FTimerManager::InternalClearTimer(FTimerHandle const& InHandle)
{
	SCOPE_CYCLE_COUNTER(STAT_ClearTimer);

	// not currently threadsafe
	check(IsInGameThread());

	// Skip if the handle is invalid as it  would not be found by FindTimer and unbind the current handler if it also used INDEX_NONE. 
	if (!InHandle.IsValid())
	{
		return;
	}

	int32 TimerIdx;
	FTimerData const* const TimerData = FindTimer(InHandle, &TimerIdx);
	if (TimerData)
	{
		InternalClearTimer(TimerIdx, TimerData->Status);
	}
}

void FTimerManager::InternalClearTimer(int32 TimerIdx, ETimerStatus TimerStatus)
{
	switch (TimerStatus)
	{
		case ETimerStatus::Pending:
			PendingTimerList.RemoveAtSwap(TimerIdx, 1, /*bAllowShrinking=*/ false);
			break;

		case ETimerStatus::Active:
			ActiveTimerHeap.HeapRemoveAt(TimerIdx, /*bAllowShrinking=*/ false);
			break;

		case ETimerStatus::Paused:
			PausedTimerList.RemoveAtSwap(TimerIdx, 1, /*bAllowShrinking=*/ false);
			break;

		case ETimerStatus::Executing:
			// Edge case. We're currently handling this timer when it got cleared.  Clear it to prevent it firing again
			// in case it was scheduled to fire multiple times.
			CurrentlyExecutingTimer.Clear();
			break;

		default:
			check(false);
	}
}


void FTimerManager::InternalClearAllTimers(void const* Object)
{
	if (Object)
	{
		// search active timer heap for timers using this object and remove them
		int32 const OldActiveHeapSize = ActiveTimerHeap.Num();

		for (int32 Idx=0; Idx<ActiveTimerHeap.Num(); ++Idx)
		{
			if ( ActiveTimerHeap[Idx].TimerDelegate.IsBoundToObject(Object) )
			{
				// remove this item
				// this will break the heap property, but we will re-heapify afterward
				ActiveTimerHeap.RemoveAtSwap(Idx--, 1, /*bAllowShrinking=*/ false);
			}
		}
		if (OldActiveHeapSize != ActiveTimerHeap.Num())
		{
			// need to heapify
			ActiveTimerHeap.Heapify();
		}

		// search paused timer list for timers using this object and remove them, too
		for (int32 Idx=0; Idx<PausedTimerList.Num(); ++Idx)
		{
			if (PausedTimerList[Idx].TimerDelegate.IsBoundToObject(Object))
			{
				PausedTimerList.RemoveAtSwap(Idx--, 1, /*bAllowShrinking=*/ false);
			}
		}

		// search pending timer list for timers using this object and remove them, too
		for (int32 Idx=0; Idx<PendingTimerList.Num(); ++Idx)
		{
			if (PendingTimerList[Idx].TimerDelegate.IsBoundToObject(Object))
			{
				PendingTimerList.RemoveAtSwap(Idx--, 1, /*bAllowShrinking=*/ false);
			}
		}

		// Edge case. We're currently handling this timer when it got cleared.  Unbind it to prevent it firing again
		// in case it was scheduled to fire multiple times.
		if (CurrentlyExecutingTimer.TimerDelegate.IsBoundToObject(Object))
		{
			CurrentlyExecutingTimer.Clear();
		}
	}
}

float FTimerManager::InternalGetTimerRemaining(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
				return TimerData->ExpireTime - InternalTime;
			case ETimerStatus::Executing:
				return 0.0f;
			default:
				// ExpireTime is time remaining for paused timers
				return TimerData->ExpireTime;
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerElapsed(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
				// intentional fall through
			case ETimerStatus::Executing:
				return (TimerData->Rate - (TimerData->ExpireTime - InternalTime));
			default:
				// ExpireTime is time remaining for paused timers
				return (TimerData->Rate - TimerData->ExpireTime);
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerRate(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		return TimerData->Rate;
	}
	return -1.f;
}

void FTimerManager::InternalPauseTimer( FTimerData const* TimerToPause, int32 TimerIdx )
{
	// not currently threadsafe
	check(IsInGameThread());

	if( TimerToPause && (TimerToPause->Status != ETimerStatus::Paused) )
	{
		ETimerStatus PreviousStatus = TimerToPause->Status;

		// Don't pause the timer if it's currently executing and isn't going to loop
		if( PreviousStatus != ETimerStatus::Executing || TimerToPause->bLoop )
		{
			// Add to Paused list
			int32 NewIndex = PausedTimerList.Add(*TimerToPause);

			// Set new status
			FTimerData& NewTimer = PausedTimerList[NewIndex];
			NewTimer.Status = ETimerStatus::Paused;

			// Store time remaining in ExpireTime while paused. Don't do this if the timer is in the pending list.
			if (PreviousStatus != ETimerStatus::Pending)
			{
				NewTimer.ExpireTime = NewTimer.ExpireTime - InternalTime;
			}
		}

		// Remove from previous TArray
		switch( PreviousStatus )
		{
			case ETimerStatus::Active:
				ActiveTimerHeap.HeapRemoveAt(TimerIdx, /*bAllowShrinking=*/ false);
				break;

			case ETimerStatus::Pending:
				PendingTimerList.RemoveAtSwap(TimerIdx, 1, /*bAllowShrinking=*/ false);
				break;

			case ETimerStatus::Executing:
				CurrentlyExecutingTimer.Clear();
				break;

			default:
				check(false);
		}
	}
}

void FTimerManager::InternalUnPauseTimer(int32 PausedTimerIdx)
{
	// not currently threadsafe
	check(IsInGameThread());

	if (PausedTimerIdx != INDEX_NONE)
	{
		FTimerData& TimerToUnPause = PausedTimerList[PausedTimerIdx];
		check(TimerToUnPause.Status == ETimerStatus::Paused);

		// Move it out of paused list and into proper TArray
		if( HasBeenTickedThisFrame() )
		{
			// Convert from time remaining back to a valid ExpireTime
			TimerToUnPause.ExpireTime += InternalTime;
			TimerToUnPause.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush(TimerToUnPause);
		}
		else
		{
			TimerToUnPause.Status = ETimerStatus::Pending;
			PendingTimerList.Add(TimerToUnPause);
		}

		// remove from paused list
		PausedTimerList.RemoveAtSwap(PausedTimerIdx, 1, /*bAllowShrinking=*/ false);
	}
}

// ---------------------------------
// Public members
// ---------------------------------

DECLARE_DWORD_COUNTER_STAT(TEXT("TimerManager Heap Size"),STAT_NumHeapEntries,STATGROUP_Game);

void FTimerManager::Tick(float DeltaTime)
{
	// @todo, might need to handle long-running case
	// (e.g. every X seconds, renormalize to InternalTime = 0)

	INC_DWORD_STAT_BY(STAT_NumHeapEntries, ActiveTimerHeap.Num());

	if (HasBeenTickedThisFrame())
	{
		return;
	}

	InternalTime += DeltaTime;

	while (ActiveTimerHeap.Num() > 0)
	{
		FTimerData& Top = ActiveTimerHeap.HeapTop();
		if (InternalTime > Top.ExpireTime)
		{
			// Timer has expired! Fire the delegate, then handle potential looping.

			// Set the relevant level context for this timer
			int32 LevelCollectionIndex = INDEX_NONE;
			UWorld* LevelCollectionWorld = nullptr;
			UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
			if (OwningGameInstance && OwningWorld)
			{
				LevelCollectionIndex = OwningWorld->FindCollectionIndexByType(Top.LevelCollection);
				LevelCollectionWorld = OwningWorld;
			}

			FScopedLevelCollectionContextSwitch LevelContext(LevelCollectionIndex, LevelCollectionWorld);

			// Remove it from the heap and store it while we're executing
			ActiveTimerHeap.HeapPop(CurrentlyExecutingTimer, /*bAllowShrinking=*/ false);
			CurrentlyExecutingTimer.Status = ETimerStatus::Executing;

			// Determine how many times the timer may have elapsed (e.g. for large DeltaTime on a short looping timer)
			int32 const CallCount = CurrentlyExecutingTimer.bLoop ? 
				FMath::TruncToInt( (InternalTime - CurrentlyExecutingTimer.ExpireTime) / CurrentlyExecutingTimer.Rate ) + 1
				: 1;

			// Now call the function
			for (int32 CallIdx=0; CallIdx<CallCount; ++CallIdx)
			{ 
				CurrentlyExecutingTimer.TimerDelegate.Execute();

				// If timer was cleared in the delegate execution, don't execute further 
				if( CurrentlyExecutingTimer.Status != ETimerStatus::Executing )
				{
					break;
				}
			}

			// Status test needed to ensure it didn't get cleared during execution
			if( CurrentlyExecutingTimer.bLoop && CurrentlyExecutingTimer.Status == ETimerStatus::Executing )
			{
				// if timer requires a delegate, make sure it's still validly bound (i.e. the delegate's object didn't get deleted or something)
				if (!CurrentlyExecutingTimer.bRequiresDelegate || CurrentlyExecutingTimer.TimerDelegate.IsBound())
				{
					// Put this timer back on the heap
					CurrentlyExecutingTimer.ExpireTime += CallCount * CurrentlyExecutingTimer.Rate;
					CurrentlyExecutingTimer.Status = ETimerStatus::Active;
					ActiveTimerHeap.HeapPush(CurrentlyExecutingTimer);
				}
			}

			CurrentlyExecutingTimer.Clear();
		}
		else
		{
			// no need to go further down the heap, we can be finished
			break;
		}
	}

	// Timer has been ticked.
	LastTickedFrame = GFrameCounter;

	// If we have any Pending Timers, add them to the Active Queue.
	if( PendingTimerList.Num() > 0 )
	{
		for(int32 Index=0; Index<PendingTimerList.Num(); Index++)
		{
			FTimerData& TimerToActivate = PendingTimerList[Index];
			// Convert from time remaining back to a valid ExpireTime
			TimerToActivate.ExpireTime += InternalTime;
			TimerToActivate.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush( TimerToActivate );
		}
		PendingTimerList.Reset();
	}
}

TStatId FTimerManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTimerManager, STATGROUP_Tickables);
}

void FTimerManager::ListTimers() const
{
	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers -------"), ActiveTimerHeap.Num());
	for(const FTimerData& Data : ActiveTimerHeap)
	{
		FString TimerString = Data.TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerList.Num());
	for (const FTimerData& Data : PausedTimerList)
	{
		FString TimerString = Data.TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerList.Num());
	for (const FTimerData& Data : PendingTimerList)
	{
		FString TimerString = Data.TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerList.Num() + PausedTimerList.Num() + ActiveTimerHeap.Num());
}

void FTimerManager::ValidateHandle(FTimerHandle& InOutHandle)
{
	if (!InOutHandle.IsValid())
	{
		++LastAssignedHandle;
		InOutHandle.Handle = LastAssignedHandle;
	}

	checkf(InOutHandle.IsValid(), TEXT("Timer handle has wrapped around to 0!"));
}

// Handler for ListTimers console command
static void OnListTimers(UWorld* World)
{
	if(World != nullptr)
	{
		World->GetTimerManager().ListTimers();
	}
}

// Register ListTimers console command, needs a World context
FAutoConsoleCommandWithWorld ListTimersConsoleCommand(
	TEXT("ListTimers"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListTimers)
	);
