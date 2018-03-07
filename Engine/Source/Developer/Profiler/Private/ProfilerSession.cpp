// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerSession.h"
#include "ProfilerFPSAnalyzer.h"
#include "ProfilerDataProvider.h"


/*-----------------------------------------------------------------------------
	FProfilerStat, FProfilerGroup
-----------------------------------------------------------------------------*/

FProfilerStat FProfilerStat::Default;
FProfilerGroup FProfilerGroup::Default;

FProfilerStat::FProfilerStat( const uint32 InStatID /*= 0 */ ) 
	: _Name( TEXT("(Stat-Default)") )
	, _OwningGroupPtr( FProfilerGroup::GetDefaultPtr() )
	, _ID( InStatID )
	, _Type( EProfilerSampleTypes::InvalidOrMax )
{}

/*-----------------------------------------------------------------------------
	FProfilerSession
-----------------------------------------------------------------------------*/

FProfilerSession::FProfilerSession( EProfilerSessionTypes InSessionType, const TSharedPtr<ISessionInstanceInfo>& InSessionInstanceInfo, FGuid InSessionInstanceID, FString InDataFilepath )
: bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataCurrent( nullptr )
, CreationTime( FDateTime::Now() )
, SessionType( InSessionType )
, SessionInstanceInfo( InSessionInstanceInfo )
, SessionInstanceID( InSessionInstanceID )
, DataFilepath( InDataFilepath )
, NumFrames( 0 )
, NumFramesProcessed( 0 )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 90 ) ) )
{}

FProfilerSession::FProfilerSession( const TSharedPtr<ISessionInstanceInfo>& InSessionInstanceInfo )
: bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataCurrent( nullptr )
, CreationTime( FDateTime::Now() )
, SessionType( EProfilerSessionTypes::Live )
, SessionInstanceInfo( InSessionInstanceInfo )
, SessionInstanceID( InSessionInstanceInfo->GetInstanceId() )
, DataFilepath( TEXT("") )
, NumFrames( 0 )
, NumFramesProcessed( 0 )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 90 ) ) )
{
	// Randomize creation time to test loading profiler captures with different creation time and different amount of data.
	CreationTime = FDateTime::Now() += FTimespan( 0, 0, FMath::RandRange( 2, 8 ) );
	OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick );
}

FProfilerSession::FProfilerSession( const FString& InDataFilepath )
: bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataCurrent( nullptr )
, CreationTime( FDateTime::Now() )
, SessionType( EProfilerSessionTypes::StatsFile )
, SessionInstanceInfo( nullptr )
, SessionInstanceID( FGuid::NewGuid() )
, DataFilepath( InDataFilepath.Replace( *FStatConstants::StatsFileExtension, TEXT( "" ) ) )
, NumFrames( 0 )
, NumFramesProcessed( 0 )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 90 ) ) )
{
	// Randomize creation time to test loading profiler captures with different creation time and different amount of data.
	CreationTime = FDateTime::Now() += FTimespan( 0, 0, FMath::RandRange( 2, 8 ) );
	OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick );
}

FProfilerSession::~FProfilerSession()
{
	FTicker::GetCoreTicker().RemoveTicker( OnTickHandle );	
}

const TSharedRef<FEventGraphData, ESPMode::ThreadSafe> FProfilerSession::GetEventGraphDataTotal() const
{
	return EventGraphDataTotal.ToSharedRef();
}

const TSharedRef<FEventGraphData, ESPMode::ThreadSafe> FProfilerSession::GetEventGraphDataMaximum() const
{
	return EventGraphDataMaximum.ToSharedRef();
}

const TSharedRef<FEventGraphData, ESPMode::ThreadSafe> FProfilerSession::GetEventGraphDataAverage() const
{
	return EventGraphDataAverage.ToSharedRef();
}

TSharedRef<const FGraphDataSource> FProfilerSession::CreateGraphDataSource( const uint32 InStatID )
{
	FGraphDataSource* GraphDataSource = new FGraphDataSource( AsShared(), InStatID );
	return MakeShareable( GraphDataSource );
}

