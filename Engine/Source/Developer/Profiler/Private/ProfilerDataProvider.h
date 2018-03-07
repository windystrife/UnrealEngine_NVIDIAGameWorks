// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilerCommon.h"
#include "ProfilerSample.h"

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** Data provider interface, acts as a container for profiler samples. */
class IDataProvider
	: public TSharedFromThis<IDataProvider>
{
	friend class FGraphDataSource;
	friend class FEventGraphData;

public:

	/** Default constructor. */
	IDataProvider()
		: ElapsedTimeMS( 0.0f )
		, LastSecondFrameTimeMS( 0.0f )
		, FrameIndex( 0 )
		, LastSecondFrameCounter( 0 )
		, bHasAddedSecondStartMarker( false )
	{}

	/** Virtual destructor. */
	virtual ~IDataProvider() { }

protected:

	/**
	 * Method for creating a duplicated copy of this data provider instance with a particular group of frames.
	 *
	 * Data is stored in the instance of FArrayDataProvider class.
	 * Data source for the SEventGraph widget.
	 *
	 * @return A shared reference with a new instance of the data provider.
	 */
	template< class TDataProviderType >
	TSharedRef<IDataProvider> Duplicate( const uint32 FrameStartIndex, const uint32 NumFrames = 1 ) const
	{
		const uint32 TotalNumFrames = GetNumFrames();
		check( NumFrames <= TotalNumFrames )
		check( NumFrames >= 1 );

		check( FrameStartIndex >= 0 );
		
		const uint32 FrameEndIndex = FrameStartIndex + NumFrames;
		check( FrameEndIndex <= TotalNumFrames );

		TSharedRef<IDataProvider> DataProvider = MakeShareable( new TDataProviderType );
		InternalDuplicate( DataProvider, FrameStartIndex, FrameEndIndex );

		return DataProvider;
	}

protected:
	/** Helper method used to create a copy of a specified data provider. Removes the template parameters, so can be implemented in the source file. */
	void InternalDuplicate( const TSharedRef<IDataProvider>& DataProvider, const uint32 FrameStartIndex, const uint32 FrameEndIndex ) const;

public:
	/**
	 * Adds a new hierarchical sample to the data provider.
	 *
	 * @param InThreadID The ID of the thread that this profiler sample was captured on
	 * @param InGroupID The ID of the stat group that this profiler sample belongs to
	 * @param InStatID The ID of the stat of this profiler sample
	 * @param InStartMS The start time of this profiler sample, in milliseconds
	 * @param InDurationMS The duration of this profiler sample, in milliseconds
	 * @param InCallsPerFrame The number of times this counter was called in the frame it was captured in
	 * @param InParentIndex The parent of this profiler sample, as an index to a profiler sample
	 *
	 */
	virtual const uint32 AddHierarchicalSample
	( 
		const uint32 InThreadID, 
		const uint32 InGroupID, 
		const uint32 InStatID, 
		const uint32 InDurationCycles, 
		const uint32 InCallsPerFrame, 
		const uint32 InParentIndex = FProfilerSample::InvalidIndex 
	) = 0;

	/**
	 * Adds a new non-hierarchical sample to the data provider.
	 *
	 * @param InGroupID The ID of the stat group that this profiler sample belongs to
	 * @param InStatID The ID of the stat of this profiler sample
	 * @param InCounter The counter value for this profiler sample
	 * @param InProfilerSampleType The profiler sample type of this profiler sample
	 *
	 */
	virtual void AddCounterSample
	( 
		const uint32 InGroupID, 
		const uint32 InStatID, 
		const double InCounter, 
		const EProfilerSampleTypes::Type InProfilerSampleType 
	) = 0;

protected:

	/**  
	 * Adds a sample to the data provider.
	 * 
	 * @return An index to the newly created profiler sample.
	 */
	virtual const uint32 AddDuplicatedSample
	( 
		const FProfilerSample& ProfilerSample
	) = 0;

public:

	/**
	 * @return a reference to the collection where all the profiler samples are stored.
	 */
	virtual const FProfilerSampleArray& GetCollection() const = 0;

public:

	/** Informs this data provider that the frame has been advanced. */
	const uint32 AdvanceFrame( const float DeltaTimeMS );

	/** @return Number of profiler samples. */
	virtual const uint32 GetNumSamples() const = 0;

	/** @return Number of frames that have been rendered from the beginning. */
	const uint32 GetNumFrames() const
	{
		return FrameIndices.Num();
	}

	/** @return Number of milliseconds that have passed from the beginning. */
	const double GetTotalTimeMS() const
	{
		return ElapsedTimeMS;
	}

	const uint32 GetFrameCounter( const uint32 SecondIndex ) const
	{
		return FrameCounters[ SecondIndex ];
	}

	const uint32 GetAccumulatedFrameCounter( const uint32 SecondIndex ) const
	{
		return AccumulatedFrameCounters[ SecondIndex ];
	}

	const FIntPoint GetClosestSamplesIndicesForTime( const float StartTimeMS, const float EndTimeMS ) const;

	/**
	 * @return An instance of FIntPoint 
	 * where X is an index of first sample in the frame, and 
	 * where Y is an index of last sample in the frame + 1. 
	 */
	const FIntPoint& GetSamplesIndicesForFrame( const uint32 InFrameIndex ) const
	 {
		 return FrameIndices[ InFrameIndex ];
	 }

	/** @return frame duration for the specified frame, in milliseconds. */
	const float GetFrameTimeMS( const uint32 InFrameIndex ) const
	{
		return FrameTimes[ InFrameIndex ];
	}

	/** @return the elapsed time for the specified frame, in milliseconds. */
	const float GetElapsedFrameTimeMS( const uint32 InFrameIndex ) const
	{
		return ElapsedFrameTimes[ InFrameIndex ];
	}
 
	virtual const SIZE_T GetMemoryUsage() const = 0;
		
protected:

	/** An array of indices to the frame's range. */
	TArray<FIntPoint> FrameIndices;

	/** Each element in this array stores the frame time, accessed by a frame index, in millisecond. */
	TArray<float> FrameTimes;

	/** Each element in this array stores the total frame time, accessed by a frame index, in millisecond. */
	TArray<float> ElapsedFrameTimes;

	/** Each element in the array stores the number of frames, accessed by a second index. */
	TArray<uint32> FrameCounters;

	/** Each element in the array stores the total number of frames, accessed by a second index. */
	TArray<uint32> AccumulatedFrameCounters;

	/** How many milliseconds have passed from the beginning. */
	double ElapsedTimeMS;

private:

	/** Accumulates frame times until it reaches a value of one second. */
	float LastSecondFrameTimeMS;

	/** Current frame index. */
	uint32 FrameIndex;

	/** The number of frame that have passed from the last second. */
	uint32 LastSecondFrameCounter;

	/** True, if we have added a frame start marker. */
	bool bHasAddedSecondStartMarker;
};


