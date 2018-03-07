// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Containers/Ticker.h"
#include "Stats/Stats.h"
#include "Misc/TimeGuard.h"

FTicker::FTicker()
	: CurrentTime(0.0)
	, bInTick(false)
	, bCurrentElementRemoved(false)
{
}

FTicker::~FTicker()
{
}

FDelegateHandle FTicker::AddTicker(const FTickerDelegate& InDelegate, float InDelay)
{
	// We can add elements safely even during tick.
	Elements.Emplace(CurrentTime + InDelay, InDelay, InDelegate);
	// @todo this needs a unique handle for each add call to allow you to register the same delegate twice with a different delay safely.
	// because of this, RemoveTicker removes all elements that use this handle.
	return InDelegate.GetHandle();
}

void FTicker::RemoveTicker(FDelegateHandle Handle)
{
	// must remove the handle from both arrays because we could be in the middle of a tick, 
	// and may be removing ourselves or something else we've already considered this Tick
	auto CompareHandle = [=](const FElement& Element){ return Element.Delegate.GetHandle() == Handle; };
	// We can remove elements safely even during tick.
	Elements.RemoveAllSwap(CompareHandle);
	TickedElements.RemoveAllSwap(CompareHandle);
	// if we are ticking, we must check for the edge case of the CurrentElement removing itself.
	if (bInTick)
	{
		if (CompareHandle(CurrentElement))
		{
			// Technically it's possible for someone to try to remove CurrentDelegate multiple times, so make sure we never set this value to false in here.
			bCurrentElementRemoved = true;
		}
	}
}

void FTicker::Tick(float DeltaTime)
{
	SCOPE_TIME_GUARD(TEXT("FTicker::Tick"));

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FTicker_Tick);
	if (!Elements.Num())
	{
		return;
	}

	// make sure we scope the "InTick" state
	TGuardValue<bool> TickGuard(bInTick, true);

	CurrentTime += DeltaTime;
	// we will keep popping off elements until the array is empty.
	// We cannot keep references or iterators into any of these containers because the act of firing a delegate could
	// add or remove any other delegate, including itself. 
	// As we pop off elements, we track the CurrentElement in case it tries to remove itself.
	// When finished, we put it onto a TickedElements list. This allows another delegate in the list to remove it,
	// while still keeping track of elements we've ticked by popping off the Elements array.
	// See RemoveTicker for details on how this state is used.
	while (Elements.Num())
	{
		// as an optimization, if the Element is not ready to fire, move it immediately to the TickedElements list.
		// This is safe because no one can mutate the arrays while we do this.
		if (Elements.Last().FireTime > CurrentTime)
		{
			TickedElements.Add(Elements.Pop(false));
		}
		else
		{
			// we now know the element is ready to fire, so pop it off the list.
			CurrentElement = Elements.Pop(false);
			// reset this state every time we reassign to CurrentElement.
			bCurrentElementRemoved = false;
			// fire the delegate. It can return false to tell us to remove it immediately.
			bool bRemoveElement = !CurrentElement.Fire(DeltaTime);
			// The act of firing the delegate could also have caused it to remove itself.
			// if either happens, we just won't add this to the TickedElements array.
			if (!bRemoveElement && !bCurrentElementRemoved)
			{
				// update the fire time.
				// Note this is where Timer skew occurs. Use FireTime += DelayTime if skew is not wanted.
				CurrentElement.FireTime = CurrentTime + CurrentElement.DelayTime;
				// push this element onto the TickedElement list so we know we already ticked it.
				TickedElements.Push(CurrentElement);
			}
		}
	}

	// Now that we've considered all the delegates, we swap it back into the Elements array.
	Exchange(TickedElements, Elements);
}

FTicker::FElement::FElement() 
	: FireTime(0.0)
	, DelayTime(0.0f)
{}

FTicker::FElement::FElement(double InFireTime, float InDelayTime, const FTickerDelegate& InDelegate) 
	: FireTime(InFireTime)
	, DelayTime(InDelayTime)
	, Delegate(InDelegate)
{}

bool FTicker::FElement::Fire(float DeltaTime)
{
	if (Delegate.IsBound())
	{
		return Delegate.Execute(DeltaTime);
	}
	return false;
}

FTickerObjectBase::FTickerObjectBase(float InDelay, FTicker& InTicker)
	: Ticker(InTicker)
{
	// Register delegate for ticker callback
	FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FTickerObjectBase::Tick);
	TickHandle = Ticker.AddTicker(TickDelegate, InDelay);
}

FTickerObjectBase::~FTickerObjectBase()
{
	// Unregister ticker delegate
	if (TickHandle != FDelegateHandle())
	{
		Ticker.RemoveTicker(TickHandle);
		TickHandle = FDelegateHandle();
	}
}

