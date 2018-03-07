// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerStream.h"


/*-----------------------------------------------------------------------------
	FProfilerUIStream
-----------------------------------------------------------------------------*/

void FProfilerUIStream::GenerateUIStream( const FProfilerStream& ProfilerStream, const double StartTimeMS, const double EndTimeMS, const double ZoomFactorX, const double NumMillisecondsPerWindow, const double NumPixelsPerMillisecond, const double NumMillisecondsPerSample )
{
	//SCOPE_LOG_TIME_FUNC();

	// Just copy the data for now.
	const FIntPoint FramesIndices = ProfilerStream.GetFramesIndicesForTimeRange( StartTimeMS, EndTimeMS );
	const double FrameTimeRangeMS = EndTimeMS - StartTimeMS;

	const double WindowWidth = NumMillisecondsPerWindow*NumPixelsPerMillisecond;

	// Preallocate nodes for threads.
	ThreadNodes.Reset(ProfilerStream.GetNumThreads());

	if( true || NumMillisecondsPerWindow >= FrameTimeRangeMS )
	{
		// Just copy the data.
		const int32 NumFrames = ProfilerStream.GetNumFrames();
		const int32 MaxFrameIndex = FMath::Min( FramesIndices.Y + 1, NumFrames - 1 );

		for( int32 FrameIndex = FramesIndices.X; FrameIndex < MaxFrameIndex; ++FrameIndex )
		{
			const FProfilerFrame* ProfilerFrame = ProfilerStream.GetProfilerFrame( FrameIndex );

			// Thread nodes.
			const int32 NumThreads = ProfilerFrame->Root->Children.Num();
			for( int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex )
			{
				FProfilerUIStackNode* ThreadUIStackNode = new FProfilerUIStackNode( ProfilerFrame->Root->Children[ThreadIndex], FProfilerUIStackNode::THREAD_NODE_INDEX, FProfilerUIStackNode::THREAD_NODE_INDEX, FrameIndex );
				ThreadUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );

				ThreadNodes.Add( ThreadUIStackNode );

				CombineOrSet( ThreadUIStackNode, *ProfilerFrame->Root->Children[ThreadIndex], ThreadIndex*DEFAULT_VISIBLE_THREAD_DEPTH, NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
			}
		}
	}
	else
	{
		// #Profiler: 2014-04-28 Combining profiler frames doesn't make sense at this moment
		// At this scale we may want to switch into line graph or something?
	}

	// #Profiler: 2014-04-25 MAX_THREAD_DEPTH;
	LinearizeStream();
}

