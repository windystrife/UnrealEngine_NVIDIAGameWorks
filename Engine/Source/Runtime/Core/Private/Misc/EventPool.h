// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/LockFreeList.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Event.h"

/**
 * Enumerates available event pool types.
 */
enum class EEventPoolTypes
{
	/** Creates events that have their signaled state reset automatically. */
	AutoReset,

	/** Creates events that have their signaled state reset manually. */
	ManualReset
};

class FSafeRecyclableEvent  final: public FEvent
{
public:
	FEvent* InnerEvent;

	FSafeRecyclableEvent(FEvent* InInnerEvent)
		: InnerEvent(InInnerEvent)
	{
	}

	~FSafeRecyclableEvent()
	{
		InnerEvent = nullptr;
	}

	virtual bool Create(bool bIsManualReset = false)
	{
		return InnerEvent->Create(bIsManualReset);
	}

	virtual bool IsManualReset()
	{
		return InnerEvent->IsManualReset();
	}

	virtual void Trigger()
	{
		InnerEvent->Trigger();
	}

	virtual void Reset()
	{
		InnerEvent->Reset();
	}

	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false)
	{
		return InnerEvent->Wait(WaitTime, bIgnoreThreadIdleStats);
	}

};

/**
 * Template class for event pools.
 *
 * Events are expensive to create on most platforms. This pool allows for efficient
 * recycling of event instances that are no longer used. Events can have their signaled
 * state reset automatically or manually. The PoolType template parameter specifies
 * which type of events the pool managers.
 *
 * @param PoolType Specifies the type of pool.
 * @see FEvent
 */
template<EEventPoolTypes PoolType>
class FEventPool
{
public:

	/**
	 * Gets the singleton instance of the event pool.
	 *
	 * @return Pool singleton.
	 */
	CORE_API static FEventPool& Get()
	{
		static FEventPool Singleton;
		return Singleton;
	}

	/**
	 * Gets an event from the pool or creates one if necessary.
	 *
	 * @return The event.
	 * @see ReturnToPool
	 */
	FEvent* GetEventFromPool()
	{
		FEvent* Result = Pool.Pop();

		if (!Result)
		{
			// FEventPool is allowed to create synchronization events.
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Result = FPlatformProcess::CreateSynchEvent((PoolType == EEventPoolTypes::ManualReset));
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		Result->AdvanceStats();

		check(Result);
		return new FSafeRecyclableEvent(Result);
	}

	/**
	 * Returns an event to the pool.
	 *
	 * @param Event The event to return.
	 * @see GetEventFromPool
	 */
	void ReturnToPool(FEvent* Event)
	{
		check(Event);
		check(Event->IsManualReset() == (PoolType == EEventPoolTypes::ManualReset));
		FSafeRecyclableEvent* SafeEvent = (FSafeRecyclableEvent*)Event;
		FEvent* Result = SafeEvent->InnerEvent;
		delete SafeEvent;
		check(Result);
		Result->Reset();
		Pool.Push(Result);
	}

private:

	/** Holds the collection of recycled events. */
	TLockFreePointerListUnordered<FEvent, PLATFORM_CACHE_LINE_SIZE> Pool;
};
