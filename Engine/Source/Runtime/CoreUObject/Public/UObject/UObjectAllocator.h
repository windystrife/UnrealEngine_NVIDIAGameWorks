// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjAllocator.h: Unreal object allocation
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

class COREUOBJECT_API FUObjectAllocator
{
public:

	/**
	 * Constructor, initializes to no permanent object pool
	 */
	FUObjectAllocator() :
	  PermanentObjectPoolSize(0),
	  PermanentObjectPool(NULL),
	  PermanentObjectPoolTail(NULL),
		PermanentObjectPoolExceededTail(NULL)
	{
	}

	/**
	 * Allocates and initializes the permanent object pool
	 *
	 * @param InPermanentObjectPoolSize size of permanent object pool
	 */
	void AllocatePermanentObjectPool(int32 InPermanentObjectPoolSize);

	/**
	 * Prints a debugf message to allow tuning
	 */
	void BootMessage();

	/**
	 * Checks whether object is part of permanent object pool.
	 *
	 * @param Object object to test as a member of permanent object pool
	 * @return true if object is part of permanent object pool, false otherwise
	 */
	FORCEINLINE bool ResidesInPermanentPool(const UObjectBase *Object) const
	{
		return ((const uint8*)Object >= PermanentObjectPool) && ((const uint8*)Object < PermanentObjectPoolTail);
	}

	/**
	 * Allocates a UObjectBase from the free store or the permanent object pool
	 *
	 * @param Size size of uobject to allocate
	 * @param Alignment alignment of uobject to allocate
	 * @param bAllowPermanent if true, allow allocation in the permanent object pool, if it fits
	 * @return newly allocated UObjectBase (not really a UObjectBase yet, no constructor like thing has been called).
	 */
	UObjectBase* AllocateUObject(int32 Size, int32 Alignment, bool bAllowPermanent);

	/**
	 * Returns a UObjectBase to the free store, unless it is in the permanent object pool
	 *
	 * @param Object object to free
	 */
	void FreeUObject(UObjectBase *Object) const;

private:

	/** Size in bytes of pool for objects disregarded for GC.								*/
	int32							PermanentObjectPoolSize;
	/** Begin of pool for objects disregarded for GC.										*/
	uint8*						PermanentObjectPool;
	/** Current position in pool for objects disregarded for GC.							*/
	uint8*						PermanentObjectPoolTail;
	/** Tail that exceeded the size of the permanent object pool, >= PermanentObjectPoolTail.		*/
	uint8*						PermanentObjectPoolExceededTail;
};

/** Global UObjectBase allocator							*/
extern COREUOBJECT_API FUObjectAllocator GUObjectAllocator;

