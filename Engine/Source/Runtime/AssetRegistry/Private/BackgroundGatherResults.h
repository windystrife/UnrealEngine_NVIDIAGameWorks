// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * This is a specialized container for storing the results of the asset gather process.
 * Internally it is an array, but it acts like a FIFO queue. Items are pushed and appended, and then popped off for processing.
 * The difference between it and a standard queue are that the popped items aren't removed until the container is trimmed,
 * which allows multiple results to be processed per-trim with a minimal amount of array reshuffling (since we're removing from the front).
 */
template <typename T, typename Allocator = FDefaultAllocator>
class TBackgroundGatherResults
{
public:
	/** Constructor. */
	TBackgroundGatherResults()
		: PoppedCount(0)
	{
	}

	/** Push the given item onto the end of the queue. */
	FORCEINLINE void Push(const T& Item)
	{
		InternalQueue.Add(Item);
	}
	
	/** Push the given item onto the end of the queue. */
	FORCEINLINE void Push(T&& Item)
	{
		InternalQueue.Add(MoveTemp(Item));
	}
	
	/** Append the given items onto the end of the queue. */
	FORCEINLINE void Append(const TArray<T>& Items)
	{
		InternalQueue.Append(Items);
	}
	
	/** Append the given items onto the end of the queue. */
	FORCEINLINE void Append(TArray<T>&& Items)
	{
		InternalQueue.Append(MoveTemp(Items));
	}
	
	/** Pop an item from the front of the queue. The returned data is valid until the queue is trimmed. */
	FORCEINLINE T& Pop()
	{
		return InternalQueue[PoppedCount++];
	}
	
	/** Trim any popped items from this queue. */
	FORCEINLINE void Trim()
	{
		if (PoppedCount > 0)
		{
			InternalQueue.RemoveAt(0, PoppedCount);
			PoppedCount = 0;
		}
	}
	
	/** Get the number of items left to process in this queue. */
	FORCEINLINE int32 Num() const
	{
		return InternalQueue.Num() - PoppedCount;
	}
	
	/** Empty the queue, keeping the current allocation. */
	FORCEINLINE void Reset()
	{
		PoppedCount = 0;
		InternalQueue.Reset();
	}

	/** Empty the queue, discarding the current allocation. */
	FORCEINLINE void Empty()
	{
		PoppedCount = 0;
		InternalQueue.Empty();
	}

	/** Gets the size of the current allocation */
	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return InternalQueue.GetAllocatedSize();
	}

	/** Prioritize any items that pass the given predicate to be at the front of the queue. */
	void Prioritize(TFunctionRef<bool(const T&)> Pred)
	{
		int32 LowestNonPriorityIdx = PoppedCount;
		for (int32 Idx = PoppedCount; Idx < InternalQueue.Num(); ++Idx)
		{
			if (Pred(InternalQueue[Idx]))
			{
				InternalQueue.Swap(Idx, LowestNonPriorityIdx++);
			}
		}
	}


	
private:
	typedef TArray<T, Allocator> InternalQueueType;
	typedef typename InternalQueueType::RangedForIteratorType RangedForIteratorType;
	typedef typename InternalQueueType::RangedForConstIteratorType RangedForConstIteratorType;

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	#if TARRAY_RANGED_FOR_CHECKS
		FORCEINLINE friend RangedForIteratorType      begin(      TBackgroundGatherResults& Results) { return RangedForIteratorType     (Results.InternalQueue.Num(), Results.InternalQueue.GetData() + Results.PoppedCount); }
		FORCEINLINE friend RangedForConstIteratorType begin(const TBackgroundGatherResults& Results) { return RangedForConstIteratorType(Results.InternalQueue.Num(), Results.InternalQueue.GetData() + Results.PoppedCount); }
		FORCEINLINE friend RangedForIteratorType      end  (      TBackgroundGatherResults& Results) { return RangedForIteratorType     (Results.InternalQueue.Num(), Results.InternalQueue.GetData() + Results.InternalQueue.Num()); }
		FORCEINLINE friend RangedForConstIteratorType end  (const TBackgroundGatherResults& Results) { return RangedForConstIteratorType(Results.InternalQueue.Num(), Results.InternalQueue.GetData() + Results.InternalQueue.Num()); }
	#else
		FORCEINLINE friend RangedForIteratorType      begin(      TBackgroundGatherResults& Results) { return Results.InternalQueue.GetData() + Results.PoppedCount; }
		FORCEINLINE friend RangedForConstIteratorType begin(const TBackgroundGatherResults& Results) { return Results.InternalQueue.GetData() + Results.PoppedCount; }
		FORCEINLINE friend RangedForIteratorType      end  (      TBackgroundGatherResults& Results) { return Results.InternalQueue.GetData() + Results.InternalQueue.Num(); }
		FORCEINLINE friend RangedForConstIteratorType end  (const TBackgroundGatherResults& Results) { return Results.InternalQueue.GetData() + Results.InternalQueue.Num(); }
	#endif

private:
	/** Number of items that have been popped off the queue without it being trimmed. Items before this count should not be mutated, and we should trim to this number. */
	int32 PoppedCount;
	
	/** Internal FIFO queue of data. */
	InternalQueueType InternalQueue;
};
