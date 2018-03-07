// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/IndirectArray.h"
#include "Containers/ChunkedArray.h"
#include "Misc/ScopeLock.h"
#include "ProfilerCommon.h"
#include "Stats/StatsData.h"

/*-----------------------------------------------------------------------------
	Basic structures
-----------------------------------------------------------------------------*/

/** Helper struct used to calculate inclusive times, ignoring recursive calls. */
struct FInclusiveTime
{
	/** Duration in cycles. */
	uint32 DurationCycles;
	/** Number of calls. */
	int32 CallCount;
	/** Number of recursion. */
	int32 Recursion;
	FInclusiveTime()
		: DurationCycles( 0.0 )
		, CallCount( 0 )
		, Recursion( 0 )
	{}
};


/** Profiler stack node, used to store the whole call stack for one frame. */
struct FProfilerStackNode
{
	/** Short name. */
	FName StatName;
	FName LongName;

	TArray<FProfilerStackNode*> Children;
	FProfilerStackNode* Parent;

	int64 CyclesStart;
	int64 CyclesEnd;

	// #Profiler: 2014-04-23 This should be based on the global time.
	double CycleCounterStartTimeMS;
	double CycleCounterEndTimeMS;

	/** Index of this node in the data provider's collection. */
	/*const*/ uint32 SampleIndex;

	/** Index of the frame the this node belongs to. */
	const int32 FrameIndex;

	/** Initializes thread root node. */
	FProfilerStackNode( int32 InFrameIndex )
		: StatName( FStatConstants::NAME_ThreadRoot )
		, LongName( FStatConstants::NAME_ThreadRoot )
		, Parent( nullptr )
		, CyclesStart( 0 )
		, CyclesEnd( 0 )
		, CycleCounterStartTimeMS( 0.0f )
		, CycleCounterEndTimeMS( 0.0f )
		, SampleIndex( 0 )
		, FrameIndex( InFrameIndex )
	{}

	/** Initializes children node. */
	FProfilerStackNode( FProfilerStackNode* InParent, const FStatMessage& StatMessage, uint32 InSampleIndex, int32 InFrameIndex )
		: StatName( StatMessage.NameAndInfo.GetShortName() )
		, LongName( StatMessage.NameAndInfo.GetRawName() )
		, Parent( InParent )
		, CyclesStart( StatMessage.GetValue_int64() )
		, CyclesEnd( 0 )
		, CycleCounterStartTimeMS( 0.0f )
		, CycleCounterEndTimeMS( 0.0f )
		, SampleIndex( InSampleIndex )
		, FrameIndex( InFrameIndex )
	{}

	~FProfilerStackNode()
	{
		for( const auto& Child : Children )
		{
			delete Child;
		}
	}

	double GetDurationMS() const
	{
		return CycleCounterEndTimeMS - CycleCounterStartTimeMS;
	}

	/** Calculates allocated size by all stacks node, only valid for the thread root node. */
	int64 GetAllocatedSize() const
	{
		int64 AllocatedSize = sizeof(*this) + Children.GetAllocatedSize();
		for( const auto& StackNode : Children )
		{
			AllocatedSize += StackNode->GetAllocatedSize();
		}
		return AllocatedSize;
	}

	void AdjustCycleCounters( double CycleCounterAdjustmentMS )
	{
		CycleCounterStartTimeMS -= CycleCounterAdjustmentMS;
		CycleCounterEndTimeMS -= CycleCounterAdjustmentMS;
		for( const auto& Child : Children )
		{
			Child->AdjustCycleCounters( CycleCounterAdjustmentMS );
		}
	}
};


/** Profiler frame. */
struct FProfilerFrame
{
protected:
	enum
	{
		STACKNODE_INVALID = 0,
		STACKNODE_VALID = 1,
	};

public:
	/** Root node. */
	// #Profiler: 2014-04-25 Replace with TIndirectArray<FProfilerStackNode>??
	FProfilerStackNode* Root;

	/** Thread times in milliseconds for this frame. */
	TMap<uint32, float> ThreadTimesMS;

	/** Target frame as captured by the stats system. */
	int64 TargetFrame;

	/** Frame time for this frame. */
	double FrameTimeMS;

	/** How many milliseconds have passed from the beginning. */
	double ElapsedTimeMS;

	/**
	 *	Last time this frame has been accessed.
	 *	Used by the profiler's GC to remove 'idle' profiler frames.
	 *	Used only if working under specified memory constraint.
	 */
	double LastAccessTime;

	// #Profiler: 2014-04-22 Non-frame stats like accumulator, counters, memory etc?

	/**
	*	Indicates whether this profiler frame is in the memory.
	*	This is set by one thread and accessed by another thread, there is no thread contention.
	*/
	FThreadSafeCounter AccessLock;