void FProfilerSession::UpdateAggregatedStats( const uint32 FrameIndex )
{
	static FTotalTimeAndCount TimeAndCount( 0.0f, 0 );
	PROFILER_SCOPE_LOG_TIME( TEXT( "2 FProfilerSession::UpdateAggregatedStats" ), &TimeAndCount );

	const FIntPoint& IndicesForFrame = DataProvider->GetSamplesIndicesForFrame( FrameIndex );
	const uint32 SampleStartIndex = IndicesForFrame.X;
	const uint32 SampleEndIndex = IndicesForFrame.Y;

	// Aggregate counters etc.
	const FProfilerSampleArray& Collection = DataProvider->GetCollection();
	for( uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++ )
	{
		const FProfilerSample& ProfilerSample = Collection[ SampleIndex ];

		// Skip hierarchical samples to ignore misleading recursion which would be counted twice etc.
		if (ProfilerSample.Type() == EProfilerSampleTypes::HierarchicalTime)
		{
			continue;
		}
		
		const uint32 StatID = ProfilerSample.StatID();
		FProfilerAggregatedStat* AggregatedStat = AggregatedStats.Find( StatID );
		if( !AggregatedStat )
		{
			const FProfilerStat& ProfilerStat = GetMetaData()->GetStatByID( StatID );
			AggregatedStat = &AggregatedStats.Add( ProfilerSample.StatID(), FProfilerAggregatedStat( ProfilerStat.Name(), ProfilerStat.OwningGroup().Name(), ProfilerSample.Type() ) );
		}
		AggregatedStat->Aggregate( ProfilerSample, StatMetaData );
	}

	// Aggregate hierarchical stat.
	const TMap<uint32, FInclusiveTime>& InclusiveAggregates = GetInclusiveAggregateStackStats( FrameIndex );
	for (const auto& It : InclusiveAggregates)
	{
		const uint32 StatID = It.Key;
		const FInclusiveTime InclusiveTime = It.Value;
		const FProfilerStat& ProfilerStat = GetMetaData()->GetStatByID( StatID );

		FProfilerAggregatedStat* AggregatedStat = AggregatedStats.Find( StatID );
		if (!AggregatedStat)
		{
			AggregatedStat = &AggregatedStats.Add( StatID, FProfilerAggregatedStat( ProfilerStat.Name(), ProfilerStat.OwningGroup().Name(), ProfilerStat.Type() ) );
		}
		FProfilerSample MinimalSample( 0, 0, 0, InclusiveTime.DurationCycles, InclusiveTime.CallCount );
		AggregatedStat->Aggregate( MinimalSample, StatMetaData );
	}

	for( auto It = AggregatedStats.CreateIterator(); It; ++It )
	{
		FProfilerAggregatedStat& AggregatedStat = It.Value();
		AggregatedStat.Advance();
	}
}

FEventGraphData* FProfilerSession::CombineEventGraphs( const uint32 FrameStartIndex, const uint32 FrameEndIndex )
{
	FEventGraphData* EventGraphData = new FEventGraphData( this, FrameStartIndex );
	for (uint32 FrameIndex = FrameStartIndex + 1; FrameIndex < FrameEndIndex; ++FrameIndex)
	{
		// Create a temporary event graph data for the specified frame.
		const FEventGraphData CurrentEventGraphData( this, FrameIndex );
		EventGraphData->Combine( CurrentEventGraphData );
	}
	return EventGraphData;
}

void FProfilerSession::CombineEventGraphsTask( const uint32 FrameStartIndex, const uint32 FrameEndIndex )
{
	FEventGraphData* SubEventGraph = CombineEventGraphs( FrameStartIndex, FrameEndIndex );
	CombinedSubEventGraphsLFL.Push( SubEventGraph );
}

