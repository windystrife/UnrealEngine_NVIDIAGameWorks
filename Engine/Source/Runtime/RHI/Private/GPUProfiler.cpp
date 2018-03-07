// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUProfiler.h: Hierarchical GPU Profiler Implementation.
=============================================================================*/

#include "GPUProfiler.h"
#include "Misc/WildcardString.h"

#if !UE_BUILD_SHIPPING
#include "VisualizerEvents.h"
#include "STaskGraph.h"
#include "Modules/ModuleManager.h"
#endif

#define LOCTEXT_NAMESPACE "GpuProfiler"

static TAutoConsoleVariable<FString> GProfileGPUPatternCVar(
	TEXT("r.ProfileGPU.Pattern"),
	TEXT("*"),
	TEXT("Allows to filter the entries when using ProfileGPU, the pattern match is case sensitive.\n")
	TEXT("'*' can be used in the end to get all entries starting with the string.\n")
	TEXT("    '*' without any leading characters disables the pattern matching and uses a time threshold instead (default).\n")
	TEXT("'?' allows to ignore one character.\n")
	TEXT("e.g. AmbientOcclusionSetup, AmbientOcclusion*, Ambient???lusion*, *"),
	ECVF_Default);

static TAutoConsoleVariable<FString> GProfileGPURootCVar(
	TEXT("r.ProfileGPU.Root"),
	TEXT("*"),
	TEXT("Allows to filter the tree when using ProfileGPU, the pattern match is case sensitive."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GProfileShowEventHistogram(
	TEXT("r.ProfileGPU.ShowEventHistogram"),
	0,
	TEXT("Whether the event histogram should be shown."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GProfileGPUShowEvents(
	TEXT("r.ProfileGPU.ShowLeafEvents"),
	0,
	TEXT("Allows profileGPU to display event-only leaf nodes with no draws associated."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GProfileGPUTransitions(
	TEXT("r.ProfileGPU.ShowTransitions"),
	0,
	TEXT("Allows profileGPU to display resource transition events."),
	ECVF_Default);

// Should we print a summary at the end?
static TAutoConsoleVariable<int32> GProfilePrintAssetSummary(
	TEXT("r.ProfileGPU.PrintAssetSummary"),
	0,
	TEXT("Should we print a summary split by asset (r.ShowMaterialDrawEvents is strongly recommended as well).\n"),
	ECVF_Default);

// Should we print a summary at the end?
static TAutoConsoleVariable<FString> GProfileAssetSummaryCallOuts(
	TEXT("r.ProfileGPU.AssetSummaryCallOuts"),
	TEXT(""),
	TEXT("Comma separated list of substrings that deserve special mention in the final summary (e.g., \"LOD,HeroName\"\n")
	TEXT("r.ProfileGPU.PrintAssetSummary must be true to enable this feature"),
	ECVF_Default);

enum class EGPUProfileSortMode
{
	EChronological,
	ETimeElapsed,
	ENumPrims,
	ENumVerts,
	EMax
};

static TAutoConsoleVariable<int32> GProfileGPUSort(
	TEXT("r.ProfileGPU.Sort"),	
	0,
	TEXT("Sorts the TTY Dump independently at each level of the tree in various modes.\n")
	TEXT("0 : Chronological\n")
	TEXT("1 : By time elapsed\n")
	TEXT("2 : By number of prims\n")
	TEXT("3 : By number of verts\n"),
	ECVF_Default);

struct FNodeStatsCompare
{
	/** Sorts nodes by descending durations. */
	FORCEINLINE bool operator()( const FGPUProfilerEventNodeStats& A, const FGPUProfilerEventNodeStats& B ) const
	{
		return B.TimingResult < A.TimingResult;
	}
};

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
static bool IsVolumetricLightingNode(const FString& NodeName)
{
	if (NodeName == FString(TEXT("VolumetricLightingBeginAccumulation"))
		|| NodeName == FString(TEXT("VolumetricLightingRenderVolume"))
		|| NodeName == FString(TEXT("VolumetricLightingEndAccumulation"))
		|| NodeName == FString(TEXT("VolumetricLightingApplyLighting")))
	{
		return true;
	}

	return false;
}
#endif
// NVCHANGE_END: Nvidia Volumetric Lighting

/** Recursively generates a histogram of nodes and stores their timing in TimingResult. */
static void GatherStatsEventNode(FGPUProfilerEventNode* Node, int32 Depth, TMap<FString, FGPUProfilerEventNodeStats>& EventHistogram)
{
	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	// Volumetirc Lighting used external API to render, its NumDraws always 0
	if (IsVolumetricLightingNode(Node->Name) || Node->NumDraws > 0 || Node->Children.Num() > 0)
#else
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
#endif
		// NVCHANGE_END: Nvidia Volumetric Lighting
	{
		Node->TimingResult = Node->GetTiming() * 1000.0f;
		Node->NumTotalDraws = Node->NumDraws;
		Node->NumTotalPrimitives = Node->NumPrimitives;
		Node->NumTotalVertices = Node->NumVertices;

		FGPUProfilerEventNode* Parent = Node->Parent;		
		while (Parent)
		{
			Parent->NumTotalDraws += Node->NumDraws;
			Parent->NumTotalPrimitives += Node->NumPrimitives;
			Parent->NumTotalVertices += Node->NumVertices;

			Parent = Parent->Parent;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			// Traverse children
			GatherStatsEventNode(Node->Children[ChildIndex], Depth + 1, EventHistogram);
		}

		FGPUProfilerEventNodeStats* FoundHistogramBucket = EventHistogram.Find(Node->Name);
		if (FoundHistogramBucket)
		{
			FoundHistogramBucket->NumDraws += Node->NumTotalDraws;
			FoundHistogramBucket->NumPrimitives += Node->NumTotalPrimitives;
			FoundHistogramBucket->NumVertices += Node->NumTotalVertices;
			FoundHistogramBucket->TimingResult += Node->TimingResult;
			FoundHistogramBucket->NumEvents++;
		}
		else
		{
			FGPUProfilerEventNodeStats NewNodeStats;
			NewNodeStats.NumDraws = Node->NumTotalDraws;
			NewNodeStats.NumPrimitives = Node->NumTotalPrimitives;
			NewNodeStats.NumVertices = Node->NumTotalVertices;
			NewNodeStats.TimingResult = Node->TimingResult;
			NewNodeStats.NumEvents = 1;
			EventHistogram.Add(Node->Name, NewNodeStats);
		}
	}
}

struct FGPUProfileInfoPair
{
	int64 Triangles;
	int32 DrawCalls;

	FGPUProfileInfoPair()
		: Triangles(0)
		, DrawCalls(0)
	{
	}

	void AddDraw(int64 InTriangleCount)
	{
		Triangles += InTriangleCount;
		++DrawCalls;
	}
};

struct FGPUProfileStatSummary
{
	TMap<FString, FGPUProfileInfoPair> TrianglesPerMaterial;
	TMap<FString, FGPUProfileInfoPair> TrianglesPerMesh;
	TMap<FString, FGPUProfileInfoPair> TrianglesPerNonMesh;

	int32 TotalNumNodes;
	int32 TotalNumDraws;

	bool bGatherSummaryStats;
	bool bDumpEventLeafNodes;

	FGPUProfileStatSummary()
		: TotalNumNodes(0)
		, TotalNumDraws(0)
		, bGatherSummaryStats(false)
		, bDumpEventLeafNodes(false)
	{
		bDumpEventLeafNodes = GProfileGPUShowEvents.GetValueOnRenderThread() != 0;
		bGatherSummaryStats = GProfilePrintAssetSummary.GetValueOnRenderThread() != 0;
	}

	void ProcessMatch(FGPUProfilerEventNode* Node)
	{
		if (bGatherSummaryStats && (Node->NumTotalPrimitives > 0) && (Node->NumTotalVertices > 0) && (Node->Children.Num() == 0))
		{
			FString MaterialPart;
			FString AssetPart;
			if (Node->Name.Split(TEXT(" "), &MaterialPart, &AssetPart, ESearchCase::CaseSensitive))
			{
				TrianglesPerMaterial.FindOrAdd(MaterialPart).AddDraw(Node->NumTotalPrimitives);
				TrianglesPerMesh.FindOrAdd(AssetPart).AddDraw(Node->NumTotalPrimitives);
			}
			else
			{
				TrianglesPerNonMesh.FindOrAdd(Node->Name).AddDraw(Node->NumTotalPrimitives);
			}
		}
	}

	void PrintSummary()
	{
		UE_LOG(LogRHI, Log, TEXT("Total Nodes %u Draws %u"), TotalNumNodes, TotalNumDraws);
		UE_LOG(LogRHI, Log, TEXT(""));
		UE_LOG(LogRHI, Log, TEXT(""));

		if (bGatherSummaryStats)
		{
			// Sort the lists and print them out
			TrianglesPerMesh.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MeshList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerMesh)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%d,%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			TrianglesPerMaterial.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MaterialList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerMaterial)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%d,%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			TrianglesPerNonMesh.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MiscList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerNonMesh)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%d,%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			// See if we want to call out any particularly interesting matches
			TArray<FString> InterestingSubstrings;
			GProfileAssetSummaryCallOuts.GetValueOnRenderThread().ParseIntoArray(InterestingSubstrings, TEXT(","), true);

			if (InterestingSubstrings.Num() > 0)
			{
				UE_LOG(LogRHI, Log, TEXT(""));
				UE_LOG(LogRHI, Log, TEXT("Information about specified mesh substring matches (r.ProfileGPU.AssetSummaryCallOuts)"));
				for (const FString& InterestingSubstring : InterestingSubstrings)
				{
					int32 InterestingNumDraws = 0;
					int64 InterestingNumTriangles = 0;

					for (auto& Pair : TrianglesPerMesh)
					{
						if (Pair.Key.Contains(InterestingSubstring))
						{
							InterestingNumDraws += Pair.Value.DrawCalls;
							InterestingNumTriangles += Pair.Value.Triangles;
						}
					}

					UE_LOG(LogRHI, Log, TEXT("Matching '%s': %d draw calls, with %d tris (%.2f M)"), *InterestingSubstring, InterestingNumDraws, InterestingNumTriangles, InterestingNumTriangles * 1e-6);
				}
				UE_LOG(LogRHI, Log, TEXT(""));
			}
		}
	}
};

/** Recursively dumps stats for each node with a depth first traversal. */
static void DumpStatsEventNode(FGPUProfilerEventNode* Node, float RootResult, int32 Depth, const FWildcardString& WildcardFilter, bool bParentMatchedFilter, FGPUProfileStatSummary& Summary)
{
	Summary.TotalNumNodes++;
	if (Node->NumDraws > 0 || Node->Children.Num() > 0 || Summary.bDumpEventLeafNodes)
	{
		Summary.TotalNumDraws += Node->NumDraws;
		// Percent that this node was of the total frame time
		const float Percent = Node->TimingResult * 100.0f / (RootResult * 1000.0f);

		const int32 EffectiveDepth = FMath::Max(Depth - 1, 0);

		const bool bMatchesFilter = bParentMatchedFilter || WildcardFilter.IsMatch(Node->Name);

		if (bMatchesFilter)
		{
			FString Extra;
			
			if(Node->TimingResult >= 0.1f && Node->NumVertices * Node->NumDraws > 100)
			{
				Extra = FString::Printf(TEXT(" %.0f prims/ms %.0f verts/ms"),
					Node->NumPrimitives / Node->TimingResult,
					Node->NumVertices / Node->TimingResult);
			}

			// Print information about this node, padded to its depth in the tree
			UE_LOG(LogRHI, Log, TEXT("%s%4.1f%%%5.2fms   %s %u draws %u prims %u verts%s"), 
				*FString(TEXT("")).LeftPad(EffectiveDepth * 3), 
				Percent,
				Node->TimingResult,
				*Node->Name,
				Node->NumTotalDraws,
				Node->NumTotalPrimitives,
				Node->NumTotalVertices,
				*Extra
				);

			Summary.ProcessMatch(Node);
		}

		struct FCompareGPUProfileNode
		{
			EGPUProfileSortMode SortMode;
			FCompareGPUProfileNode(EGPUProfileSortMode InSortMode)
				: SortMode(InSortMode)
			{}
			FORCEINLINE bool operator()(const FGPUProfilerEventNode* A, const FGPUProfilerEventNode* B) const
			{
				switch (SortMode)
				{
					case EGPUProfileSortMode::ENumPrims:
						return B->NumTotalPrimitives < A->NumTotalPrimitives;
					case EGPUProfileSortMode::ENumVerts:
						return B->NumTotalVertices < A->NumTotalVertices;
					case EGPUProfileSortMode::ETimeElapsed:
					default:
						return B->TimingResult < A->TimingResult;
				}
			}
		};

		EGPUProfileSortMode SortMode = (EGPUProfileSortMode)FMath::Clamp(GProfileGPUSort.GetValueOnRenderThread(), 0, ((int32)EGPUProfileSortMode::EMax - 1));
		if (SortMode != EGPUProfileSortMode::EChronological)
		{
			Node->Children.Sort(FCompareGPUProfileNode(SortMode));
		}

		float TotalChildTime = 0;
		uint32 TotalChildDraws = 0;
		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			FGPUProfilerEventNode* ChildNode = Node->Children[ChildIndex];

			// Traverse children			
			const int32 PrevNumDraws = Summary.TotalNumDraws;
			DumpStatsEventNode(Node->Children[ChildIndex], RootResult, Depth + 1, WildcardFilter, bMatchesFilter, Summary);
			const int32 NumChildDraws = Summary.TotalNumDraws - PrevNumDraws;

			TotalChildTime += ChildNode->TimingResult;
			TotalChildDraws += NumChildDraws;
		}

		const float UnaccountedTime = FMath::Max(Node->TimingResult - TotalChildTime, 0.0f);
		const float UnaccountedPercent = UnaccountedTime * 100.0f / (RootResult * 1000.0f);

		// Add an 'Other Children' node if necessary to show time spent in the current node that is not in any of its children
		if (bMatchesFilter && Node->Children.Num() > 0 && TotalChildDraws > 0 && (UnaccountedPercent > 2.0f || UnaccountedTime > .2f))
		{
			UE_LOG(LogRHI, Log, TEXT("%s%4.1f%%%5.2fms   Other Children"), 
				*FString(TEXT("")).LeftPad((EffectiveDepth + 1) * 3), 
				UnaccountedPercent,
				UnaccountedTime);
		}
	}
}

#if !UE_BUILD_SHIPPING

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerDataRecursively( const TRefCountPtr< class FGPUProfilerEventNode >& InNode, TSharedPtr< FVisualizerEvent > InParentEvent, const double InStartTimeMs, const double InTotalTimeMs )
{
	TSharedPtr< FVisualizerEvent > VisualizerEvent( new FVisualizerEvent( InStartTimeMs / InTotalTimeMs, InNode->TimingResult / InTotalTimeMs, InNode->TimingResult, 0, InNode->Name ) );
	VisualizerEvent->ParentEvent = InParentEvent;

	double ChildStartTimeMs = InStartTimeMs;
	for( int32 ChildIndex = 0; ChildIndex < InNode->Children.Num(); ChildIndex++ )
	{
		TRefCountPtr< FGPUProfilerEventNode > ChildNode = InNode->Children[ ChildIndex ];
		TSharedPtr< FVisualizerEvent > ChildEvent = CreateVisualizerDataRecursively( ChildNode, VisualizerEvent, ChildStartTimeMs, InTotalTimeMs );
		VisualizerEvent->Children.Add( ChildEvent );

		ChildStartTimeMs += ChildNode->TimingResult;
	}

	return VisualizerEvent;
}

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerData( const TArray<TRefCountPtr<class FGPUProfilerEventNode> >& InProfileData )
{
	// Calculate total time first
	double TotalTimeMs = 0.0;
	for( int32 Index = 0; Index < InProfileData.Num(); ++Index )
	{
		TotalTimeMs += InProfileData[ Index ]->TimingResult;
	}
	
	// Assumption: InProfileData contains only one (root) element. Otherwise an extra FVisualizerEvent root event is required.
	TSharedPtr< FVisualizerEvent > DummyRoot;
	// Recursively create visualizer event data.
	TSharedPtr< FVisualizerEvent > StatEvents( CreateVisualizerDataRecursively( InProfileData[0], DummyRoot, 0.0, TotalTimeMs ) );
	return StatEvents;
}

#endif

void FGPUProfilerEventNodeFrame::DumpEventTree()
{
	if (EventTree.Num() > 0)
	{
		float RootResult = GetRootTimingResults();

		UE_LOG(LogRHI, Log, TEXT("Perf marker hierarchy, total GPU time %.2fms"), RootResult * 1000.0f);

		// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled
		FText VsyncEnabledWarningText = FText::GetEmpty();
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		if (CVSyncVar->GetInt() != 0 && !PlatformDisablesVSync())
		{
			VsyncEnabledWarningText = LOCTEXT("GpuProfileVsyncEnabledWarning",
				"WARNING: This GPU profile was captured with v-sync enabled.  V-sync wait time may show up in any bucket, and as a result the data in this profile may be skewed. Please profile with v-sync disabled to obtain the most accurate data.");
			UE_LOG(LogRHI, Log, TEXT("%s"), *(VsyncEnabledWarningText.ToString()));
		}

		LogDisjointQuery();

		TMap<FString, FGPUProfilerEventNodeStats> EventHistogram;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			GatherStatsEventNode(EventTree[BaseNodeIndex], 0, EventHistogram);
		}

		static IConsoleVariable* CVar2 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.Root"));
		FString RootWildcardString = CVar2->GetString(); 
		FWildcardString RootWildcard(RootWildcardString);

		FGPUProfileStatSummary Summary;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			DumpStatsEventNode(EventTree[BaseNodeIndex], RootResult, 0, RootWildcard, false, /*inout*/ Summary);
		}
		Summary.PrintSummary();

		const bool bShowHistogram = GProfileShowEventHistogram.GetValueOnRenderThread() != 0;

		if (RootWildcardString == TEXT("*") && bShowHistogram)
		{
			// Sort descending based on node duration
			EventHistogram.ValueSort( FNodeStatsCompare() );

			// Log stats about the node histogram
			UE_LOG(LogRHI, Log, TEXT("Node histogram %u buckets"), EventHistogram.Num());

			static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.Pattern"));

			// bad: reading on render thread but we don't support ECVF_RenderThreadSafe on strings yet
			// It's very unlikely to cause a problem as the cvar is only changes by the user.
			FString WildcardString = CVar->GetString(); 

			FGPUProfilerEventNodeStats Sum;

			const float ThresholdInMS = 5.0f;

			if(WildcardString == FString(TEXT("*")))
			{
				// disable Wildcard functionality
				WildcardString.Empty();
			}

			if(WildcardString.IsEmpty())
			{
				UE_LOG(LogRHI, Log, TEXT(" r.ProfileGPU.Pattern = '*' (using threshold of %g ms)"), ThresholdInMS);
			}
			else
			{
				UE_LOG(LogRHI, Log, TEXT(" r.ProfileGPU.Pattern = '%s' (not using time threshold)"), *WildcardString);
			}

			FWildcardString Wildcard(WildcardString);

			int32 NumNotShown = 0;
			for (TMap<FString, FGPUProfilerEventNodeStats>::TIterator It(EventHistogram); It; ++It)
			{
				const FGPUProfilerEventNodeStats& NodeStats = It.Value();

				bool bDump = NodeStats.TimingResult > RootResult * ThresholdInMS;

				if(!Wildcard.IsEmpty())
				{
					// if a Wildcard string was specified, we want to always dump all entries
					bDump = Wildcard.IsMatch(*It.Key());
				}

				if (bDump)
				{
					UE_LOG(LogRHI, Log, TEXT("   %.2fms   %s   Events %u   Draws %u"), NodeStats.TimingResult, *It.Key(), NodeStats.NumEvents, NodeStats.NumDraws);
					Sum += NodeStats;
				}
				else
				{
					NumNotShown++;
				}
			}

			UE_LOG(LogRHI, Log, TEXT("   Total %.2fms   Events %u   Draws %u,    %u buckets not shown"), 
				Sum.TimingResult, Sum.NumEvents, Sum.NumDraws, NumNotShown);
		}

#if !UE_BUILD_SHIPPING
		// Create and display profile visualizer data
		if (RHIConfig::ShouldShowProfilerAfterProfilingGPU())
		{

		// execute on main thread
			{
				struct FDisplayProfilerVisualizer
				{
					void Thread( TSharedPtr<FVisualizerEvent> InVisualizerData, const FText InVsyncEnabledWarningText )
					{
						static FName TaskGraphModule(TEXT("TaskGraph"));			
						if (FModuleManager::Get().IsModuleLoaded(TaskGraphModule))
						{
							IProfileVisualizerModule& ProfileVisualizer = FModuleManager::GetModuleChecked<IProfileVisualizerModule>(TaskGraphModule);
							// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled (otherwise InVsyncEnabledWarningText is empty)
							ProfileVisualizer.DisplayProfileVisualizer( InVisualizerData, TEXT("GPU"), InVsyncEnabledWarningText, FLinearColor::Red );
						}
					}
				} DisplayProfilerVisualizer;

				TSharedPtr<FVisualizerEvent> VisualizerData = CreateVisualizerData( EventTree );

				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DisplayProfilerVisualizer"),
					STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer,
					STATGROUP_TaskGraphTasks);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateRaw(&DisplayProfilerVisualizer, &FDisplayProfilerVisualizer::Thread, VisualizerData, VsyncEnabledWarningText),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer), nullptr, ENamedThreads::GameThread
				);
			}

			
		}