	FProfilerFrame( int64 InTargetFrame, double InFrameTimeMS, double InElapsedTimeMS )
		: Root( new FProfilerStackNode( (int32)(InTargetFrame&MAX_int32) ) )
		, TargetFrame( InTargetFrame )
		, FrameTimeMS( InFrameTimeMS )
		, ElapsedTimeMS( InElapsedTimeMS )
		, LastAccessTime( FPlatformTime::Seconds() )
		, AccessLock( STACKNODE_INVALID )
	{}

	~FProfilerFrame()
	{
		FreeMemory();
	}

	void AddChild( FProfilerStackNode* ProfilerStackNode )
	{
		Root->Children.Add( ProfilerStackNode );
	}

	void SortChildren()
	{
		/**
		*	Sorts thread nodes to be in a particular order.
		*	GameThread, RenderThread.
		*/
		struct FThreadSort
		{
			FORCEINLINE bool operator()( const FProfilerStackNode& A, const FProfilerStackNode& B ) const
			{
				if( A.StatName == NAME_GameThread && B.StatName == NAME_RenderThread )
				{
					return true;
				}
				return false;
			}
		};

		Root->Children.Sort( FThreadSort() );
	}

	void MarkAsValid()
	{
		const int32 OldLock = AccessLock.Set( STACKNODE_VALID );
		check( OldLock == STACKNODE_INVALID );
	}

	void MarkAsInvalid()
	{
		const int32 OldLock = AccessLock.Set( STACKNODE_INVALID );
		check( OldLock == STACKNODE_VALID );
	}

	bool IsValid() const
	{
		return AccessLock.GetValue() == STACKNODE_VALID;
	}

	int64 GetAllocatedSize() const
	{
		return ThreadTimesMS.GetAllocatedSize() + ( Root ? Root->GetAllocatedSize() : 0 );
	}

	/** Frees most of the memory allocated by this profiler frame. */
	void FreeMemory()
	{
		delete Root;
		Root = nullptr;

		ThreadTimesMS.Empty();
	}
};

/** Contains all processed profiler's frames. */
// #Profiler: 2014-04-22 Array frames to real frames conversion.
// #Profiler: 2014-04-22 Can be done fully lock free and thread safe, but later.
struct FProfilerStream
{
protected:

	/** History frames collected so far or read from the file. */
	// #Profiler: 2014-04-24 TSharedRef<THREAD_SAFE> ?
	TChunkedArray<FProfilerFrame*> Frames;

	/** Each element in this array stores the frame time, accessed by a frame index, in millisecond. */
	TArray<double> FrameTimesMS;

	/** Each element in this array stores the total frame time, accessed by a frame index, in millisecond. */
	TArray<double> ElapsedFrameTimesMS;

	/** Thread FNames that have been visible so far. */
	TSet<FName> ThreadIDs;
	
	/** Critical section. */
	mutable FCriticalSection CriticalSection;


public:
	void AddProfilerFrame( int64 TargetFrame, FProfilerFrame* ProfilerFrame )
	{
		FScopeLock Lock( &CriticalSection );
		Frames.AddElement( ProfilerFrame );

		FrameTimesMS.Add( ProfilerFrame->FrameTimeMS );
		ElapsedFrameTimesMS.Add( ProfilerFrame->ElapsedTimeMS );

		//TArray<uint32> FrameThreadIDs;
		//ProfilerFrame->ThreadTimesMS.GenerateKeyArray( FrameThreadIDs );
		ThreadIDs.Add( NAME_GameThread );
		ThreadIDs.Add( NAME_RenderThread );
	}

	/**
	* @return a pointer to the profiler frame, once obtained can be used until end of the profiler session.
	*/
	FProfilerFrame* GetProfilerFrame( int32 FrameIndex ) const
	{
		FScopeLock Lock( &CriticalSection );
		return Frames[FrameIndex];
	}

	/**
	 * @return frames indices, where X is the start frame index, Y is the end frame index
	 */
	const FIntPoint GetFramesIndicesForTimeRange( const double StartTimeMS, const double EndTimeMS ) const
	{
		FScopeLock Lock( &CriticalSection );

		// Find the start frame index when the start time is less or equal.
		const int32 StartFrameIndex = FBinaryFindIndex::LessEqual( ElapsedFrameTimesMS, StartTimeMS );
		const int32 EndFrameIndex = FBinaryFindIndex::GreaterEqual( ElapsedFrameTimesMS, EndTimeMS, StartFrameIndex );
		
		return FIntPoint( StartFrameIndex, EndFrameIndex );
	}

