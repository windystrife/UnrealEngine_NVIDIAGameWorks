// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ChunkedArray.h"
#include "Stats/Stats.h"

/*-----------------------------------------------------------------------------
	Type definitions
-----------------------------------------------------------------------------*/

/** Type definition for an array of profiler samples.*/
typedef TChunkedArray< class FProfilerSample, 1024 * 64 > FProfilerSampleArray;

/** Type definition for an array of indices.*/
typedef TArray< uint32 > FIndicesArray;


/*-----------------------------------------------------------------------------
	Enumerators
-----------------------------------------------------------------------------*/

// TODO: Feedback, is this grouping sufficient for the profiler?

/** Enumerates profiler sample types. */
struct EProfilerSampleTypes
{
	enum Type : uint32
	{
		/** Hierarchical - Displayed as a time, used by hierarchical stats, as "%.3f ms". */
		HierarchicalTime = 0,

		/** Numerical - Displayed as a integer number, as "%i". */
		NumberInt = 1,

		/** Numerical - Displayed as a floating number, as "%.2f". */
		NumberFloat = 2,

		/** Memory - Displayed as a human readable data counter, as "%.2f kb". */
		Memory = 3,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax = 4,

		/** For extracting the type from the combined type. */
		Shift = 0,
		NumBits = 3,
		Num = (1 << NumBits) - 1,
		Mask = Num,
	};

	/**
	 * @param EProfilerSampleTypes - The value to get the string for.
	 *
	 * @return string representation of the specified EProfilerSampleTypes value.
	 */
	static FString ToName( EProfilerSampleTypes::Type ProfilerSampleType );

	/**
	 * @param EProfilerSampleTypes - The value to get the string for.
	 *
	 * @return string representation with more detailed explanation of the specified EProfilerSampleTypes value.
	 */
	static FString ToDescription( EProfilerSampleTypes::Type ProfilerSampleType );
};

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** 
 *	A base profiler sample, should be sufficient to store and visualize all range of profiling samples. 
 */
class FProfilerSample
{	
	friend class IDataProvider;

	enum class EStatID : uint32
	{
		/** For extracting the StatID from the combined type. */
		Shift = 13,
		NumBits = 19,
		Num = (1 << NumBits) - 1, // 2 ^ 19 -1 = 524287 unique stats
		Mask = Num,
	};

	enum class EGroupID : uint32
	{
		/** For extracting the StatID from the combined type. */
		Shift = 3,
		NumBits = 10,
		Num = (1 << NumBits) - 1, // 2^10 -1 = 1023 different groups
		Mask = Num,
	};

protected:
	/** Child samples of this profiler sample, as indices to profiler samples. */
	FIndicesArray _ChildrenIndices;

	/** 
	  * Contains stat data, similar to the FStatMessage.StatData
	  * Can be interpreted as uint64 counter or uint32 duration and uint32 callcount.
	  */
	int64 StatData;

	/** The ID of the thread that this profiler sample was captured on. */
	const uint32 _ThreadID;

	/** 
	  * The ID of the stat of the profiler sample, eg. Frametime				19bits
	  * The ID of the stat group this profiler sample belongs to, eg. Engine	10bits
	  * Type of this profiler sample											03bits
	  */
	uint32 CombinedMeta;

public:
	static SIZE_T SizeOf()
	{
		return sizeof(FProfilerSample);
	}

	/** The ID of the thread that this profiler sample was captured on. */
	const uint32 ThreadID() const 
	{ 
		return _ThreadID; 
	}

	/** The ID of the stat group this profiler sample belongs to, eg. Engine */
	FORCEINLINE_DEBUGGABLE const uint32 GroupID() const
	{ 
		return (CombinedMeta >> (uint32)EGroupID::Shift) & (uint32)EGroupID::Mask;
	}

	/** The ID of the stat of the profiler sample, eg. Frametime */
	FORCEINLINE_DEBUGGABLE const uint32 StatID() const
	{ 
		return (CombinedMeta >> (uint32)EStatID::Shift) & (uint32)EStatID::Mask;
	}

	/** Type of this profiler sample. */
	FORCEINLINE_DEBUGGABLE const EProfilerSampleTypes::Type Type() const
	{
		return (EProfilerSampleTypes::Type)((CombinedMeta >> (uint32)EProfilerSampleTypes::Shift) & (uint32)EProfilerSampleTypes::Mask);
	};

