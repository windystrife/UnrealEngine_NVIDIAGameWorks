// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"

/*-----------------------------------------------------------------------------
	MRU array.
-----------------------------------------------------------------------------*/

/**
 * Same as TArray except:
 * - Has an upper limit of the number of items it will store.
 * - Any item that is added to the array is moved to the top.
 */
template<typename T, typename Allocator = FDefaultAllocator>
class TMRUArray
	: public TArray<T, Allocator>
{
public:
	typedef TArray<T, Allocator> Super;

	/** The maximum number of items we can store in this array. */
	int32 MaxItems;

	/**
	 * Constructor.
	 */
	TMRUArray()
		: Super()
	{
		MaxItems = 0;
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TMRUArray(TMRUArray&&) = default;
	TMRUArray(const TMRUArray&) = default;
	TMRUArray& operator=(TMRUArray&&) = default;
	TMRUArray& operator=(const TMRUArray&) = default;

#else

	/**
	 * Copy constructor.
	 *
	 * @param Other Other array to copy.
	 */
	FORCEINLINE TMRUArray(const TMRUArray& Other)
		: Super((const Super&)Other)
	{
	}

	/**
	 * Assign operator.
	 *
	 * @param Other Other array to assign with.
	 * @returns Reference to this object.
	 */
	FORCEINLINE TMRUArray& operator=(const TMRUArray& Other)
	{
		(Super&)*this = (const Super&)Other;
		return *this;
	}

	/**
	 * Move constructor.
	 *
	 * @param Other Other array to move.
	 */
	FORCEINLINE TMRUArray(TMRUArray&& Other)
		: Super((TMRUArray&&)Other)
	{
	}

	/**
	 * Move assign operator.
	 *
	 * @param Other Other array to assign with.
	 * @returns Reference to this object.
	 */
	FORCEINLINE TMRUArray& operator=(TMRUArray&& Other)
	{
		(Super&)*this = (Super&&)Other;
		return *this;
	}

#endif
	/**
	 * Adds item to the array. Makes sure that we don't add more than the
	 * limit.
	 *
	 * @param Item Item to add.
	 * @returns Always 0.
	 */
	int32 Add(const T& Item)
	{
		const int32 idx = Super::Add(Item);
		this->Swap(idx, 0);
		CullArray();
		return 0;
	}

	/**
	 * Adds a number of zeroed elements to the array. Makes sure that we don't
	 * add more than the limit.
	 *
	 * @param Count (Optional) A number of elements to add. Default is 0.
	 * @returns Always 0.
	 */
	int32 AddZeroed(int32 Count = 1)
	{
		const int32 idx = Super::AddZeroed(Count);
		this->Swap(idx, 0);
		CullArray();
		return 0;
	}

	/**
	 * Adds unique item to the array. Makes sure that we don't add more than
	 * the limit. If the item existed it will be removed before addition.
	 *
	 * @param Item Element to add.
	 * @returns Always 0.
	 */
	int32 AddUnique(const T& Item)
	{
		// Remove any existing copies of the item.
		this->Remove(Item);

		this->InsertUninitialized(0);
		(*this)[0] = Item;

		CullArray();

		return 0;
	}

	/**
	 * Makes sure that the array never gets beyond MaxItems in size.
	 */
	void CullArray()
	{
		// 0 = no limit
		if (!MaxItems)
		{
			return;
		}

		while (this->Num() > MaxItems)
		{
			this->RemoveAt(this->Num() - 1, 1);
		}
	}
};


template<typename T, typename Allocator>
struct TContainerTraits<TMRUArray<T, Allocator> > : public TContainerTraitsBase<TMRUArray<T, Allocator> >
{
	enum { MoveWillEmptyContainer = TContainerTraitsBase<typename TMRUArray<T, Allocator>::Super>::MoveWillEmptyContainer };
};


