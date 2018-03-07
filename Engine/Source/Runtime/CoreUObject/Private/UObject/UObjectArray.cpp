// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjArray.cpp: Unreal array of all objects
=============================================================================*/

#include "UObject/UObjectArray.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectArray, Log, All);

FUObjectClusterContainer GUObjectClusters;

FUObjectArray::FUObjectArray()
: ObjFirstGCIndex(0)
, ObjLastNonGCIndex(INDEX_NONE)
, MaxObjectsNotConsideredByGC(0)
, OpenForDisregardForGC(!HACK_HEADER_GENERATOR)
, MasterSerialNumber(START_SERIAL_NUMBER)
{
	GCoreObjectArrayForDebugVisualizers = &GUObjectArray.ObjObjects;
}

void FUObjectArray::AllocateObjectPool(int32 InMaxUObjects, int32 InMaxObjectsNotConsideredByGC)
{
	check(IsInGameThread());

	MaxObjectsNotConsideredByGC = InMaxObjectsNotConsideredByGC;

	// GObjFirstGCIndex is the index at which the garbage collector will start for the mark phase.
	// If disregard for GC is enabled this will be set to an invalid value so that later we
	// know if disregard for GC pool has already been closed (at least once)
	ObjFirstGCIndex = DisregardForGCEnabled() ? -1 : 0;

	// Pre-size array.
	check(ObjObjects.Num() == 0);
	UE_CLOG(InMaxUObjects <= 0, LogUObjectArray, Fatal, TEXT("Max UObject count is invalid. It must be a number that is greater than 0."));
	ObjObjects.PreAllocate(InMaxUObjects);

	if (MaxObjectsNotConsideredByGC > 0)
	{
		ObjObjects.AddRange(MaxObjectsNotConsideredByGC);
	}
}

void FUObjectArray::OpenDisregardForGC()
{
	check(IsInGameThread());
	check(!OpenForDisregardForGC);
	OpenForDisregardForGC = true;
	UE_LOG(LogUObjectArray, Log, TEXT("OpenDisregardForGC: %d/%d objects in disregard for GC pool"), ObjLastNonGCIndex + 1, MaxObjectsNotConsideredByGC);
}

void FUObjectArray::CloseDisregardForGC()
{
#if THREADSAFE_UOBJECTS
	FScopeLock ObjObjectsLock(&ObjObjectsCritical);
#else
	// Disregard from GC pool is only available from the game thread, at least for now
	check(IsInGameThread());
#endif

	check(OpenForDisregardForGC);

	UClass::AssembleReferenceTokenStreams();

	if (GIsInitialLoad)
	{
		// Iterate over all objects and mark them to be part of root set.
		int32 NumAlwaysLoadedObjects = 0;
		int32 NumRootObjects = 0;
		for (FObjectIterator It; It; ++It)
		{
			UObject* Object = *It;
			if (Object->IsSafeForRootSet())
			{
				NumRootObjects++;
				Object->AddToRoot();
			}
			else if (Object->IsRooted())
			{
				Object->RemoveFromRoot();
			}
			NumAlwaysLoadedObjects++;
		}

		UE_LOG(LogUObjectArray, Log, TEXT("%i objects as part of root set at end of initial load."), NumAlwaysLoadedObjects);
		if (GUObjectArray.DisregardForGCEnabled())
		{
			UE_LOG(LogUObjectArray, Log, TEXT("%i objects are not in the root set, but can never be destroyed because they are in the DisregardForGC set."), NumAlwaysLoadedObjects - NumRootObjects);
		}

		// When disregard for GC pool is closed for the first time, make sure the first GC index is set after the last non-GC index.
		// We do allow here for some slack if MaxObjectsNotConsideredByGC > (ObjLastNonGCIndex + 1) so that disregard for GC pool
		// can be re-opened later.
		ObjFirstGCIndex = FMath::Max(ObjFirstGCIndex, ObjLastNonGCIndex + 1);

		GUObjectAllocator.BootMessage();
	}

	UE_LOG(LogUObjectArray, Log, TEXT("CloseDisregardForGC: %d/%d objects in disregard for GC pool"), ObjLastNonGCIndex + 1, MaxObjectsNotConsideredByGC);	

	OpenForDisregardForGC = false;
	GIsInitialLoad = false;
}