FEventGraphContainer FProfilerSession::CreateEventGraphData( const uint32 FrameStartIndex, const uint32 FrameEndIndex )
{
	static FTotalTimeAndCount Current( 0.0f, 0 );
	SCOPE_LOG_TIME_FUNC_WITH_GLOBAL( &Current );
	
	const uint32 TotalNumFrames = FrameEndIndex - FrameStartIndex + 1;
	enum
	{
		// Minimum number of frames to combine per task
		MIN_NUM_FRAMES_PER_TASK = 8
	};

	FEventGraphData* EventGraphData = nullptr;

	static const bool bUseTaskGraph = true;
	if (!bUseTaskGraph)
	{
		EventGraphData = CombineEventGraphs( FrameStartIndex, FrameEndIndex );
	}
	else
	{
		uint32 NumWorkerThreads = (uint32)FTaskGraphInterface::Get().GetNumWorkerThreads();
		uint32 NumFramesPerTask = TotalNumFrames / (NumWorkerThreads + 1);

		// Find the best configuration to utilize all worker threads.
		while (NumFramesPerTask < MIN_NUM_FRAMES_PER_TASK)
		{
			NumWorkerThreads--;
			NumFramesPerTask = TotalNumFrames / (NumWorkerThreads + 1);

			if (NumWorkerThreads <= 0)
			{
				break;
			}
		}

		UE_LOG( LogStats, Verbose, TEXT( "NumFrames: %u, NumWorkerThreads: %u, NumFramesPerTask: %u" ), TotalNumFrames, NumWorkerThreads, NumFramesPerTask );

		uint32 NumRemainingFrames = TotalNumFrames;
		uint32 MyFrameStartIndex = FrameStartIndex;
		FGraphEventArray CompletionEvents;

		// Don't run parallel code if not really needed.
		if (NumFramesPerTask >= MIN_NUM_FRAMES_PER_TASK)
		{
			for (uint32 ThreadIndex = 0; ThreadIndex < NumWorkerThreads; ++ThreadIndex)
			{
				CompletionEvents.Add(FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
				(
					FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerSession::CombineEventGraphsTask, MyFrameStartIndex, MyFrameStartIndex + NumFramesPerTask ),
					TStatId()
				));

				NumRemainingFrames -= NumFramesPerTask;
				MyFrameStartIndex += NumFramesPerTask;
			}
		}

		// Final job for remaining frames.
		FEventGraphData* FinalSubEventGraph = CombineEventGraphs( MyFrameStartIndex, MyFrameStartIndex + NumRemainingFrames );
		
		// Wait for results.
		FTaskGraphInterface::Get().WaitUntilTasksComplete( CompletionEvents );

		// Combine with sub event graphs.
		TArray<FEventGraphData*> CombinedSubEventGraphs;
		CombinedSubEventGraphsLFL.PopAll( CombinedSubEventGraphs );

		for (const auto& It : CombinedSubEventGraphs)
		{
			FEventGraphData * const SubEventGraph = It;
			FinalSubEventGraph->Combine( *SubEventGraph );
			delete SubEventGraph;
		}
		EventGraphData = FinalSubEventGraph;
	}

	check( EventGraphData );
	EventGraphData->Finalize( FrameStartIndex, FrameEndIndex + 1 );

	TSharedRef<FEventGraphData, ESPMode::ThreadSafe> Total = MakeShareable( EventGraphData );
	TSharedRef<FEventGraphData, ESPMode::ThreadSafe> Average = Total->DuplicateAsRef();
	Average->SetAsAverage();

	TSharedRef<FEventGraphData, ESPMode::ThreadSafe> Maximum = Total->DuplicateAsRef();
	Maximum->SetAsMaximim();
	
	FEventGraphContainer EventGraphContainer( FrameStartIndex, FrameEndIndex + 1, Average, Maximum, Total );
	return EventGraphContainer;
}

void FProfilerSession::EventGraphCombine( const FEventGraphData* Current, const uint32 InNumFrames )
{
	if (EventGraphDataTotal.IsValid())
	{
		EventGraphDataTotal->Combine( *Current );
	}
	else
	{
		EventGraphDataTotal = MakeShareable( new FEventGraphData( *Current ) );
	}
	
	if (InNumFrames > 0)
	{
		UpdateAllEventGraphs( InNumFrames );
	}
}

void FProfilerSession::UpdateAllEventGraphs( const uint32 InNumFrames )
{
	EventGraphDataTotal->Finalize( 0, DataProvider->GetNumFrames() );
	
	EventGraphDataAverage = EventGraphDataTotal->DuplicateAsRef();
	EventGraphDataAverage->SetAsAverage();

	EventGraphDataMaximum = EventGraphDataTotal->DuplicateAsRef();
	EventGraphDataMaximum->SetAsMaximim();
}

void FProfilerSession::UpdateAggregatedEventGraphData( const uint32 FrameIndex )
{
	static FTotalTimeAndCount TimeAndCount( 0.0f, 0 );
	PROFILER_SCOPE_LOG_TIME( TEXT( "3  FProfilerSession::UpdateAggregatedEventGraphData" ), &TimeAndCount );

	CompletionSyncAggregatedEventGraphData();

	// Create a temporary event graph data for the specified frame.
	delete EventGraphDataCurrent;
	EventGraphDataCurrent = new FEventGraphData( this, FrameIndex );
	const uint32 NumFramesLocal = SessionType == EProfilerSessionTypes::Live ? DataProvider->GetNumFrames() : 0;

	static const bool bUseTaskGraph = true;

	if( bUseTaskGraph )
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.EventGraphData.GraphCombine"),
			STAT_FSimpleDelegateGraphTask_EventGraphData_GraphCombine,
			STATGROUP_TaskGraphTasks);

		CompletionSync = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerSession::EventGraphCombine, EventGraphDataCurrent, NumFramesLocal ), 
			GET_STATID( STAT_FSimpleDelegateGraphTask_EventGraphData_GraphCombine )
		);
		
	}
	else
	{
		EventGraphCombine( EventGraphDataCurrent, NumFramesLocal );
	}
}

void FProfilerSession::CompletionSyncAggregatedEventGraphData()
{
	if (CompletionSync.GetReference() && !CompletionSync->IsComplete())
	{
		static FTotalTimeAndCount JoinTasksTimeAndCont( 0.0f, 0 );
		PROFILER_SCOPE_LOG_TIME( TEXT( "4   FProfilerSession::CombineJoinAndContinue" ), &JoinTasksTimeAndCont );

		FTaskGraphInterface::Get().WaitUntilTaskCompletes( CompletionSync, ENamedThreads::GameThread );
	}
}

