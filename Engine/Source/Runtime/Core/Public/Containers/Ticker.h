// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
#include "Delegates/Delegate.h"


/**
 * Ticker delegates return true to automatically reschedule at the same delay or false for one-shot.
 * You will not get more than one fire per "frame", which is just a FTicker::Tick call.
 * DeltaTime argument iz the time since the last game frame, *not* since the last tick the
 * delegate received.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FTickerDelegate, float);

/**
 * Ticker class. Fires delegates after a delay.
 * 
 * Note: Do not try to add the same delegate instance twice, as there is no way to remove only a single instance (see member RemoveTicker).
 */
class FTicker
{
public:

	/** Singleton used for the ticker in Core / Launch. If you add a new ticker for a different subsystem, do not put that singleton here! **/
	CORE_API static FTicker& GetCoreTicker();

	CORE_API FTicker();
	CORE_API ~FTicker();

	/**
	 * Add a new ticker with a given delay / interval
	 * 
	 * @param InDelegate Delegate to fire after the delay
	 * @param InDelay Delay until next fire; 0 means "next frame"
	 */
	CORE_API FDelegateHandle AddTicker(const FTickerDelegate& InDelegate, float InDelay = 0.0f);

	/**
	 * Removes a previously added ticker delegate.
	 * 
	 * Note: will remove ALL tickers that use this handle, as there's no way to uniquely identify which one you are trying to remove.
	 *
	 * @param Handle The handle of the ticker to remove.
	 */
	CORE_API void RemoveTicker(FDelegateHandle Handle);

	/**
	 * Fire all tickers who have passed their delay and reschedule the ones that return true
	 * 
	 * Note: This reschedule has timer skew, meaning we always wait a full Delay period after 
	 * each invocation even if we missed the last interval by a bit. For instance, setting a 
	 * delay of 0.1 seconds will not necessarily fire the delegate 10 times per second, depending 
	 * on the Tick() rate. What this DOES guarantee is that a delegate will never be called MORE
	 * FREQUENTLY than the Delay interval, regardless of how far we miss the scheduled interval.
	 * 
	 * @param DeltaTime	time that has passed since the last tick call
	 */
	CORE_API void Tick(float DeltaTime);

private:
	/**
	 * Element of the priority queue
	 */
	struct FElement
	{
		/** Time that this delegate must not fire before **/
		double FireTime;
		/** Delay that this delegate was scheduled with. Kept here so that if the delegate returns true, we will reschedule it. **/
		float DelayTime;
		/** Delegate to fire **/
		FTickerDelegate Delegate;

		/** Default ctor is only required to implement CurrentElement handling without making it a pointer. */
		CORE_API FElement();
		/** This is the ctor that the code will generally use. */
		CORE_API FElement(double InFireTime, float InDelayTime, const FTickerDelegate& InDelegate);
		
		/** Fire the delegate if it is fireable **/
		CORE_API bool Fire(float DeltaTime);
	};

	/** Current time of the ticker **/
	double CurrentTime;
	/** Future delegates to fire **/
	TArray<FElement> Elements;
	/** List of delegates that have been considered during Tick. Either they been fired or are not yet ready.*/
	TArray<FElement> TickedElements;
	/** Current element being ticked (only valid during tick). */
	FElement CurrentElement;
	/** State to track whether CurrentElement is valid. */
	bool bInTick;
	/** State to track whether the CurrentElement removed itself during its own delegate execution. */
	bool bCurrentElementRemoved;
};


/**
 * Base class for ticker objects
 */
class FTickerObjectBase
{
public:

	/**
	 * Constructor
	 *
	 * @param InDelay Delay until next fire; 0 means "next frame"
	 * @param Ticker the ticker to register with. Defaults to FTicker::GetCoreTicker().
	*/
#if ( PLATFORM_WINDOWS && defined(__clang__) )
	CORE_API FTickerObjectBase()							// @todo clang: non-default argument constructors needed to prevent an ICE in clang
		: FTickerObjectBase(0.0f, FTicker::GetCoreTicker())	// https://llvm.org/bugs/show_bug.cgi?id=28137
	{
	}

	CORE_API FTickerObjectBase(float InDelay)
		: FTickerObjectBase(InDelay, FTicker::GetCoreTicker())
	{
	}

	CORE_API FTickerObjectBase(float InDelay, FTicker& Ticker);
#else
	CORE_API FTickerObjectBase(float InDelay = 0.0f, FTicker& Ticker = FTicker::GetCoreTicker());
#endif

	/** Virtual destructor. */
	CORE_API virtual ~FTickerObjectBase();

	/**
	 * Pure virtual that must be overloaded by the inheriting class.
	 *
	 * @param DeltaTime	time passed since the last call.
	 * @return true if should continue ticking
	 */
	virtual bool Tick(float DeltaTime) = 0;

private:
	// noncopyable idiom
	FTickerObjectBase(const FTickerObjectBase&);
	FTickerObjectBase& operator=(const FTickerObjectBase&);

	/** Ticker to register with */
	FTicker& Ticker;
	/** Delegate for callbacks to Tick */
	FDelegateHandle TickHandle;
};