void FUObjectArray::DisableDisregardForGC()
{
	MaxObjectsNotConsideredByGC = 0;
	ObjFirstGCIndex = 0;
	if (IsOpenForDisregardForGC())
	{
		CloseDisregardForGC();
	}
}

void FUObjectArray::AllocateUObjectIndex(UObjectBase* Object, bool bMergingThreads /*= false*/)
{
	int32 Index = INDEX_NONE;
	check(Object->InternalIndex == INDEX_NONE || bMergingThreads);

	// Special non- garbage collectable range.
	if (OpenForDisregardForGC && DisregardForGCEnabled())
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ObjObjectsLock(&ObjObjectsCritical);
#else
		// Disregard from GC pool is only available from the game thread, at least for now
		check(IsInGameThread());
#endif

		Index = ++ObjLastNonGCIndex;
		// Check if we're not out of bounds, unless there hasn't been any gc objects yet
		UE_CLOG(ObjLastNonGCIndex >= MaxObjectsNotConsideredByGC && ObjFirstGCIndex >= 0, LogUObjectArray, Fatal, TEXT("Unable to add more objects to disregard for GC pool (Max: %d)"), MaxObjectsNotConsideredByGC);
		// If we haven't added any GC objects yet, it's fine to keep growing the disregard pool past its initial size.
		if (ObjLastNonGCIndex >= MaxObjectsNotConsideredByGC)
		{
			Index = ObjObjects.AddSingle();
			check(Index == ObjLastNonGCIndex);
		}
		MaxObjectsNotConsideredByGC = FMath::Max(MaxObjectsNotConsideredByGC, ObjLastNonGCIndex + 1);
	}
	// Regular pool/ range.
	else
	{
		int32* AvailableIndex = ObjAvailableList.Pop();
		if (AvailableIndex)
		{
#if UE_GC_TRACK_OBJ_AVAILABLE
			const int32 AvailableCount = ObjAvailableCount.Decrement();
			checkSlow(AvailableCount >= 0);
#endif
			Index = (int32)(uintptr_t)AvailableIndex;
			check(ObjObjects[Index].Object==nullptr);
		}
		else
		{
			// Make sure ObjFirstGCIndex is valid, otherwise we didn't close the disregard for GC set
			check(ObjFirstGCIndex >= 0);
#if THREADSAFE_UOBJECTS
			FScopeLock ObjObjectsLock(&ObjObjectsCritical);
#else
			check(IsInGameThread());
#endif
			Index = ObjObjects.AddSingle();			
		}
		check(Index >= ObjFirstGCIndex && Index > ObjLastNonGCIndex);
	}
	// Add to global table.
	if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&ObjObjects[Index].Object, Object, NULL) != NULL) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
	{
		UE_LOG(LogUObjectArray, Fatal, TEXT("Unexpected concurency while adding new object"));
	}
	IndexToObject(Index)->ResetSerialNumberAndFlags();
	Object->InternalIndex = Index;
	//  @todo: threading: lock UObjectCreateListeners
	for (int32 ListenerIndex = 0; ListenerIndex < UObjectCreateListeners.Num(); ListenerIndex++)
	{
		UObjectCreateListeners[ListenerIndex]->NotifyUObjectCreated(Object,Index);
	}
}

/**
 * Returns a UObject index to the global uobject array
 *
 * @param Object object to free
 */