enum
{
	ProfilerThreadRoot = 1
};

bool FProfilerSession::HandleTicker( float DeltaTime )
{
	static double ProcessingTime = 1.0;
	ProcessingTime -= DeltaTime;

	static int32 NumFramesProcessedLastTime = 0;

	if (ProcessingTime < 0.0)
	{
		UE_LOG( LogStats, Verbose, TEXT( "NumFramesProcessedLastTime: %4i / %4i" ), NumFramesProcessedLastTime, FrameToProcess.Num() );
		ProcessingTime = 1.0;
		NumFramesProcessedLastTime = 0;
	}

	// Limit processing to 50ms per frame.
	const double TimeLimit = 250 / 1000.0;
	double Seconds = 0;

	for( int32 It = 0; It < FrameToProcess.Num(); It++ )
	{	
		if( Seconds > TimeLimit )
		{
			break;
		}

		// Update metadata if needed
		if (bRequestStatMetadataUpdate)
		{
			StatMetaData->Update(ClientStatMetadata);
			bRequestStatMetadataUpdate = false;
		}

		static FTotalTimeAndCount Current(0.0f, 0);
		PROFILER_SCOPE_LOG_TIME( TEXT( "1 FProfilerSession::HandleTicker" ), &Current );

		NumFramesProcessedLastTime++;
		NumFramesProcessed++;

		FSimpleScopeSecondsCounter SecondsCounter(Seconds);

		const uint32 TargetFrame = FrameToProcess[0];
		FrameToProcess.RemoveAt( 0 );

		FProfilerDataFrame& CurrentProfilerData = FrameToProfilerDataMapping.FindChecked( TargetFrame );

		TMap<uint32, float> ThreadMS;

		// Preprocess the hierarchical samples for the specified frame.
		const TMap<uint32, FProfilerCycleGraph>& CycleGraphs = CurrentProfilerData.CycleGraphs;

		// Add a root sample for this frame.
		const uint32 FrameRootSampleIndex = DataProvider->AddHierarchicalSample( 0, StatMetaData->GetStatByID( 1 ).OwningGroup().ID(), ProfilerThreadRoot, 0, 0 );
		
		uint32 GameThreadCycles = 0;
		uint32 MaxThreadCycles = 0;

		TMap<uint32, FInclusiveTime> StatIDToInclusiveTime;

		for( auto ThreadIt = CycleGraphs.CreateConstIterator(); ThreadIt; ++ThreadIt )
		{
			const uint32 ThreadID = ThreadIt.Key();
			const FProfilerCycleGraph& ThreadGraph = ThreadIt.Value();

			// Calculate total time for this thread.
			FInclusiveTime ThreadDuration;
			ThreadDuration.CallCount = 1;

			for( int32 Index = 0; Index < ThreadGraph.Children.Num(); Index++ )
			{
				ThreadDuration.DurationCycles += ThreadGraph.Children[Index].Value;
			}

			if (ThreadDuration.DurationCycles > 0)
			{
				// Check for game thread.
				const FString GameThreadName = FName( NAME_GameThread ).GetPlainNameString();
				const bool bGameThreadFound = StatMetaData->GetThreadDescriptions().FindChecked( ThreadID ).Contains( GameThreadName );
				if( bGameThreadFound )
				{
					GameThreadCycles = ThreadDuration.DurationCycles;
				}

				// Add a root sample for each thread.
				const uint32& StatThreadID = StatMetaData->ThreadIDtoStatID.FindChecked( ThreadID );

				const uint32 ThreadRootSampleIndex = DataProvider->AddHierarchicalSample
				( 
					StatThreadID, 
					StatMetaData->GetStatByID(StatThreadID).OwningGroup().ID(), 
					StatThreadID, 
					ThreadDuration.DurationCycles, 1, 
					FrameRootSampleIndex 
				);
				ThreadMS.FindOrAdd( ThreadID ) = StatMetaData->ConvertCyclesToMS( ThreadDuration.DurationCycles );

				// Recursively add children and parent to the root samples.
				for( int32 Index = 0; Index < ThreadGraph.Children.Num(); Index++ )
				{
					const FProfilerCycleGraph& CycleGraph = ThreadGraph.Children[Index];
					const uint32 ChildDurationCycles = CycleGraph.Value;

					if (ChildDurationCycles > 0.0)
					{					
						PopulateHierarchy_Recurrent( StatThreadID, CycleGraph, ChildDurationCycles, ThreadRootSampleIndex, StatIDToInclusiveTime );
					}
				}

				StatIDToInclusiveTime.Add( StatThreadID, ThreadDuration );
				MaxThreadCycles = FMath::Max( MaxThreadCycles, ThreadDuration.DurationCycles );
			}
		}

		InclusiveAggregateStackStats.Add( MoveTemp( StatIDToInclusiveTime ) );
		
		// Fix the root stat time.
		FProfilerSampleArray& MutableCollection = const_cast<FProfilerSampleArray&>(DataProvider->GetCollection());
		MutableCollection[FrameRootSampleIndex].SetDurationCycles( GameThreadCycles != 0 ? GameThreadCycles : MaxThreadCycles );

		// Update FPS analyzer.
		const float GameThreadTimeMS = StatMetaData->ConvertCyclesToMS( GameThreadCycles );
		FPSAnalyzer->AddSample( GameThreadTimeMS > 0.0f ? 1000.0f / GameThreadTimeMS : 0.0f );

		// Process the non-hierarchical samples for the specified frame.
		{
			// Process integer counters.
			for( int32 Index = 0; Index < CurrentProfilerData.CountAccumulators.Num(); Index++ )
			{
				const FProfilerCountAccumulator& IntCounter = CurrentProfilerData.CountAccumulators[Index];
				const EProfilerSampleTypes::Type ProfilerSampleType = StatMetaData->GetSampleTypeForStatID( IntCounter.StatId );
				DataProvider->AddCounterSample( StatMetaData->GetStatByID(IntCounter.StatId).OwningGroup().ID(), IntCounter.StatId, (double)IntCounter.Value, ProfilerSampleType );
			}

			// Process floating point counters.
			for( int32 Index = 0; Index < CurrentProfilerData.FloatAccumulators.Num(); Index++ )
			{
				const FProfilerFloatAccumulator& FloatCounter = CurrentProfilerData.FloatAccumulators[Index];
				DataProvider->AddCounterSample( StatMetaData->GetStatByID(FloatCounter.StatId).OwningGroup().ID(), FloatCounter.StatId, (double)FloatCounter.Value, EProfilerSampleTypes::NumberFloat );
			}
		}

		// Advance frame
		const uint32 DataProviderFrameIndex = DataProvider->GetNumFrames();
		DataProvider->AdvanceFrame( StatMetaData->ConvertCyclesToMS( MaxThreadCycles ) );

		// Update aggregated stats
		UpdateAggregatedStats( DataProviderFrameIndex );

		// Update aggregated events - NOTE: This may update the metadata and set bRequestStatMetadataUpdate = true
		UpdateAggregatedEventGraphData( DataProviderFrameIndex );

		// Update mini-view.
		OnAddThreadTime.ExecuteIfBound( DataProviderFrameIndex, ThreadMS, StatMetaData );
		
		FrameToProfilerDataMapping.Remove( TargetFrame );
	}

	if( SessionType == EProfilerSessionTypes::StatsFile )
	{
		if( FrameToProcess.Num() == 0 && bHasAllProfilerData )
		{
			CompletionSyncAggregatedEventGraphData();

			// Advance event graphs.
			UpdateAllEventGraphs( DataProvider->GetNumFrames() );

			// Broadcast that a capture file has been fully processed.
			OnCaptureFileProcessed.ExecuteIfBound( GetInstanceID() );

			// Disable tick method as we no longer need to tick.
			return false;
		}
	}

	return true;
}

