// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaClock.h"

#include "CoreTypes.h"

#include "IMediaClockSink.h"


/* FMediaClock structors
 *****************************************************************************/

FMediaClock::FMediaClock()
	: DeltaTime(FTimespan::Zero())
	, Timecode(FTimespan::MinValue())
{ }


/* FMediaClock interface
 *****************************************************************************/

void FMediaClock::TickFetch()
{
	for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
	{
		auto Sink = Sinks[SinkIndex].Pin();

		if (Sink.IsValid())
		{
			Sink->TickFetch(DeltaTime, Timecode);
		}
		else
		{
			Sinks.RemoveAtSwap(SinkIndex);
		}
	}
}


void FMediaClock::TickInput()
{
	for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
	{
		auto Sink = Sinks[SinkIndex].Pin();

		if (Sink.IsValid())
		{
			Sink->TickInput(DeltaTime, Timecode);
		}
		else
		{
			Sinks.RemoveAtSwap(SinkIndex);
		}
	}
}


void FMediaClock::TickOutput()
{
	for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
	{
		auto Sink = Sinks[SinkIndex].Pin();

		if (Sink.IsValid())
		{
			Sink->TickOutput(DeltaTime, Timecode);
		}
		else
		{
			Sinks.RemoveAtSwap(SinkIndex);
		}
	}
}


void FMediaClock::TickRender()
{
	for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
	{
		auto Sink = Sinks[SinkIndex].Pin();

		if (Sink.IsValid())
		{
			Sink->TickRender(DeltaTime, Timecode);
		}
		else
		{
			Sinks.RemoveAtSwap(SinkIndex);
		}
	}
}


void FMediaClock::UpdateTimecode(const FTimespan NewTimecode, bool NewTimecodeLocked)
{
	if (Timecode == FTimespan::MinValue())
	{
		DeltaTime = FTimespan::Zero();
	}
	else
	{
		DeltaTime = NewTimecode - Timecode;
	}

	Timecode = NewTimecode;
	TimecodeLocked = NewTimecodeLocked;
}


/* IMediaClock interface
 *****************************************************************************/

void FMediaClock::AddSink(const TSharedRef<IMediaClockSink, ESPMode::ThreadSafe>& Sink)
{
	Sinks.AddUnique(Sink);
}


FTimespan FMediaClock::GetTimecode() const
{
	return Timecode;
}


bool FMediaClock::IsTimecodeLocked() const
{
	return TimecodeLocked;
}


void FMediaClock::RemoveSink(const TSharedRef<IMediaClockSink, ESPMode::ThreadSafe>& Sink)
{
	Sinks.Remove(Sink);
}
