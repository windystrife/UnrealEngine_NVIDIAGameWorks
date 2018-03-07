// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/CircularBuffer.h"

/**
 * Implements a lock-free first-in first-out queue using a circular array.
 *
 * This class is thread safe only in two-thread scenarios, where the first thread
 * always reads and the second thread always writes. The head and tail indices are
 * stored in volatile memory to prevent the compiler from reordering the instructions
 * that are meant to update the indices AFTER items have been inserted or removed.
 *
 * The number of items that can be enqueued is one less than the queue's capacity,
 * because one item will be used for detecting full and empty states.
 *
 * @param ElementType The type of elements held in the queue.
 */
template<typename ElementType> class TCircularQueue
{
public:

	/**
	 * Default constructor.
	 *
	 * @param CapacityPlusOne The number of elements that the queue can hold (will be rounded up to the next power of 2).
	 */
	TCircularQueue(uint32 CapacityPlusOne)
		: Buffer(CapacityPlusOne)
		, Head(0)
		, Tail(0)
	{ }

	/** Virtual destructor. */
	virtual ~TCircularQueue() { }

public:

	/**
	 * Gets the number of elements in the queue.
	 *
	 * The result reflects the calling thread's current view. Since no
	 * locking is used, different threads may return different results.
	 *
	 * @return Number of queued elements.
	 */
	uint32 Count() const
	{
		int32 Count = Tail - Head;

		if (Count < 0)
		{
			Count += Buffer.Capacity();
		}

		return Count;
	}

	/**
	 * Removes an item from the front of the queue.
	 *
	 * @param OutElement Will contain the element if the queue is not empty.
	 * @return true if an element has been returned, false if the queue was empty.
	 * @note To be called only from consumer thread.
	 */
	bool Dequeue(ElementType& OutElement)
	{
		if (Head != Tail)
		{
			OutElement = Buffer[Head];
			Head = Buffer.GetNextIndex(Head);

			return true;
		}

		return false;
	}

	/**
	 * Empties the queue.
	 *
	 * @note To be called only from consumer thread.
	 * @see IsEmpty
	 */
	void Empty()
	{
		Head = Tail;
	}

	/**
	 * Adds an item to the end of the queue.
	 *
	 * @param Element The element to add.
	 * @return true if the item was added, false if the queue was full.
	 * @note To be called only from producer thread.
	 */
	bool Enqueue(const ElementType& Element)
	{
		uint32 NewTail = Buffer.GetNextIndex(Tail);

		if (NewTail != Head)
		{
			Buffer[Tail] = Element;
			Tail = NewTail;

			return true;
		}

		return false;
	}

	/**
	 * Checks whether the queue is empty.
	 *
	 * The result reflects the calling thread's current view. Since no
	 * locking is used, different threads may return different results.
	 *
	 * @return true if the queue is empty, false otherwise.
	 * @see Empty, IsFull
	 */
	FORCEINLINE bool IsEmpty() const
	{
		return (Head == Tail);
	}

	/**
	 * Checks whether the queue is full.
	 *
	 * The result reflects the calling thread's current view. Since no
	 * locking is used, different threads may return different results.
	 *
	 * @return true if the queue is full, false otherwise.
	 * @see IsEmpty
	 */
	bool IsFull() const
	{
		return (Buffer.GetNextIndex(Tail) == Head);
	}

	/**
	 * Returns the oldest item in the queue without removing it.
	 *
	 * @param OutItem Will contain the item if the queue is not empty.
	 * @return true if an item has been returned, false if the queue was empty.
	 * @note To be called only from consumer thread.
	 */
	bool Peek(ElementType& OutItem)
	{
		if (Head != Tail)
		{
			OutItem = Buffer[Head];

			return true;
		}

		return false;
	}

private:

	/** Holds the buffer. */
	TCircularBuffer<ElementType> Buffer;

	/** Holds the index to the first item in the buffer. */
	MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile uint32 Head GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);

	/** Holds the index to the last item in the buffer. */
	MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile uint32 Tail GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);
};