void FProfilerSession::PopulateHierarchy_Recurrent
( 
	const uint32 StatThreadID,
	const FProfilerCycleGraph& ParentGraph, 
	const uint32 ParentDurationCycles,
	const uint32 ParentSampleIndex,
	TMap<uint32, FInclusiveTime>& StatIDToInclusiveTime
)
{
	const TSharedRef<FProfilerStatMetaData> MetaData = GetMetaData();

#if	UE_BUILD_DEBUG
	const FName& ParentStatName = MetaData->GetStatByID( ParentGraph.StatId ).Name();
	const FName& ParentGroupName = MetaData->GetStatByID( ParentGraph.StatId ).OwningGroup().Name();
#endif // UE_BUILD_DEBUG

	{
		FInclusiveTime& InclusiveTime = StatIDToInclusiveTime.FindOrAdd( ParentGraph.StatId );
		InclusiveTime.Recursion++;
	}

	const uint32 SampleIndex = DataProvider->AddHierarchicalSample
	( 
		StatThreadID, 
		MetaData->GetStatByID(ParentGraph.StatId).OwningGroup().ID(), 
		ParentGraph.StatId, 
		ParentDurationCycles, ParentGraph.CallsPerFrame, 
		ParentSampleIndex 
	);

	uint32 ChildrenDurationCycles = 0;

	for( int32 DataIndex = 0; DataIndex < ParentGraph.Children.Num(); DataIndex++ )
	{
		const FProfilerCycleGraph& ChildCyclesCounter = ParentGraph.Children[DataIndex];
		const uint32 ChildDurationCycles = ChildCyclesCounter.Value;

		if (ChildDurationCycles > 0.0)
		{
			PopulateHierarchy_Recurrent( StatThreadID, ChildCyclesCounter, ChildDurationCycles, SampleIndex, StatIDToInclusiveTime );
		}
		ChildrenDurationCycles += ChildDurationCycles;
	}

	const uint32 SelfTimeCycles = ParentDurationCycles - ChildrenDurationCycles;
	if( SelfTimeCycles > 0.0f && ParentGraph.Children.Num() > 0 )
	{
		// Create a fake stat that represents this profiler sample's exclusive time.
		// This is required if we want to create correct combined event graphs later.
		DataProvider->AddHierarchicalSample
		(
			StatThreadID,
			MetaData->GetStatByID(0).OwningGroup().ID(),
			0, // @see FProfilerStatMetaData.Update, 0 means "Self"
			SelfTimeCycles,	1,
			SampleIndex
		);
	}

	{
		FInclusiveTime& InclusiveTime = StatIDToInclusiveTime.FindChecked( ParentGraph.StatId );
		InclusiveTime.Recursion--;

		if (InclusiveTime.Recursion == 0)
		{
			InclusiveTime.DurationCycles += ParentDurationCycles;
			InclusiveTime.CallCount++;
		}
	}
}