	int64 GetAllocatedSize() const
	{
		FScopeLock Lock( &CriticalSection );
		int64 AllocatedSize = 0;

		for( int32 FrameIndex = 0; FrameIndex < Frames.Num(); FrameIndex++ )
		{
			const FProfilerFrame* ProfilerFrame = Frames[FrameIndex];
			if( ProfilerFrame->IsValid() )
			{
				AllocatedSize += ProfilerFrame->GetAllocatedSize();
			}
		}

		return AllocatedSize;
	}

	/** @return number of frames that have been collected so far. */
	int32 GetNumFrames() const
	{
		FScopeLock Lock( &CriticalSection );
		return Frames.Num();
	}

	/** @return the elapsed time for all collected frames. */
	double GetElapsedTime() const
	{
		FScopeLock Lock( &CriticalSection );
		return ElapsedFrameTimesMS[ElapsedFrameTimesMS.Num() - 1];
	}

	/** @return the frame duration for the specified frame, in milliseconds. */
	const double GetFrameTimeMS( const int32 InFrameIndex ) const
	{
		FScopeLock Lock( &CriticalSection );
		return FrameTimesMS[InFrameIndex];
	}

	/** @return the elapsed time for the specified frame, in milliseconds. */
	const double GetElapsedFrameTimeMS( const int32 InFrameIndex ) const
	{
		FScopeLock Lock( &CriticalSection );
		return ElapsedFrameTimesMS[InFrameIndex];
	}

	void AdjustCycleCounters( double CycleCounterAdjustmentMS )
	{
		FScopeLock Lock( &CriticalSection );
		for( int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex )
		{
			FProfilerStackNode* RootNode = Frames[FrameIndex]->Root;
			RootNode->AdjustCycleCounters( CycleCounterAdjustmentMS );
		}
	}

	/**
	 * @return peak number of threads
	 */
	int32 GetNumThreads() const
	{
		FScopeLock Lock( &CriticalSection );
		return ThreadIDs.Num();
	}
};

// #Profiler: 2014-04-23 ProfilerFrameLOD?

/**
*	Profiler UI stack node.
*	Similar to the profiler stack node, but contains data prepared and optimized for the UI
*/
struct FProfilerUIStackNode
{
	enum 
	{
		/** Indicates that the nodes is the thread node and shouldn't be displayed. */
		THREAD_NODE_INDEX = -1,
	};

// 	FProfilerUIStackNode()
// 		: CycleCountersStartTimeMS( 0.0f )
// 		, CycleCountersEndTimeMS( 0.0f )
// 		, PositionXPx( 0.0f )
// 		, PositionY( 0.0f )
// 		, Width( 0.0f )
// 		, GlobalNodeDepth( THREAD_NODE_INDEX )
// 		, ThreadNodeDepth( 0 )
// 		, ThreadIndex( THREAD_NODE_INDEX )
// 		, bIsCombined( false )
// 	{}

	FProfilerUIStackNode( const FProfilerStackNode* ProfilerStackNode, int32 InGlobalNodeDepth, int32 InThreadIndex, int32 InFrameIndex )
		: StatName( ProfilerStackNode->StatName )
		, LongName( ProfilerStackNode->LongName )
		, CycleCountersStartTimeMS( ProfilerStackNode->CycleCounterStartTimeMS )
		, CycleCountersEndTimeMS( ProfilerStackNode->CycleCounterEndTimeMS )
		, PositionXPx( 0.0f )
		, PositionY( 0.0f )
		, WidthPx( 0.0f )
		, GlobalNodeDepth( InGlobalNodeDepth )
		, ThreadNodeDepth( 0 )
		, ThreadIndex( InThreadIndex )
		, FrameIndex( InFrameIndex )
		, bIsCombined( false )
		, bIsCulled( false )
	{
		OriginalStackNodes.Add( ProfilerStackNode );
	}

	FProfilerUIStackNode( const TArray<const FProfilerStackNode*>& ProfilerStackNodes, int32 NumStackNodes, int32 InGlobalNodeDepth, int32 InThreadIndex, int32 InFrameIndex )
		: StatName( *FString::Printf( TEXT( "[%i]" ), NumStackNodes ) )
		, LongName( *FString::Printf( TEXT( "Combined %i items" ), NumStackNodes ) )
		, CycleCountersStartTimeMS( ProfilerStackNodes[0]->CycleCounterStartTimeMS )
		, CycleCountersEndTimeMS( ProfilerStackNodes[NumStackNodes - 1]->CycleCounterEndTimeMS )
		, PositionXPx( 0.0f )
		, PositionY( 0.0f )
		, WidthPx( 0.0f )
		, GlobalNodeDepth( InGlobalNodeDepth )
		, ThreadNodeDepth( 0 )
		, ThreadIndex( InThreadIndex )
		, FrameIndex( InFrameIndex )
		, bIsCombined( true )
		, bIsCulled( false )
	{
		OriginalStackNodes.Append( ProfilerStackNodes );
	}