void FUObjectArray::FreeUObjectIndex(UObjectBase* Object)
{
	// This should only be happening on the game thread (GC runs only on game thread when it's freeing objects)
	check(IsInGameThread());

	int32 Index = Object->InternalIndex;
	// At this point no two objects exist with the same index so no need to lock here
	if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&ObjObjects[Index].Object, NULL, Object) == NULL) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
	{
		UE_LOG(LogUObjectArray, Fatal, TEXT("Unexpected concurency while adding new object"));
	}

	// @todo: threading: delete listeners should be locked while we're doing this
	// Iterate in reverse order so that when one of the listeners removes itself from the array inside of NotifyUObjectDeleted we don't skip the next listener.
	for (int32 ListenerIndex = UObjectDeleteListeners.Num() - 1; ListenerIndex >= 0; --ListenerIndex)
	{
		UObjectDeleteListeners[ListenerIndex]->NotifyUObjectDeleted(Object, Index);
	}
	// You cannot safely recycle indicies in the non-GC range
	// No point in filling this list when doing exit purge. Nothing should be allocated afterwards anyway.
	if (Index > ObjLastNonGCIndex && !GExitPurge)  
	{
		IndexToObject(Index)->ResetSerialNumberAndFlags();
		ObjAvailableList.Push((int32*)(uintptr_t)Index);
#if UE_GC_TRACK_OBJ_AVAILABLE
		ObjAvailableCount.Increment();
#endif
	}
}

/**
 * Adds a creation listener
 *
 * @param Listener listener to notify when an object is deleted
 */
void FUObjectArray::AddUObjectCreateListener(FUObjectCreateListener* Listener)
{
	check(!UObjectCreateListeners.Contains(Listener));
	UObjectCreateListeners.Add(Listener);
}

/**
 * Removes a listener for object creation
 *
 * @param Listener listener to remove
 */
void FUObjectArray::RemoveUObjectCreateListener(FUObjectCreateListener* Listener)
{
	int32 NumRemoved = UObjectCreateListeners.RemoveSingleSwap(Listener);
	check(NumRemoved==1);
}

/**
 * Checks whether object is part of permanent object pool.
 *
 * @param Listener listener to notify when an object is deleted
 */
void FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener* Listener)
{
#if THREADSAFE_UOBJECTS
	FScopeLock UObjectDeleteListenersLock(&UObjectDeleteListenersCritical);
#endif
	check(!UObjectDeleteListeners.Contains(Listener));
	UObjectDeleteListeners.Add(Listener);
}

/**
 * removes a listener for object deletion
 *
 * @param Listener listener to remove
 */
void FUObjectArray::RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener)
{
#if THREADSAFE_UOBJECTS
	FScopeLock UObjectDeleteListenersLock(&UObjectDeleteListenersCritical);
#endif
	UObjectDeleteListeners.RemoveSingleSwap(Listener);
}



/**
 * Checks if a UObject index is valid
 *
 * @param	Object object to test for validity
 * @return	true if this index is valid
 */
bool FUObjectArray::IsValid(const UObjectBase* Object) const 
{ 
	int32 Index = Object->InternalIndex;
	if( Index == INDEX_NONE )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Object is not in global object array") );
		return false;
	}
	if( !ObjObjects.IsValidIndex(Index))
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Invalid object index %i"), Index );
		return false;
	}
	const FUObjectItem& Slot = ObjObjects[Index];
	if( Slot.Object == NULL )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Empty slot") );
		return false;
	}
	if( Slot.Object != Object )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Other object in slot") );
		return false;
	}
	return true;
}

int32 FUObjectArray::AllocateSerialNumber(int32 Index)
{
	FUObjectItem* ObjectItem = IndexToObject(Index);
	checkSlow(ObjectItem);

	volatile int32 *SerialNumberPtr = &ObjectItem->SerialNumber;
	int32 SerialNumber = *SerialNumberPtr;
	if (!SerialNumber)
	{
		SerialNumber = MasterSerialNumber.Increment();
		UE_CLOG(SerialNumber <= START_SERIAL_NUMBER, LogUObjectArray, Fatal, TEXT("UObject serial numbers overflowed (trying to allocate serial number %d)."), SerialNumber);
		int32 ValueWas = FPlatformAtomics::InterlockedCompareExchange((int32*)SerialNumberPtr, SerialNumber, 0);
		if (ValueWas != 0)
		{
			// someone else go it first, use their value
			SerialNumber = ValueWas;
		}
	}
	checkSlow(SerialNumber > START_SERIAL_NUMBER);
	return SerialNumber;
}

/**
 * Clears some internal arrays to get rid of false memory leaks
 */
void FUObjectArray::ShutdownUObjectArray()
{
}
