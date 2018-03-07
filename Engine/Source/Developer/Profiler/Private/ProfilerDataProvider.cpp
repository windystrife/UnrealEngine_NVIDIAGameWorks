// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerDataProvider.h"


/*-----------------------------------------------------------------------------
	IDataProvider
-----------------------------------------------------------------------------*/

void IDataProvider::InternalDuplicate(const TSharedRef<IDataProvider>& DataProvider, const uint32 FrameStartIndex, const uint32 FrameEndIndex) const
{
	for(uint32 FrameIdx = FrameStartIndex; FrameIdx < FrameEndIndex; FrameIdx++)
	{
		const uint32 SampleStartIndex = GetSamplesIndicesForFrame(FrameIdx).X;
		const uint32 SampleEndIndex = GetSamplesIndicesForFrame(FrameIdx).Y;

		for(uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++)
		{
			FProfilerSample ProfilerSample(GetCollection()[ SampleIndex ]);

			for(int32 ChildIdx = 0; ChildIdx < ProfilerSample._ChildrenIndices.Num(); ChildIdx++)
			{
				ProfilerSample._ChildrenIndices[ChildIdx] -= SampleStartIndex;
			}

			DataProvider->AddDuplicatedSample(ProfilerSample);
		}
		DataProvider->AdvanceFrame(GetFrameTimeMS(FrameIdx));
	}
}


const uint32 IDataProvider::AdvanceFrame(const float DeltaTimeMS)
{
	if(!bHasAddedSecondStartMarker)
	{
		bHasAddedSecondStartMarker = true;

		// Add the default values.
		FrameCounters.Add(LastSecondFrameCounter);
		AccumulatedFrameCounters.Add(GetNumFrames());
	}

	ElapsedTimeMS += DeltaTimeMS;
	LastSecondFrameTimeMS += DeltaTimeMS;

	LastSecondFrameCounter ++;

	const uint32 SampleEndIndex = GetNumSamples();
	FrameIndices.Add(FIntPoint(FrameIndex, SampleEndIndex));

	FrameTimes.Add(DeltaTimeMS);
	ElapsedFrameTimes.Add(ElapsedTimeMS);

	// Update the values.
	FrameCounters.Last() = LastSecondFrameCounter;
	AccumulatedFrameCounters.Last() = GetNumFrames();

	int NumLongFrames = 0;
	while(LastSecondFrameTimeMS > 1000.0f)
	{
		if(NumLongFrames > 0)
		{
			// Add the default values.
			FrameCounters.Add(LastSecondFrameCounter);
			AccumulatedFrameCounters.Add(GetNumFrames());
		}

		LastSecondFrameTimeMS -= 1000.0f;
		bHasAddedSecondStartMarker = false;
		LastSecondFrameCounter = 0;
		NumLongFrames ++;
	}

	FrameIndex = SampleEndIndex;
	return FrameIndex;
}


const FIntPoint IDataProvider::GetClosestSamplesIndicesForTime(const float StartTimeMS, const float EndTimeMS) const
{
	// Find the first sample that start time is less or equal.
	const int StartIndex = FBinaryFindIndex::LessEqual(ElapsedFrameTimes, StartTimeMS);
	int32 EndIndex = StartIndex;

	// Find the first sample that start time is greater or equal, usually the next sample. 
	// More efficient then using binary search again.
	for (; EndIndex < ElapsedFrameTimes.Num(); ++EndIndex)
	{
		const float NextIndexTimeMS = ElapsedFrameTimes[EndIndex];
		if (NextIndexTimeMS >= EndTimeMS)
		{
			break;
		}
	}

	return FIntPoint(StartIndex, FMath::Min(ElapsedFrameTimes.Num(), EndIndex + 1));
}


/*-----------------------------------------------------------------------------
	FArrayDataProvider
-----------------------------------------------------------------------------*/

const uint32 FArrayDataProvider::AddHierarchicalSample(
	const uint32 InThreadID, 
	const uint32 InGroupID, 
	const uint32 InStatID, 
	const uint32 InDurationCycles,
	const uint32 InCallsPerFrame, 
	const uint32 InParentIndex /*= FProfilerSample::InvalidIndex */
)
{
	uint32 HierSampleIndex = Collection.Num();
	FProfilerSample HierarchicalSample(InThreadID, InGroupID, InStatID, InDurationCycles, InCallsPerFrame);

	Collection.AddElement(HierarchicalSample);

	if(FProfilerSample::IsIndexValid(InParentIndex))
	{
		FProfilerSample& Parent = Collection[ InParentIndex ];
		const uint32 InitialMemUsage = Parent.ChildrenIndices().GetAllocatedSize();
		Parent.AddChild(HierSampleIndex);
		const uint32 AccumulatedMemoryUsage = Parent.ChildrenIndices().GetAllocatedSize() - InitialMemUsage;
		ChildrenIndicesMemoryUsage += AccumulatedMemoryUsage;
	}

	return HierSampleIndex;
}


void FArrayDataProvider::AddCounterSample(
	const uint32 InGroupID, 
	const uint32 InStatID, 
	const double InCounter, 
	const EProfilerSampleTypes::Type InProfilerSampleType
)
{
	FProfilerSample CounterSample(InGroupID, InStatID, InCounter, InProfilerSampleType);
	Collection.AddElement(CounterSample);
}


const uint32 FArrayDataProvider::AddDuplicatedSample(const FProfilerSample& ProfilerSample)
{
	uint32 DuplicateSampleIndex = Collection.Num();
	Collection.AddElement(ProfilerSample);
	return DuplicateSampleIndex;
}


const uint32 FArrayDataProvider::GetNumSamples() const
{
	return Collection.Num();
}


const SIZE_T FArrayDataProvider::GetMemoryUsage() const
{
	SIZE_T MemoryUsage = FrameTimes.GetAllocatedSize();
	MemoryUsage += FrameIndices.GetAllocatedSize();
	MemoryUsage += ElapsedFrameTimes.GetAllocatedSize();
	MemoryUsage += FrameCounters.GetAllocatedSize();
	MemoryUsage += AccumulatedFrameCounters.GetAllocatedSize();

	MemoryUsage += Collection.GetAllocatedSize();
	MemoryUsage += ChildrenIndicesMemoryUsage;
	return MemoryUsage;
}