/** Implements the data provider interface where samples are stored in the instance of the TArray class. */
class FArrayDataProvider
	: public IDataProvider
{
public:

	FArrayDataProvider()
		: IDataProvider()
		, ChildrenIndicesMemoryUsage( 0 )
	{}

	virtual ~FArrayDataProvider() { }

public:

	//~ IDataProvider interface

	virtual const uint32 AddHierarchicalSample
	( 
		const uint32 InThreadID, 
		const uint32 InGroupID, 
		const uint32 InStatID, 
		const uint32 InDurationCycles,
		const uint32 InCallsPerFrame, 
		const uint32 InParentIndex = FProfilerSample::InvalidIndex 
	) override;

	virtual void AddCounterSample
	( 
		const uint32 InGroupID, 
		const uint32 InStatID, 
		const double InCounter, 
		const EProfilerSampleTypes::Type InProfilerSampleType 
	) override;

	virtual const uint32 AddDuplicatedSample( const FProfilerSample& ProfilerSample ) override;

	virtual const uint32 GetNumSamples() const override;

	virtual const SIZE_T GetMemoryUsage() const override;;

	virtual const FProfilerSampleArray& GetCollection() const override
	{
		return Collection;
	}

protected:

	/** Profiler samples stored in TArray. */
	FProfilerSampleArray Collection;

	uint64 ChildrenIndicesMemoryUsage;
};