void FProfilerSession::LoadComplete()
{
	bHasAllProfilerData = true;
}

void FProfilerSession::SetNumberOfFrames( int32 InNumFrames )
{
	NumFrames = InNumFrames;

	InclusiveAggregateStackStats.Reserve( NumFrames );
	AggregatedStats.Reserve( 4096 );

	FrameToProfilerDataMapping.Reserve( 256 );
}

float FProfilerSession::GetProgress() const
{
	if (NumFrames > 0)
	{
		return (float)NumFramesProcessed / NumFrames;
	}
	return 0.0f;
}

const SIZE_T FProfilerSession::GetMemoryUsage() const
{
	SIZE_T MemoryUsage = 0;
	MemoryUsage += DataProvider->GetMemoryUsage();
	MemoryUsage += StatMetaData->GetMemoryUsage();

	MemoryUsage += AggregatedStats.GetAllocatedSize();

	MemoryUsage += InclusiveAggregateStackStats.GetAllocatedSize();
	for (const auto& It : InclusiveAggregateStackStats)
	{
		MemoryUsage += It.GetAllocatedSize();
	}

	MemoryUsage += FPSAnalyzer->GetMemoryUsage();

	return MemoryUsage;
}

void FProfilerSession::UpdateProfilerData( const FProfilerDataFrame& Content )
{
	FrameToProfilerDataMapping.FindOrAdd( Content.Frame ) = Content;
	FrameToProcess.Add( Content.Frame );
}

void FProfilerSession::UpdateMetadata( const FStatMetaData& InClientStatMetaData )
{
	const uint32 NewStatMetaDataSize = InClientStatMetaData.GetMetaDataSize();
	if( NewStatMetaDataSize != StatMetaDataSize )
	{
		ClientStatMetadata = InClientStatMetaData;
		bRequestStatMetadataUpdate = true;
		StatMetaDataSize = NewStatMetaDataSize;
	}
}

void FProfilerStatMetaData::Update( const FStatMetaData& ClientStatMetaData )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerStatMetaData.Update" ), nullptr );

	// Iterate through all thread descriptions.
	ThreadDescriptions.Append( ClientStatMetaData.ThreadDescriptions );

	// Initialize fake stat for Self.
	const uint32 NoGroupID = 0;

	InitializeGroup( NoGroupID, "NoGroup" );
	InitializeStat( 0, NoGroupID, TEXT( "Self" ), STATTYPE_CycleCounter );
	InitializeStat( ProfilerThreadRoot, NoGroupID, FStatConstants::NAME_ThreadRoot.GetPlainNameString(), STATTYPE_CycleCounter, FStatConstants::NAME_ThreadRoot );

	// Iterate through all stat group descriptions.
	for( auto It = ClientStatMetaData.GroupDescriptions.CreateConstIterator(); It; ++It )
	{
		const FStatGroupDescription& GroupDesc = It.Value();
		InitializeGroup( GroupDesc.ID, GroupDesc.Name );
	}

	// Iterate through all stat descriptions.
	for( auto It = ClientStatMetaData.StatDescriptions.CreateConstIterator(); It; ++It )
	{
		const FStatDescription& StatDesc = It.Value();
		InitializeStat( StatDesc.ID, StatDesc.GroupID, StatDesc.Name, (EStatType)StatDesc.StatType );
	}

	SecondsPerCycle = ClientStatMetaData.SecondsPerCycle;
}


