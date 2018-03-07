// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Delegates/IDelegateInstance.h"
#include "Delegates/DelegateBase.h"

/**
 * Abstract base class for multicast delegates.
 */
typedef TArray<FDelegateBase, TInlineAllocator<1> > TInvocationList;

template<typename ObjectPtrType>
class FMulticastDelegateBase
{
public:

	~FMulticastDelegateBase()
	{
	}

	/** Removes all functions from this delegate's invocation list. */
	void Clear( )
	{
		for (FDelegateBase& DelegateBaseRef : InvocationList)
		{
			DelegateBaseRef.Unbind();
		}

		CompactInvocationList(false);
	}

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate.
	 *
	 * @return true if any functions are bound, false otherwise.
	 */
	inline bool IsBound( ) const
	{
		for (const FDelegateBase& DelegateBaseRef : InvocationList)
		{
			if (DelegateBaseRef.GetDelegateInstanceProtected())
			{
				return true;
			}
		}
		return false;
	}

	/** 
	 * Checks to see if any functions are bound to the given user object.
	 *
	 * @return	True if any functions are bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		for (const FDelegateBase& DelegateBaseRef : InvocationList)
		{
			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Removes all functions from this multi-cast delegate's invocation list that are bound to the specified UserObject.
	 * Note that the order of the delegates may not be preserved!
	 *
	 * @param InUserObject The object to remove all delegates for.
	 */
	void RemoveAll( const void* InUserObject )
	{
		if (InvocationListLockCount > 0)
		{
			bool NeedsCompacted = false;
			for (FDelegateBase& DelegateBaseRef : InvocationList)
			{
				IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
				if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
				{
					// Manually unbind the delegate here so the compaction will find and remove it.
					DelegateBaseRef.Unbind();
					NeedsCompacted = true;
				}
			}

			// can't compact at the moment, but set out threshold to zero so the next add will do it
			if (NeedsCompacted)
			{
				CompactionThreshold = 0;
			}
		}
		else
		{
			// compact us while shuffling in later delegates to fill holes
			for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num();)
			{
				FDelegateBase& DelegateBaseRef = InvocationList[InvocationListIndex];

				IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
				if (DelegateInstance == nullptr
					|| DelegateInstance->HasSameObject(InUserObject)
					|| DelegateInstance->IsCompactable())
				{
					InvocationList.RemoveAtSwap(InvocationListIndex, 1, false);
				}
				else
				{
					InvocationListIndex++;
				}
			}

			CompactionThreshold = FMath::Max(2, 2 * InvocationList.Num());

			InvocationList.Shrink();
		}
	}

protected:

	/** Hidden default constructor. */
	inline FMulticastDelegateBase( )
		: CompactionThreshold(2)
		, InvocationListLockCount(0)
	{ }

protected:

	/**
	 * Adds the given delegate instance to the invocation list.
	 *
	 * @param NewDelegateBaseRef The delegate instance to add.
	 */
	inline FDelegateHandle AddInternal(FDelegateBase&& NewDelegateBaseRef)
	{
		// compact but obey threshold of when this will trigger
		CompactInvocationList(true);
		FDelegateHandle Result = NewDelegateBaseRef.GetHandle();
		InvocationList.Add(MoveTemp(NewDelegateBaseRef));
		return Result;
	}

	/**
	 * Removes any expired or deleted functions from the invocation list.
	 *
	 * @see RequestCompaction
	 */
	void CompactInvocationList(bool CheckThreshold=false)
	{
		// if locked and no object, just return
		if (InvocationListLockCount > 0)
		{
			return;
		}

		// if checking threshold, obey but decay. This is to ensure that even infrequently called delegates will
		// eventually compact during an Add()
		if (CheckThreshold 	&& --CompactionThreshold > InvocationList.Num())
		{
			return;
		}

		int32 OldNumItems = InvocationList.Num();

		// Find anything null or compactable and remove it
		for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num();)
		{
			FDelegateBase& DelegateBaseRef = InvocationList[InvocationListIndex];

			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if (DelegateInstance == nullptr	|| DelegateInstance->IsCompactable())
			{
				InvocationList.RemoveAtSwap(InvocationListIndex);
			}
			else
			{
				InvocationListIndex++;
			}
		}

		CompactionThreshold = FMath::Max(2, 2 * InvocationList.Num());

		if (OldNumItems > CompactionThreshold)
		{
			// would be nice to shrink down to threshold, but reserve only grows..?
			InvocationList.Shrink();
		}
	}

	/**
	 * Gets a read-only reference to the invocation list.
	 *
	 * @return The invocation list.
	 */
	inline const TInvocationList& GetInvocationList( ) const
	{
		return InvocationList;
	}

	/** Increments the lock counter for the invocation list. */
	inline void LockInvocationList( ) const
	{
		++InvocationListLockCount;
	}

	/** Decrements the lock counter for the invocation list. */
	inline void UnlockInvocationList( ) const
	{
		--InvocationListLockCount;
	}

protected:
	/**
	 * Helper function for derived classes of FMulticastDelegateBase to get at the delegate instance.
	 */
	static FORCEINLINE IDelegateInstance* GetDelegateInstanceProtectedHelper(const FDelegateBase& Base)
	{
		return Base.GetDelegateInstanceProtected();
	}

private:

	/** Holds the collection of delegate instances to invoke. */
	TInvocationList InvocationList;

	/** Used to determine when a compaction should happen. */
	int32 CompactionThreshold;

	/** Holds a lock counter for the invocation list. */
	mutable int32 InvocationListLockCount;
};