void FProfilerUIStream::CombineOrSet( FProfilerUIStackNode* ParentUIStackNode, const FProfilerStackNode& ProfilerStackNode, int32 GlobalNodeDepth, const double NumMillisecondsPerWindow, const double NumPixelsPerMillisecond, const double NumMillisecondsPerSample )
{
	const int32 ThreadIndex = GlobalNodeDepth / DEFAULT_VISIBLE_THREAD_DEPTH;
	const int32 ThreadNodeDepth = GlobalNodeDepth % DEFAULT_VISIBLE_THREAD_DEPTH;
	const int32 NumChildren = ProfilerStackNode.Children.Num();
	const int32 FrameIndex = ProfilerStackNode.FrameIndex;

	if( NumChildren > 0 )
	{
		FProfilerStackNode* FirstProfilerChildNode = nullptr;
		bool bLookingForChildToCombine = false;

		TArray<const FProfilerStackNode*> OriginalStackNodes;

		for( int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			if( bLookingForChildToCombine )
			{
				const FProfilerStackNode* NextProfilerChildNode = ProfilerStackNode.Children[ChildIndex];
				const bool bNeedsToBeCombined = NextProfilerChildNode->GetDurationMS() < NumMillisecondsPerSample;

				// The next node can be displayed in the UI, add it and the previous one as the combined. 
				if( !bNeedsToBeCombined )
				{
					// Combine child nodes into one node.
					{
						FProfilerUIStackNode* ProfilerUIStackNode = new FProfilerUIStackNode( OriginalStackNodes, OriginalStackNodes.Num(), GlobalNodeDepth, ThreadIndex, FrameIndex );
						ProfilerUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
						ParentUIStackNode->Children.Add( ProfilerUIStackNode );
						bLookingForChildToCombine = false;
						OriginalStackNodes.Reset();
					}

					// Add the next node as a single node.
					{
						FProfilerUIStackNode* ProfilerUIStackNode = new FProfilerUIStackNode( NextProfilerChildNode, GlobalNodeDepth, ThreadIndex, FrameIndex );
						ProfilerUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
						ParentUIStackNode->Children.Add( ProfilerUIStackNode );

						// Check the call stack depth.
						if( ThreadNodeDepth != DEFAULT_VISIBLE_THREAD_DEPTH - 1 )
						{
							CombineOrSet( ProfilerUIStackNode, *NextProfilerChildNode, GlobalNodeDepth + 1, NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
						}
						else
						{
							// Mark this node as culled.
							ProfilerUIStackNode->MarkAsCulled();
						}
					}

					continue;
				}

				// We need to combine a few child nodes into one node.
				const double CombinedDurationMS = NextProfilerChildNode->CycleCounterEndTimeMS - FirstProfilerChildNode->CycleCounterStartTimeMS;

				

				OriginalStackNodes.Add( NextProfilerChildNode );
				const bool bCanBeCombined = CombinedDurationMS > NumMillisecondsPerSample;
				if( bCanBeCombined )
				{
					// Combine child nodes into one node.
					FProfilerUIStackNode* ProfilerUIStackNode = new FProfilerUIStackNode( OriginalStackNodes, OriginalStackNodes.Num(), GlobalNodeDepth, ThreadIndex, FrameIndex );
					ProfilerUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
					ParentUIStackNode->Children.Add( ProfilerUIStackNode );
					bLookingForChildToCombine = false;
					OriginalStackNodes.Reset();
				}

				continue;
			}

			FirstProfilerChildNode = ProfilerStackNode.Children[ChildIndex];
			const bool bNeedsToBeCombined = FirstProfilerChildNode->GetDurationMS() < NumMillisecondsPerSample;

			if( !bNeedsToBeCombined && !bLookingForChildToCombine )
			{
				// We have a sample that can be displayed in the UI, add it.
				FProfilerUIStackNode* ProfilerUIStackNode = new FProfilerUIStackNode( FirstProfilerChildNode, GlobalNodeDepth, ThreadIndex, FrameIndex );
				ProfilerUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
				ParentUIStackNode->Children.Add( ProfilerUIStackNode );

				// Check the call stack depth.
				if( ThreadNodeDepth != DEFAULT_VISIBLE_THREAD_DEPTH - 1 )
				{
					CombineOrSet( ProfilerUIStackNode, *FirstProfilerChildNode, GlobalNodeDepth + 1, NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
				}
				else
				{
					// Mark this node as culled.
					ProfilerUIStackNode->MarkAsCulled();
				}
			}
			else
			{
				bLookingForChildToCombine = true;			
				OriginalStackNodes.Add( FirstProfilerChildNode );
			}
		}

		// We are out of the loop, but still need to combine children even if not visible.
		if( bLookingForChildToCombine )
		{
			// Combine child nodes into one node.
			{
				FProfilerUIStackNode* ProfilerUIStackNode = new FProfilerUIStackNode( OriginalStackNodes, OriginalStackNodes.Num(), GlobalNodeDepth, ThreadIndex, FrameIndex );
				ProfilerUIStackNode->InitializeUIData( NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
				ParentUIStackNode->Children.Add( ProfilerUIStackNode );
				bLookingForChildToCombine = false;
				OriginalStackNodes.Reset();
			}
		}
	}
}