void FProfilerStatMetaData::UpdateFromStatsState( const FStatsThreadState& StatsThreadStats )
{
	TMap<FName, int32> GroupFNameIDs;

	for( auto It = StatsThreadStats.Threads.CreateConstIterator(); It; ++It )
	{
		ThreadDescriptions.Add( It.Key(), It.Value().ToString() );
	}

	const uint32 NoGroupID = 0;
	const uint32 ThreadGroupID = 1;

	// Special groups.
	InitializeGroup( NoGroupID, "NoGroup" );

	// Self must be 0.
	InitializeStat( 0, NoGroupID, TEXT( "Self" ), STATTYPE_CycleCounter );

	// ThreadRoot must be 1.
	InitializeStat( 1, NoGroupID, FStatConstants::NAME_ThreadRoot.GetPlainNameString(), STATTYPE_CycleCounter, FStatConstants::NAME_ThreadRoot );

	int32 UniqueID = 15;

	TArray<FName> GroupFNames;
	StatsThreadStats.Groups.MultiFind( NAME_Groups, GroupFNames );
	for( const auto& GroupFName : GroupFNames  )
	{
		UniqueID++;
		InitializeGroup( UniqueID, GroupFName.ToString() );
		GroupFNameIDs.Add( GroupFName, UniqueID );
	}

	for( auto It = StatsThreadStats.ShortNameToLongName.CreateConstIterator(); It; ++It )
	{
		const FStatMessage& LongName = It.Value();
		
		const FName GroupName = LongName.NameAndInfo.GetGroupName();
		if( GroupName == NAME_Groups )
		{
			continue;
		}
		const int32 GroupID = GroupFNameIDs.FindChecked( GroupName );

		const FName StatName = It.Key();
		UniqueID++;

		EStatType StatType = STATTYPE_Error;
		if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64 )
		{
			if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsCycle ) )
			{
				StatType = STATTYPE_CycleCounter;
			}
			else if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsMemory ) )
			{
				StatType = STATTYPE_MemoryCounter;
			}
			else
			{
				StatType = STATTYPE_AccumulatorDWORD;
			}
		}
		else if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double )
		{
			StatType = STATTYPE_AccumulatorFLOAT;
		}
		else if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_Ptr )
		{
			// Not supported at this moment.
			continue;
		}

		check( StatType != STATTYPE_Error );

		int32 StatID = UniqueID;
		// Some hackery.
		if( StatName == TEXT( "STAT_FrameTime" ) )
		{
			StatID = 2;
		}

		const FString Description = LongName.NameAndInfo.GetDescription();
		const FString StatDesc = !Description.IsEmpty() ? Description : StatName.ToString();

		InitializeStat( StatID, GroupID, StatDesc, StatType, StatName );

		// Setup thread id to stat id.
		if( GroupName == FStatConstants::NAME_ThreadGroup )
		{
			uint32 ThreadID = 0;
			for( auto ThreadsIt = StatsThreadStats.Threads.CreateConstIterator(); ThreadsIt; ++ThreadsIt )
			{
				if (ThreadsIt.Value() == StatName)
				{
					ThreadID = ThreadsIt.Key();
					break;
				}
			}
			ThreadIDtoStatID.Add( ThreadID, StatID );

			// Game thread is always NAME_GameThread
			if( StatName == NAME_GameThread )
			{
				GameThreadID = ThreadID;
			}
			// Rendering thread may be "Rendering thread" or NAME_RenderThread with an index
			else if( StatName.GetPlainNameString().Contains( FName(NAME_RenderThread).GetPlainNameString() ) )
			{
				RenderThreadIDs.AddUnique( ThreadID );
			}
			else if( StatName.GetPlainNameString().Contains( TEXT( "RenderingThread" ) ) )
			{
				RenderThreadIDs.AddUnique( ThreadID );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FProfilerAggregatedStat
-----------------------------------------------------------------------------*/

FProfilerAggregatedStat::FProfilerAggregatedStat( const FName& InStatName, const FName& InGroupName, EProfilerSampleTypes::Type InStatType ) : _StatName( InStatName )
, _GroupName( InGroupName )
, _ValueOneFrame( 0.0f )
, _ValueAllFrames( 0.0f )
, _MinValueAllFrames( FLT_MAX )
, _MaxValueAllFrames( FLT_MIN )

, _NumCallsAllFrames( 0 )
, _NumCallsOneFrame( 0 )
, _MinNumCallsAllFrames( MAX_uint32 )
, _MaxNumCallsAllFrames( 0 )

, _NumFrames( 0 )
, _NumFramesWithCall( 0 )

, StatType( InStatType )
{

}

FString FProfilerAggregatedStat::ToString() const
{
	FString FormattedValueStr;
	if (StatType == EProfilerSampleTypes::HierarchicalTime)
	{
		FormattedValueStr = FString::Printf( TEXT( "{Value Min:%.3f Avg:%.3f Max:%.3f (MS) / Calls (%.1f%%) Min:%.1f Avg:%.1f Max:%.1f}" ),
											 MinValue(), AvgValue(), MaxValue(),
											 FramesWithCallPct(), MinNumCalls(), AvgNumCalls(), MaxNumCalls() );
	}
	else if (StatType == EProfilerSampleTypes::Memory)
	{
		FormattedValueStr = FString::Printf( TEXT( "{Min:%.2f Avg:%.2f Max:%.2f (KB)}" ), MinValue(), AvgValue(), MaxValue() );
	}
	else if (StatType == EProfilerSampleTypes::NumberInt)
	{
		FormattedValueStr = FString::Printf( TEXT( "{Min:%.2f Avg:%.2f Max:%.2f}" ), MinValue(), AvgValue(), MaxValue() );
	}
	else if (StatType == EProfilerSampleTypes::NumberFloat)
	{
		FormattedValueStr = FString::Printf( TEXT( "{Min:%.2f Avg:%.2f Max:%.2f}" ), MinValue(), AvgValue(), MaxValue() );
	}
	else
	{
		check( 0 );
	}
	return FormattedValueStr;
}

FString FProfilerAggregatedStat::GetFormattedValue( const Type ValueType ) const
{
	check( ValueType < Type::EInvalidOrMax );
	const double ValueArray[Type::EInvalidOrMax] =
	{
		AvgValue(),
		MinValue(),
		MaxValue(),

		AvgNumCalls(),
		MinNumCalls(),
		MaxNumCalls(),

		FramesWithCallPct(),
	};

	FString FormatValueStr;
	if (StatType == EProfilerSampleTypes::HierarchicalTime)
	{
		if (ValueType == EMinValue || ValueType == EAvgValue || ValueType == EMaxValue)
		{
			FormatValueStr = FString::Printf( TEXT( "%.3f (MS)" ), ValueArray[ValueType] );
		}
		else if (ValueType == EFramesWithCallPct)
		{
			FormatValueStr = FString::Printf( TEXT( "%.1f%%" ), ValueArray[EFramesWithCallPct] );
		}
		else
		{
			FormatValueStr = FString::Printf( TEXT( "%.1f" ), ValueArray[ValueType] );
		}
	}
	else if (StatType == EProfilerSampleTypes::Memory)
	{
		FormatValueStr = FString::Printf( TEXT( "%.2f (KB)" ), ValueArray[ValueType] );
	}
	else if (StatType == EProfilerSampleTypes::NumberInt)
	{
		FormatValueStr = FString::Printf( TEXT( "%.2f" ), ValueArray[ValueType] );
	}
	else if (StatType == EProfilerSampleTypes::NumberFloat)
	{
		FormatValueStr = FString::Printf( TEXT( "%.2f" ), ValueArray[ValueType] );
	}
	else
	{
		check( 0 );
	}
	return FormatValueStr;
}

void FProfilerAggregatedStat::Advance()
{
	_NumFrames++;

	_NumCallsAllFrames += _NumCallsOneFrame;
	_ValueAllFrames += _ValueOneFrame;

	// Calculate new extreme values.
	if (StatType == EProfilerSampleTypes::HierarchicalTime)
	{
		// Update the extreme values only if this stat has been called at least once.
		if (_NumCallsOneFrame > 0)
		{
			_NumFramesWithCall++;
		}

		{
			_MinValueAllFrames = FMath::Min<float>( _MinValueAllFrames, _ValueOneFrame );
			_MaxValueAllFrames = FMath::Max<float>( _MaxValueAllFrames, _ValueOneFrame );

			_MinNumCallsAllFrames = FMath::Min<float>( _MinNumCallsAllFrames, _NumCallsOneFrame );
			_MaxNumCallsAllFrames = FMath::Max<float>( _MaxNumCallsAllFrames, _NumCallsOneFrame );
		}
	}
	else
	{
		_MinValueAllFrames = FMath::Min<float>( _MinValueAllFrames, _ValueOneFrame );
		_MaxValueAllFrames = FMath::Max<float>( _MaxValueAllFrames, _ValueOneFrame );
	}

	_ValueOneFrame = 0.0f;
	_NumCallsOneFrame = 0;
}

void FProfilerAggregatedStat::Aggregate(const FProfilerSample& Sample, const TSharedRef<FProfilerStatMetaData>& Metadata)
{
	double TypedValue = 0.0;

	// Determine whether to we are reading a time hierarchical sample or not.
	if (Sample.Type() == EProfilerSampleTypes::HierarchicalTime)
	{
		TypedValue = Metadata->ConvertCyclesToMS( Sample.GetDurationCycles() );
		_NumCallsOneFrame += Sample.GetCallCount();
	}
	else
	{
		TypedValue = Sample.GetDoubleValue();

		if (Sample.Type() == EProfilerSampleTypes::Memory)
		{
			// @TODO: Remove later
			TypedValue *= 1.0f / 1024.0f;
		}
	}

	_ValueOneFrame += TypedValue;
}
