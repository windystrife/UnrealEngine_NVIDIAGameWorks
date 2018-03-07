// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerRawStatsForThreadView.h"
#include "HAL/FileManager.h"
#include "Templates/ScopedPointer.h"
#include "Serialization/MemoryReader.h"
#include "ProfilerDataProvider.h"


// Only copied from ProfilerSession, still not working.

/*-----------------------------------------------------------------------------
	FRawProfilerSession
-----------------------------------------------------------------------------*/

FRawProfilerSession::FRawProfilerSession( const FString& InRawStatsFileFileath )
	: FProfilerSession( EProfilerSessionTypes::StatsFileRaw, nullptr, FGuid::NewGuid(), InRawStatsFileFileath.Replace( *FStatConstants::StatsFileRawExtension, TEXT( "" ) ) )
	, CurrentMiniViewFrame( 0 )
{
	OnTick = FTickerDelegate::CreateRaw( this, &FRawProfilerSession::HandleTicker );
}

FRawProfilerSession::~FRawProfilerSession()
{
	FTicker::GetCoreTicker().RemoveTicker( OnTickHandle );
}

bool FRawProfilerSession::HandleTicker( float DeltaTime )
{
#if	0
	StatsThreadStats;
	Stream;

	enum
	{
		MAX_NUM_DATA_PER_TICK = 30
	};
	int32 NumDataThisTick = 0;

	// Add the data to the mini-view.
	for( int32 FrameIndex = CurrentMiniViewFrame; FrameIndex < Stream.FramesInfo.Num(); ++FrameIndex )
	{
		const FStatsFrameInfo& StatsFrameInfo = Stream.FramesInfo[FrameIndex];
		// Convert from cycles to ms.
		TMap<uint32, float> ThreadMS;
		for( auto InnerIt = StatsFrameInfo.ThreadCycles.CreateConstIterator(); InnerIt; ++InnerIt )
		{
			ThreadMS.Add( InnerIt.Key(), StatMetaData->ConvertCyclesToMS( InnerIt.Value() ) );
		}

		// Pass the reference to the stats' metadata.
		// #Profiler: 2014-04-03 Figure out something better later.
		OnAddThreadTime.ExecuteIfBound( FrameIndex, ThreadMS, StatMetaData );

		//CurrentMiniViewFrame++;
		NumDataThisTick++;
		if( NumDataThisTick > MAX_NUM_DATA_PER_TICK )
		{
			break;
		}
	}
#endif // 0

	return true;
}

static double GetSecondsPerCycle( const FStatPacketArray& Frame )
{
	const FName SecondsPerCycleFName = FName(TEXT("//STATGROUP_Engine//STAT_SecondsPerCycle///Seconds$32$Per$32$Cycle///////STATCAT_Advanced////"));
	const FName SecondsPerCycleRawName = FStatConstants::RAW_SecondsPerCycle;
	double Result = 0;

	for( const FStatPacket* Packet : Frame.Packets )
	{
		if( Packet->ThreadType == EThreadType::Game )
		{
			for( const FStatMessage& Item : Packet->StatMessages )
			{
				check( Item.NameAndInfo.GetFlag( EStatMetaFlags::DummyAlwaysOne ) ); 

				const FName LongName = Item.NameAndInfo.GetEncodedName();
				const FName RawName = Item.NameAndInfo.GetRawName();
				if( LongName.IsEqual( SecondsPerCycleFName, ENameCase::IgnoreCase, false ) )
				{
					Result = Item.GetValue_double();
					UE_LOG( LogStats, Log, TEXT( "STAT_SecondsPerCycle is %f [ns]" ), Result*1000*1000 );

					goto BreakPacketLoop;
				}
			}
		}
	}
BreakPacketLoop:;

	return Result;
}

static int64 GetFastThreadFrameTimeInternal( const FStatPacketArray& Frame, EThreadType::Type ThreadType )
{
	int64 Result = 0;

	for( const FStatPacket* Packet : Frame.Packets )
	{
		if( Packet->ThreadType == ThreadType )
		{
			const FStatMessagesArray& Data = Packet->StatMessages;
			for (FStatMessage const& Item : Data)
			{
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeStart)
				{
					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result -= Item.GetValue_int64();
					break;
				}
			}
			for (int32 Index = Data.Num() - 1; Index >= 0; Index--)
			{
				FStatMessage const& Item = Data[Index];
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeEnd)
				{

					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result += Item.GetValue_int64();
					break;
				}
			}
		}
	}
	return Result;
}

