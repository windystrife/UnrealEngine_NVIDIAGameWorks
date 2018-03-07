// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaTicker.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"

#include "IMediaTickable.h"


/* FMediaTicker structors
 *****************************************************************************/

FMediaTicker::FMediaTicker()
	: Stopping(false)
{
	WakeupEvent = FPlatformProcess::GetSynchEventFromPool(true);
}


FMediaTicker::~FMediaTicker()
{
	FPlatformProcess::ReturnSynchEventToPool(WakeupEvent);
	WakeupEvent = nullptr;
}


/* FRunnable interface
 *****************************************************************************/

bool FMediaTicker::Init()
{
	return true;
}


uint32 FMediaTicker::Run()
{
	while (!Stopping)
	{
		if (WakeupEvent->Wait(FTimespan::MaxValue()))
		{
			TickTickables();
			FPlatformProcess::Sleep(0.005f);
		}
	}

	return 0;
}


void FMediaTicker::Stop()
{
	Stopping = true;
	WakeupEvent->Trigger();
}


void FMediaTicker::Exit()
{
	// do nothing
}


/* IMediaTicker interface
 *****************************************************************************/

void FMediaTicker::AddTickable(const TSharedRef<IMediaTickable, ESPMode::ThreadSafe>& Tickable)
{
	FScopeLock Lock(&CriticalSection);
	Tickables.AddUnique(Tickable);
	WakeupEvent->Trigger();
}


void FMediaTicker::RemoveTickable(const TSharedRef<IMediaTickable, ESPMode::ThreadSafe>& Tickable)
{
	FScopeLock Lock(&CriticalSection);
	Tickables.Remove(Tickable);
}


/* FMediaTicker implementation
 *****************************************************************************/

void FMediaTicker::TickTickables()
{
	TArray<TWeakPtr<IMediaTickable, ESPMode::ThreadSafe>> TickablesCopy;
	{
		FScopeLock Lock(&CriticalSection);

		for (int32 TickableIndex = Tickables.Num() - 1; TickableIndex >= 0; --TickableIndex)
		{
			auto Tickable = Tickables[TickableIndex].Pin();

			if (Tickable.IsValid())
			{
				TickablesCopy.Add(Tickable);
			}
			else
			{
				Tickables.RemoveAtSwap(TickableIndex);
			}
		}
	}

	if (TickablesCopy.Num() > 0)
	{
		for (auto TickablePtr : TickablesCopy)
		{
			auto Tickable = TickablePtr.Pin();

			if (Tickable.IsValid())
			{
				Tickable->TickTickable();
			}
		}
	}
	else
	{
		WakeupEvent->Reset();
	}
}