	~FProfilerUIStackNode()
	{
	}
	
	void InitializeUIData( const double NumMillisecondsPerWindow, const double NumPixelsPerMillisecond, const double NumMillisecondsPerSample )
	{
		const double DurationMS = GetDurationMS();
		const double NumPixels = DurationMS*NumPixelsPerMillisecond;
		WidthPx = NumPixels;

		PositionXPx = CycleCountersStartTimeMS*NumPixelsPerMillisecond;
		PositionY = (double)GlobalNodeDepth;
	}

	void MarkAsCulled()
	{
		bIsCulled = true;
	}

	double GetDurationMS() const
	{
		return CycleCountersEndTimeMS - CycleCountersStartTimeMS;
	}

	FVector2D GetLocalPosition( double PositionXOffsetPx, double PositionYOffset ) const
	{
		return FVector2D( PositionXPx - PositionXOffsetPx, PositionY - PositionYOffset );
	}

	bool IsVisible() const
	{
		return true;
	}

	/**
	 *	Original stack nodes used to generate this UI stack node.
	 *	Useful if this node is a combined stack node.
	 */
	TArray<const FProfilerStackNode*> OriginalStackNodes;

	TIndirectArray<FProfilerUIStackNode> Children;

	/** Short name. */
	FName StatName;

	/** Long name, short name, stat description, group name. */
	FName LongName;

	/** Time range of the stack node. */
	double CycleCountersStartTimeMS;
	double CycleCountersEndTimeMS;

	/** UI Data. */

	/** Position of the stack node, absolute position, needs to be converted to the local before rendering. */
	double PositionXPx;
	double PositionY;

	/** Width of the stack node, in pixels. */
	double WidthPx;

	/** Depth of this node, in the global scope. */
	int32 GlobalNodeDepth;

	/** Depth of this node, in the thread scope. */
	int32 ThreadNodeDepth;

	/** Thread index of this node. */
	int32 ThreadIndex;

	/** Index of the frame the this node belongs to. */
	int32 FrameIndex;

	/**
	 *	Whether this UI stack node has been combined, if this is true we stop processing here.
	 *	Mouse processing is disabled, and there is no tooltip displayed.
	 *	The user needs to zoom-in to see more details.
	 */
	bool bIsCombined;

	/**
	 *	True means that the UI stack node has deeper call stack, but can't be displayed due to UI limitation.
	 *	Culled nodes are rendered with a special marker to indicate that there are more nodes.
	 */
	bool bIsCulled;

	/** Access lock. */
	FThreadSafeCounter AccessLock;
};


struct FProfilerUIStream
{
	enum
	{
		/**
		 *	Default number of rows displaying cycle counters.
		 *	Read it as the call stack depth.
		 */
		// #Profiler: 2014-04-25 This probably should be the max seen so far for each thread.
		DEFAULT_VISIBLE_THREAD_DEPTH = 16,
	};

	TIndirectArray<FProfilerUIStackNode> ThreadNodes;

	TArray< TArray<const FProfilerUIStackNode*> > LinearRowsOfNodes;

	void GenerateUIStream( const FProfilerStream& ProfilerStream, const double StartTimeMS, const double EndTimeMS, const double ZoomFactorX, const double NumMillisecondsPerWindow, const double NumPixelsPerMillisecond, const double NumMillisecondsPerSample );

protected:
	void CombineOrSet( FProfilerUIStackNode* ParentUIStackNode, const FProfilerStackNode& ProfilerStackNode, int32 GlobalNodeDepth, const double NumMillisecondsPerWindow, const double NumPixelsPerMillisecond, const double NumMillisecondsPerSample );


	/** Converts a tree representation into a flat-array-based. */
	void LinearizeStream()
	{
		//SCOPE_LOG_TIME_FUNC();

		LinearRowsOfNodes.SetNum( ThreadNodes.Num()*DEFAULT_VISIBLE_THREAD_DEPTH );
		for( auto& RowOfNodes : LinearRowsOfNodes )
		{
			RowOfNodes.Reset();
		}

		for( const auto& ThreadNode : ThreadNodes )
		{
			LinearizeStream_Recursively( &ThreadNode );
		}
	}

	void LinearizeStream_Recursively( const FProfilerUIStackNode* UIStackNode )
	{
		if( UIStackNode->GlobalNodeDepth != FProfilerUIStackNode::THREAD_NODE_INDEX )
		{
			LinearRowsOfNodes[UIStackNode->GlobalNodeDepth].Add( UIStackNode );
		}

		for( const auto& UIStackNodeChild : UIStackNode->Children )
		{
			LinearizeStream_Recursively(&UIStackNodeChild);
		}
	}
};