#endif
	}
}

void FGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	if (bTrackingEvents)
	{
		check(StackDepth >= 0);
		StackDepth++;

		check(IsInRenderingThread() || IsInRHIThread());
		if (CurrentEventNode)
		{
			// Add to the current node's children
			CurrentEventNode->Children.Add(CreateEventNode(Name, CurrentEventNode));
			CurrentEventNode = CurrentEventNode->Children.Last();
		}
		else
		{
			// Add a new root node to the tree
			CurrentEventNodeFrame->EventTree.Add(CreateEventNode(Name, NULL));
			CurrentEventNode = CurrentEventNodeFrame->EventTree.Last();
		}

		check(CurrentEventNode);
		// Start timing the current node
		CurrentEventNode->StartTiming();
	}
}

void FGPUProfiler::PopEvent()
{
	if (bTrackingEvents)
	{
		check(StackDepth >= 1);
		StackDepth--;

		check(CurrentEventNode && (IsInRenderingThread() || IsInRHIThread()));
		// Stop timing the current node and move one level up the tree
		CurrentEventNode->StopTiming();
		CurrentEventNode = CurrentEventNode->Parent;
	}
}

/** Whether GPU timing measurements are supported by the driver. */
bool FGPUTiming::GIsSupported = false;

/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
uint64 FGPUTiming::GTimingFrequency = 0;

