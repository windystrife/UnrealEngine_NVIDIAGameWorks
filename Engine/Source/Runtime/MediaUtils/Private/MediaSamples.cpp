// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaSamples.h"

#include "IMediaAudioSample.h"
#include "IMediaBinarySample.h"
#include "IMediaOverlaySample.h"
#include "IMediaTextureSample.h"


/* IMediaSamples interface
*****************************************************************************/

bool FMediaSamples::FetchAudio(TRange<FTimespan> TimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> Sample;

	if (!AudioSampleQueue.Peek(Sample))
	{
		return false;
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (!TimeRange.Overlaps(TRange<FTimespan>(SampleTime, SampleTime + Sample->GetDuration())))
	{
		return false;
	}

	AudioSampleQueue.Pop();
	OutSample = Sample;

	return true;
}


bool FMediaSamples::FetchCaption(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	if (!CaptionSampleQueue.Peek(Sample))
	{
		return false;
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (!TimeRange.Overlaps(TRange<FTimespan>(SampleTime, SampleTime + Sample->GetDuration())))
	{
		return false;
	}

	CaptionSampleQueue.Pop();
	OutSample = Sample;

	return true;
}


bool FMediaSamples::FetchMetadata(TRange<FTimespan> TimeRange, TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe> Sample;

	if (!MetadataSampleQueue.Peek(Sample))
	{
		return false;
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (!TimeRange.Overlaps(TRange<FTimespan>(SampleTime, SampleTime + Sample->GetDuration())))
	{
		return false;
	}

	MetadataSampleQueue.Pop();
	OutSample = Sample;

	return true;
}


bool FMediaSamples::FetchSubtitle(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	if (!SubtitleSampleQueue.Peek(Sample))
	{
		return false;
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (!TimeRange.Overlaps(TRange<FTimespan>(SampleTime, SampleTime + Sample->GetDuration())))
	{
		return false;
	}

	SubtitleSampleQueue.Pop();
	OutSample = Sample;

	return true;
}


bool FMediaSamples::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;

	if (!VideoSampleQueue.Peek(Sample))
	{
		return false;
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (!TimeRange.Overlaps(TRange<FTimespan>(SampleTime, SampleTime + Sample->GetDuration())))
	{
		return false;
	}

	VideoSampleQueue.Pop();
	OutSample = Sample;

	return true;
}


void FMediaSamples::FlushSamples()
{
	AudioSampleQueue.RequestFlush();
	MetadataSampleQueue.RequestFlush();
	CaptionSampleQueue.RequestFlush();
	VideoSampleQueue.RequestFlush();
}