	/** Sets new stat id and sample type. */
	FORCEINLINE_DEBUGGABLE void SetMeta( const uint32 StatID, const uint32 GroupID, const EProfilerSampleTypes::Type SampleType )
	{
		checkSlow( StatID <= (uint32)EStatID::Num );
		checkSlow( GroupID <= (uint32)EGroupID::Num );
		checkSlow( SampleType <= (uint32)EProfilerSampleTypes::Num );

		CombinedMeta = 0;
		CombinedMeta |= (StatID & (uint32)EStatID::Mask) << (uint32)EStatID::Shift;
		CombinedMeta |= (GroupID & (uint32)EGroupID::Mask) << (uint32)EGroupID::Shift;
		CombinedMeta |= (SampleType & (uint32)EProfilerSampleTypes::Mask) << (uint32)EProfilerSampleTypes::Shift;
	}

	/** Duration of the profiler sample, in cycles. */
	const uint32 GetDurationCycles() const
	{
		return FromPackedCallCountDuration_Duration( StatData );
	}

	/** Call count of the profiler sample, only for hierarchical. */
	const uint32 GetCallCount() const
	{
		return FromPackedCallCountDuration_CallCount( StatData );
	}

	/** Value of the profiler sample, as double. */
	const double GetDoubleValue() const
	{
		const double Value = *(double*)&StatData;
		return Value;
	}
	/** Child samples of this profiler sample, as indices to profiler samples. */
	const FIndicesArray& ChildrenIndices() const 
	{ 
		return _ChildrenIndices; 
	}

	/** Adds a child to this profiler sample. */
	void AddChild( const uint32 ChildIndex ) 
	{
		_ChildrenIndices.Add( ChildIndex );
	}

	/** Fixes children ordering for the hierarchical representation. */
	void FixupChildrenOrdering( const TMap<uint32,uint32>& ChildrenOrderingIndices );

public:
	/** Constant, invalid index. */
	static const uint32 InvalidIndex = (uint32)(-1);
	/** Constant, invalid profiler sample, will be use in situation when a profiler sample can't be found or doesn't exist. */
	static const FProfilerSample Invalid;

public:
	/**
	 * Initialization constructor for hierarchical samples.
	 *
	 * @param InThreadID		- The ID of the thread that this profiler sample was captured on
	 * @param InGroupID			- The ID of the stat group that this profiler sample belongs to
	 * @param InStatID			- The ID of the stat of this profiler sample
	 * @param InDurationMs		- The duration of this profiler sample, in milliseconds
	 * @param InCallsPerFrame	- The number of times this counter was called in the frame it was captured in
	 *
	 */
	FORCEINLINE_DEBUGGABLE FProfilerSample( const uint32 InThreadID, const uint32 InGroupID, const uint32 InStatID, const uint32 InDurationCycles, const uint32 InCallsPerFrame )
		: _ThreadID( InThreadID )
	{
		SetMeta( InStatID, InGroupID, EProfilerSampleTypes::HierarchicalTime );
		StatData = ToPackedCallCountDuration( InCallsPerFrame, InDurationCycles );
	}

	/**
	 * Initialization constructor for non-hierarchical samples.
	 *
	 * @param InGroupID				- The ID of the stat group that this profiler sample belongs to
	 * @param InStatID				- The ID of the stat of this profiler sample
	 * @param InCounter				- The counter value for this profiler sample
	 * @param InProfilerSampleType	- The profiler sample type of this profiler sample
	 *
	 */
	FORCEINLINE_DEBUGGABLE FProfilerSample( const uint32 InGroupID, const uint32 InStatID, const double InCounter, const EProfilerSampleTypes::Type InProfilerSampleType )
		: _ThreadID( 0 )
	{
		SetMeta( InStatID, InGroupID, InProfilerSampleType );
		*(double*)&StatData = InCounter;
	}

	/** Copy constructor. */
	FORCEINLINE_DEBUGGABLE explicit FProfilerSample( const FProfilerSample& Other )
		: _ChildrenIndices( Other._ChildrenIndices )
		, StatData(Other.StatData)
		, _ThreadID( Other._ThreadID )
		, CombinedMeta( Other.CombinedMeta )		
	{
	}

	/** Default constructor, creates an invalid profiler sample. */
	FORCEINLINE_DEBUGGABLE FProfilerSample()
		: StatData(0)
		, _ThreadID( 0 )		
	{
		SetMeta( 0, 0, EProfilerSampleTypes::InvalidOrMax );
	}

	/** Updates the duration of this sample, should be only used to update the root stat. */
	void SetDurationCycles( const uint32 InDurationCycles )
	{
		StatData = ToPackedCallCountDuration( GetCallCount(), InDurationCycles );
	}

	/** @return true if this profiler sample is valid. */
	const bool IsValid() const
	{
		return Type() != EProfilerSampleTypes::InvalidOrMax;
	}

	/** @return true if IndexToCheck is valid. */
	static const bool IsIndexValid( const uint32 IndexToCheck ) 
	{
		return IndexToCheck != InvalidIndex;
	}
};

class IHistogramDataSource
{
public:
	virtual int32 GetCount(float MinVal, float MaxVal) = 0;
	virtual int32 GetTotalCount() = 0;
};