void FRawProfilerSession::PrepareLoading()
{
	SCOPE_LOG_TIME_FUNC();

	const FString Filepath = DataFilepath + FStatConstants::StatsFileRawExtension;
	const int64 Size = IFileManager::Get().FileSize( *Filepath );
	if( Size < 4 )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filepath );
		return;
	}
	TUniquePtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *Filepath ) );
	if( !FileReader )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filepath );
		return;
	}

	if( !Stream.ReadHeader( *FileReader ) )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open, bad magic: %s" ), *Filepath );
		return;
	}

	const bool bIsFinalized = Stream.Header.IsFinalized();
	check( bIsFinalized );
	check( Stream.Header.Version == EStatMagicWithHeader::VERSION_5 );

	TArray<FStatMessage> Messages;
	if( Stream.Header.bRawStatsFile )
	{
		// Read metadata.
		TArray<FStatMessage> MetadataMessages;
		Stream.ReadFNamesAndMetadataMessages( *FileReader, MetadataMessages );
		StatsThreadStats.ProcessMetaDataOnly( MetadataMessages );

		const FName F00245 = FName(245, 245, 0);
		
		const FName F11602 = FName(11602, 11602, 0);
		const FName F06394 = FName(6394, 6394, 0);

		const int64 CurrentFilePos = FileReader->Tell();

		// Update profiler's metadata.
		StatMetaData->UpdateFromStatsState( StatsThreadStats );
		const uint32 GameThreadID = GetMetaData()->GetGameThreadID();

		// Read frames offsets.
		Stream.ReadFramesOffsets( *FileReader );

		// Buffer used to store the compressed and decompressed data.
		TArray<uint8> SrcArray;
		TArray<uint8> DestArray;
		const bool bHasCompressedData = Stream.Header.HasCompressedData();
		check(bHasCompressedData);

		TMap<int64, FStatPacketArray> CombinedHistory;
		int64 TotalPacketSize = 0;
		int64 MaximumPacketSize = 0;
		// Read all packets sequentially, force by the memory profiler which is now a part of the raw stats.
		// !!CAUTION!! Frame number in the raw stats is pointless, because it is time based, not frame based.
		// Background threads usually execute time consuming operations, so the frame number won't be valid.
		// Needs to be combined by the thread and the time, not by the frame number.
		{
			int64 FrameOffset0 = Stream.FramesInfo[0].FrameFileOffset;
			FileReader->Seek( FrameOffset0 );

			const int64 FileSize = FileReader->TotalSize();

			while( FileReader->Tell() < FileSize )
			{
				// Read the compressed data.
				FCompressedStatsData UncompressedData( SrcArray, DestArray );
				*FileReader << UncompressedData;
				if( UncompressedData.HasReachedEndOfCompressedData() )
				{
					break;
				}

				FMemoryReader MemoryReader( DestArray, true );

				FStatPacket* StatPacket = new FStatPacket();
				Stream.ReadStatPacket( MemoryReader, *StatPacket );
				
				const int64 FrameNum = StatPacket->Frame;
				FStatPacketArray& Frame = CombinedHistory.FindOrAdd(FrameNum);
			
				// Check if we need to combine packets from the same thread.
				FStatPacket** CombinedPacket = Frame.Packets.FindByPredicate([&](FStatPacket* Item) -> bool
				{
					return Item->ThreadId == StatPacket->ThreadId;
				});
				
				if( CombinedPacket )
				{
					(*CombinedPacket)->StatMessages += StatPacket->StatMessages;
				}
				else
				{
					Frame.Packets.Add(StatPacket);
				}

				const int64 CurrentPos = FileReader->Tell();
				const int32 PctPos = int32(100.0f*CurrentPos/FileSize);

				UE_LOG( LogStats, Log, TEXT( "%3i Processing FStatPacket: Frame %5i for thread %5i with %6i messages (%.1f MB)" ), 
					PctPos, 
					StatPacket->Frame, 
					StatPacket->ThreadId, 
					StatPacket->StatMessages.Num(), 
					StatPacket->StatMessages.GetAllocatedSize()/1024.0f/1024.0f );

				const int64 PacketSize = StatPacket->StatMessages.GetAllocatedSize();
				TotalPacketSize += PacketSize;
				MaximumPacketSize = FMath::Max( MaximumPacketSize, PacketSize );
			}
		}

		UE_LOG( LogStats, Log, TEXT( "TotalPacketSize: %.1f MB, Max: %1f MB" ), 
			TotalPacketSize/1024.0f/1024.0f, 
			MaximumPacketSize/1024.0f/1024.0f );

		TArray<int64> Frames;
		CombinedHistory.GenerateKeyArray(Frames);
		Frames.Sort();
		const int64 MiddleFrame = Frames[Frames.Num()/2];


		// Remove all frames without the game thread messages.
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			const int64 TargetFrame = Frames[FrameIndex];
			const FStatPacketArray& Frame = CombinedHistory.FindChecked( TargetFrame );

			const double GameThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal( Frame, EThreadType::Game ) );

			if (GameThreadTimeMS == 0.0f)
			{
				CombinedHistory.Remove( TargetFrame );
				Frames.RemoveAt( FrameIndex );
				FrameIndex--;
			}
		}
		
	
		StatMetaData->SecondsPerCycle = GetSecondsPerCycle( CombinedHistory.FindChecked(MiddleFrame) );
		check( StatMetaData->GetSecondsPerCycle() > 0.0 );

		//const int32 FirstGameThreadFrame = FindFirstFrameWithGameThread( CombinedHistory, Frames );

		// Prepare profiler frame.
		{
			SCOPE_LOG_TIME( TEXT( "Preparing profiler frames" ), nullptr );

			// Prepare profiler frames.
			double ElapsedTimeMS = 0;

			for( int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex )
			{
				const int64 TargetFrame = Frames[FrameIndex];
				const FStatPacketArray& Frame = CombinedHistory.FindChecked(TargetFrame);

				const double GameThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal(Frame,EThreadType::Game) );

				if( GameThreadTimeMS == 0.0f )
				{
					continue;
				}

				const double RenderThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal(Frame,EThreadType::Renderer) );

				// Update mini-view, convert from cycles to ms.
				TMap<uint32, float> ThreadTimesMS;
				ThreadTimesMS.Add( GameThreadID, GameThreadTimeMS );
				ThreadTimesMS.Add( GetMetaData()->GetRenderThreadID()[0], RenderThreadTimeMS );

				// Pass the reference to the stats' metadata.
				OnAddThreadTime.ExecuteIfBound( FrameIndex, ThreadTimesMS, StatMetaData );

				// Create a new profiler frame and add it to the stream.
				ElapsedTimeMS += GameThreadTimeMS;
				FProfilerFrame* ProfilerFrame = new FProfilerFrame( TargetFrame, GameThreadTimeMS, ElapsedTimeMS );
				ProfilerFrame->ThreadTimesMS = ThreadTimesMS;
				ProfilerStream.AddProfilerFrame( TargetFrame, ProfilerFrame );
			}
		}
	
		// Process the raw stats data.
		{
			SCOPE_LOG_TIME( TEXT( "Processing the raw stats" ), nullptr );

			double CycleCounterAdjustmentMS = 0.0f;

			// Read the raw stats messages.
			for( int32 FrameIndex = 0; FrameIndex < Frames.Num()-1; ++FrameIndex )
			{
				const int64 TargetFrame = Frames[FrameIndex];
				const FStatPacketArray& Frame = CombinedHistory.FindChecked(TargetFrame);

				FProfilerFrame* ProfilerFrame = ProfilerStream.GetProfilerFrame( FrameIndex );

				UE_CLOG( FrameIndex % 8 == 0, LogStats, Log, TEXT( "Processing raw stats frame: %4i/%4i" ), FrameIndex, Frames.Num() );

				ProcessStatPacketArray( Frame, *ProfilerFrame, FrameIndex ); // or ProfilerFrame->TargetFrame

				// Find the first cycle counter for the game thread.
				if( CycleCounterAdjustmentMS == 0.0f )
				{
					CycleCounterAdjustmentMS = ProfilerFrame->Root->CycleCounterStartTimeMS;
				}

				// Update thread time and mark profiler frame as valid and ready for use.
				ProfilerFrame->MarkAsValid();
			}

			// Adjust all profiler frames.
			ProfilerStream.AdjustCycleCounters( CycleCounterAdjustmentMS );
		}
	}

	const int64 AllocatedSize = ProfilerStream.GetAllocatedSize();

	// We have the whole metadata and basic information about the raw stats file, start ticking the profiler session.
	//OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick, 0.25f );

