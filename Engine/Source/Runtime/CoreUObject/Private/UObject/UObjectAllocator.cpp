// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjAllocator.cpp: Unreal object allocation
=============================================================================*/

#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectAllocator, Log, All);

/** Global UObjectBase allocator							*/
COREUOBJECT_API FUObjectAllocator GUObjectAllocator;

/**
 * Allocates and initializes the permanent object pool
 *
 * @param InPermanentObjectPoolSize size of permanent object pool
 */
void FUObjectAllocator::AllocatePermanentObjectPool(int32 InPermanentObjectPoolSize)
{
	PermanentObjectPoolSize	= InPermanentObjectPoolSize;
	PermanentObjectPool		= (uint8*) FMemory::Malloc( PermanentObjectPoolSize );
	PermanentObjectPoolTail	= PermanentObjectPool;
	PermanentObjectPoolExceededTail = PermanentObjectPoolTail;
}


/**
 * Prints a debugf message to allow tuning
 */
void FUObjectAllocator::BootMessage()
{
	if (PermanentObjectPoolSize && PermanentObjectPoolExceededTail - PermanentObjectPool > PermanentObjectPoolSize)
	{
		UE_LOG(LogUObjectAllocator, Warning, TEXT("%i Exceeds size of permanent object pool %i, please tune SizeOfPermanentObjectPool."), PermanentObjectPoolExceededTail - PermanentObjectPool, PermanentObjectPoolSize );
	}
	else
	{
		UE_LOG(LogUObjectAllocator, Log, TEXT("%i out of %i bytes used by permanent object pool."), PermanentObjectPoolExceededTail - PermanentObjectPool, PermanentObjectPoolSize );
	}
}

/**
 * Allocates a UObjectBase from the free store or the permanent object pool
 *
 * @param Size size of uobject to allocate
 * @param Alignment alignment of uobject to allocate
 * @param bAllowPermanent if true, allow allocation in the permanent object pool, if it fits
 * @return newly allocated UObjectBase (not really a UObjectBase yet, no constructor like thing has been called).
 */
UObjectBase* FUObjectAllocator::AllocateUObject(int32 Size, int32 Alignment, bool bAllowPermanent)
{
	// Force alignment to 16 bytes
	Alignment = 16;
	int32 AlignedSize = Align( Size, Alignment );
	UObjectBase* Result = NULL;

	const bool bPlaceInPerm = bAllowPermanent && (Align(PermanentObjectPoolTail,Alignment) + Size) <= (PermanentObjectPool + PermanentObjectPoolSize);
	if (bAllowPermanent && !bPlaceInPerm)
	{
		// advance anyway so we can determine how much space we should set aside in the ini
		uint8* AlignedPtr = Align( PermanentObjectPoolExceededTail, Alignment );
		PermanentObjectPoolExceededTail = AlignedPtr + Size;
	}
	// Use object memory pool for objects disregarded by GC (initially loaded ones). This allows identifying their
	// GC status by simply looking at their address.
	if (bPlaceInPerm)
	{
		// Align current tail pointer and use it for object. 
		uint8* AlignedPtr = Align( PermanentObjectPoolTail, Alignment );
		// Update tail pointer.
		PermanentObjectPoolTail = AlignedPtr + Size;
		Result = (UObjectBase*)AlignedPtr;
		if (PermanentObjectPoolExceededTail < PermanentObjectPoolTail)
		{
			PermanentObjectPoolExceededTail = PermanentObjectPoolTail;
		}
	}
	else
	{
		// Allocate new memory of the appropriate size and alignment.
		Result = (UObjectBase*)FMemory::Malloc( AlignedSize );
	}
	return Result;
}

/**
 * Returns a UObjectBase to the free store, unless it is in the permanent object pool
 *
 * @param Object object to free
 */
void FUObjectAllocator::FreeUObject(UObjectBase *Object) const
{
	check(!IsLoading());
	check(Object);
	// Only free memory if it was allocated directly from allocator and not from permanent object pool.
	if( ResidesInPermanentPool(Object) == false )
	{
		FMemory::Free(Object);
	}
	// We only destroy objects residing in permanent object pool during the exit purge.
	else
	{
		check(GExitPurge);
	}
}