/** Whether the static variables have been initialized. */
bool FGPUTiming::GAreGlobalsInitialized = false;



float FWindowedGPUTimer::GetElapsedAverage(FRHICommandListImmediate& RHICmdList, float &OutAvgTimeInSeconds)
{
	if (QueriesFinished < StartQueries.Num())
	{
		return 0.0f;
	}

	float TotalTime = 0;

	// Grab the queries in our window (specified number of frames old) and calculate total time as an average of the winow size
	//
	for (int32 i = StartQueries.Num() - WindowSize; i < StartQueries.Num(); i++)
	{
		uint64 StartTime, EndTime;
		bool StartSucceeded = RHICmdList.GetRenderQueryResult(StartQueries[i], StartTime, false);
		bool EndSucceeded = RHICmdList.GetRenderQueryResult(EndQueries[i], EndTime, false);

		// figure out what the failure rate of the queries is; they fail because the GPU hasn't finished them when we
		// try to get the data, so if the failure rate is too high, number of frames behind needs to be increased
		QueriesFailed += StartSucceeded&&EndSucceeded ? -1 : 1;
		QueriesFailed = FMath::Max<int32>(0, QueriesFailed);
		TotalTime += (EndTime - StartTime) / 1000000.0f;
	}

	float FailRate = static_cast<float>(QueriesFailed) / WindowSize;
	OutAvgTimeInSeconds = TotalTime / WindowSize;

	return FailRate;
}

#undef LOCTEXT_NAMESPACE