#if	0
	if( SessionType == EProfilerSessionTypes::OfflineRaw )
	{
		// Broadcast that a capture file has been fully processed.
		OnCaptureFileProcessed.ExecuteIfBound( GetInstanceID() );
	}
#endif // 0
}

void FRawProfilerSession::ProcessStatPacketArray( const FStatPacketArray& StatPacketArray, FProfilerFrame& out_ProfilerFrame, int32 FrameIndex )
{
	// #Profiler: 2014-03-24 Standardize thread names and id
	// #Profiler: 2014-04-22 Remove all references to the data provider, event graph etc once data graph can visualize.

	// Raw stats callstack for this stat packet array.
	TMap<FName, FProfilerStackNode*> ThreadNodes;

	const TSharedRef<FProfilerStatMetaData> MetaData = GetMetaData();

	FProfilerSampleArray& MutableCollection = const_cast<FProfilerSampleArray&>(DataProvider->GetCollection());

	// Add a root sample for this frame.
	const uint32 FrameRootSampleIndex = DataProvider->AddHierarchicalSample( 0, MetaData->GetStatByID( 1 ).OwningGroup().ID(), 1, 0.0f, 0.0f, 1 );

	// Iterate through all stats packets and raw stats messages.
	FName GameThreadFName = NAME_None;
	for (int32 PacketIndex = 0; PacketIndex < StatPacketArray.Packets.Num(); PacketIndex++)
	{
		const FStatPacket& StatPacket = *StatPacketArray.Packets[PacketIndex];
		FName ThreadFName = StatsThreadStats.Threads.FindChecked( StatPacket.ThreadId );
		const uint32 NewThreadID = MetaData->ThreadIDtoStatID.FindChecked( StatPacket.ThreadId );

		// #Profiler: 2014-04-29 Only game or render thread is supported at this moment.
		if (StatPacket.ThreadType != EThreadType::Game && StatPacket.ThreadType != EThreadType::Renderer)
		{
			continue;
		}

		// Workaround for issue with rendering thread names.
		if (StatPacket.ThreadType == EThreadType::Renderer)
		{
			ThreadFName = NAME_RenderThread;
		}
		else if (StatPacket.ThreadType == EThreadType::Game)
		{
			GameThreadFName = ThreadFName;
		}

		FProfilerStackNode* ThreadNode = ThreadNodes.FindRef( ThreadFName );
		if (!ThreadNode)
		{
			FString ThreadIdName = FStatsUtils::BuildUniqueThreadName( StatPacket.ThreadId );
			FStatMessage ThreadMessage( ThreadFName, EStatDataType::ST_int64, STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupName(), STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupCategory(), *ThreadIdName, true, true );
			//FStatMessage ThreadMessage( ThreadFName, EStatDataType::ST_int64, nullptr, nullptr, TEXT( "" ), true, true );
			ThreadMessage.NameAndInfo.SetFlag( EStatMetaFlags::IsPackedCCAndDuration, true );
			ThreadMessage.Clear();

			// Add a thread sample.
			const uint32 ThreadRootSampleIndex = DataProvider->AddHierarchicalSample
				(
				NewThreadID,
				MetaData->GetStatByID( NewThreadID ).OwningGroup().ID(),
				NewThreadID,
				-1, 1,
				FrameRootSampleIndex
				);

			ThreadNode = ThreadNodes.Add( ThreadFName, new FProfilerStackNode( nullptr, ThreadMessage, ThreadRootSampleIndex, FrameIndex ) );
		}


		TArray<const FStatMessage*> StartStack;
		TArray<FProfilerStackNode*> Stack;
		Stack.Add( ThreadNode );
		FProfilerStackNode* Current = Stack.Last();

		for (const FStatMessage& Item : StatPacket.StatMessages)
		{
			const EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
			const FName LongName = Item.NameAndInfo.GetRawName();
			const FName ShortName = Item.NameAndInfo.GetShortName();

			const FName RenderingThreadTickCommandName = TEXT( "RenderingThreadTickCommand" );

			// Workaround for render thread hierarchy. EStatOperation::AdvanceFrameEventRenderThread is called within the scope.
			if (ShortName == RenderingThreadTickCommandName)
			{
				continue;
			}

			if (Op == EStatOperation::CycleScopeStart || Op == EStatOperation::CycleScopeEnd || Op == EStatOperation::AdvanceFrameEventRenderThread)
			{
				//check( Item.NameAndInfo.GetFlag( EStatMetaFlags::IsCycle ) );
				if (Op == EStatOperation::CycleScopeStart)
				{
					FProfilerStackNode* ChildNode = new FProfilerStackNode( Current, Item, -1, FrameIndex );
					Current->Children.Add( ChildNode );

					// Add a child sample.
					const uint32 SampleIndex = DataProvider->AddHierarchicalSample
						(
						NewThreadID,
						MetaData->GetStatByFName( ShortName ).OwningGroup().ID(), // GroupID
						MetaData->GetStatByFName( ShortName ).ID(), // StatID
						0, // DurationCycles
						1,
						Current->SampleIndex
						);
					ChildNode->SampleIndex = SampleIndex;

					Stack.Add( ChildNode );
					StartStack.Add( &Item );
					Current = ChildNode;
				}
				// Workaround for render thread hierarchy. EStatOperation::AdvanceFrameEventRenderThread is called within the scope.
				if (Op == EStatOperation::AdvanceFrameEventRenderThread)
				{
					int k = 0; k++;
				}
				if (Op == EStatOperation::CycleScopeEnd)
				{
					const FStatMessage ScopeStart = *StartStack.Pop();
					const FStatMessage ScopeEnd = Item;
					const int64 Delta = int32( uint32( ScopeEnd.GetValue_int64() ) - uint32( ScopeStart.GetValue_int64() ) );
					Current->CyclesEnd = Current->CyclesStart + Delta;

					Current->CycleCounterStartTimeMS = MetaData->ConvertCyclesToMS( Current->CyclesStart );
					Current->CycleCounterEndTimeMS = MetaData->ConvertCyclesToMS( Current->CyclesEnd );

					if (Current->CycleCounterStartTimeMS > Current->CycleCounterEndTimeMS)
					{
						int k = 0; k++;
					}

					check( Current->CycleCounterEndTimeMS >= Current->CycleCounterStartTimeMS );

					FProfilerStackNode* ChildNode = Current;

					// Update the child sample's DurationMS.
					MutableCollection[ChildNode->SampleIndex].SetDurationCycles( Delta );

					verify( Current == Stack.Pop() );
					Current = Stack.Last();
				}
			}
		}
	}

	// Calculate thread times.
	for (auto It = ThreadNodes.CreateIterator(); It; ++It)
	{
		FProfilerStackNode& ThreadNode = *It.Value();
		const int32 ChildrenNum = ThreadNode.Children.Num();
		if (ChildrenNum > 0)
		{
			const int32 LastChildIndex = ThreadNode.Children.Num() - 1;
			ThreadNode.CyclesStart = ThreadNode.Children[0]->CyclesStart;
			ThreadNode.CyclesEnd = ThreadNode.Children[LastChildIndex]->CyclesEnd;
			ThreadNode.CycleCounterStartTimeMS = MetaData->ConvertCyclesToMS( ThreadNode.CyclesStart );
			ThreadNode.CycleCounterEndTimeMS = MetaData->ConvertCyclesToMS( ThreadNode.CyclesEnd );

			FProfilerSample& ProfilerSample = MutableCollection[ThreadNode.SampleIndex];
			//ProfilerSample.SetStartAndEndMS( MetaData->ConvertCyclesToMS( ThreadNode.CyclesStart ), MetaData->ConvertCyclesToMS( ThreadNode.CyclesEnd ) );
		}
	}

	// Get the game thread time.
	check( GameThreadFName != NAME_None );
	const FProfilerStackNode& GameThreadNode = *ThreadNodes.FindChecked( GameThreadFName );
	const double GameThreadStartMS = MetaData->ConvertCyclesToMS( GameThreadNode.CyclesStart );
	const double GameThreadEndMS = MetaData->ConvertCyclesToMS( GameThreadNode.CyclesEnd );
	//MutableCollection[FrameRootSampleIndex].SetStartAndEndMS( GameThreadStartMS, GameThreadEndMS );

	// Advance frame
	const uint32 LastFrameIndex = DataProvider->GetNumFrames();
	DataProvider->AdvanceFrame( GameThreadEndMS - GameThreadStartMS );

	// Update aggregated stats
	//UpdateAggregatedStats( LastFrameIndex );

	// Update aggregated events.
	UpdateAggregatedEventGraphData( LastFrameIndex );

	// RootNode is the same as the game thread node.
	out_ProfilerFrame.Root->CycleCounterStartTimeMS = GameThreadStartMS;
	out_ProfilerFrame.Root->CycleCounterEndTimeMS = GameThreadEndMS;

	for (auto It = ThreadNodes.CreateIterator(); It; ++It)
	{
		out_ProfilerFrame.AddChild( It.Value() );
	}

	out_ProfilerFrame.SortChildren();
}
