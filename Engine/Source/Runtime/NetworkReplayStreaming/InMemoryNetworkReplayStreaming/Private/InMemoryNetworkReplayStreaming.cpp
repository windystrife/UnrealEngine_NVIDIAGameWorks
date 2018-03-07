// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InMemoryNetworkReplayStreaming.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/NetworkVersion.h"

DEFINE_LOG_CATEGORY_STATIC( LogMemoryReplay, Log, All );

static FString GetAutomaticDemoName()
{
	return FGuid::NewGuid().ToString();
}

void FInMemoryNetworkReplayStreamer::StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate )
{
	if ( CustomName.IsEmpty() )
	{
		if ( bRecord )
		{
			// If we're recording and the caller didn't provide a name, generate one automatically
			CurrentStreamName = GetAutomaticDemoName();
		}
		else
		{
			// Can't play a replay if the user didn't provide a name!
			Delegate.ExecuteIfBound( false, bRecord );
			return;
		}
	}
	else
	{
		CurrentStreamName = CustomName;
	}

	if ( !bRecord )
	{
		FInMemoryReplay* FoundReplay = GetCurrentReplay();
		if (FoundReplay == nullptr)
		{
			Delegate.ExecuteIfBound(false, bRecord);
			return;
		}

		FileAr.Reset(new FInMemoryReplayStreamArchive(FoundReplay->StreamChunks));
		FileAr->ArIsSaving = bRecord;
		FileAr->ArIsLoading = !bRecord;
		HeaderAr.Reset(new FMemoryReader(FoundReplay->Header));
		StreamerState = EStreamerState::Playback;
	}
	else
	{
		// Add or overwrite a demo with this name
		TUniquePtr<FInMemoryReplay> NewReplay(new FInMemoryReplay);

		NewReplay->StreamInfo.Name = CurrentStreamName;
		NewReplay->StreamInfo.FriendlyName = FriendlyName;
		NewReplay->StreamInfo.Timestamp = FDateTime::Now();
		NewReplay->StreamInfo.bIsLive = true;
		NewReplay->StreamInfo.Changelist = ReplayVersion.Changelist;
		NewReplay->NetworkVersion = ReplayVersion.NetworkVersion;

		// Open archives for writing
		FileAr.Reset(new FInMemoryReplayStreamArchive(NewReplay->StreamChunks));
		FileAr->ArIsSaving = bRecord;
		FileAr->ArIsLoading = !bRecord;
		HeaderAr.Reset(new FMemoryWriter(NewReplay->Header));

		OwningFactory->Replays.Add(CurrentStreamName, MoveTemp(NewReplay));
		
		StreamerState = EStreamerState::Recording;
	}

	// Notify immediately
	Delegate.ExecuteIfBound( FileAr.Get() != nullptr && HeaderAr.Get() != nullptr, bRecord );
}

void FInMemoryNetworkReplayStreamer::StopStreaming()
{
	if (StreamerState == EStreamerState::Recording)
	{
		FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

		FoundReplay->StreamInfo.SizeInBytes = FoundReplay->Header.Num() + FoundReplay->TotalStreamSize() + FoundReplay->Metadata.Num();
		for(const auto& Checkpoint : FoundReplay->Checkpoints)
		{
			FoundReplay->StreamInfo.SizeInBytes += Checkpoint.Data.Num();
		}

		FoundReplay->StreamInfo.bIsLive = false;
	}

	HeaderAr.Reset();
	FileAr.Reset();

	CurrentStreamName.Empty();
	StreamerState = EStreamerState::Idle;
}

FArchive* FInMemoryNetworkReplayStreamer::GetHeaderArchive()
{
	return HeaderAr.Get();
}

FArchive* FInMemoryNetworkReplayStreamer::GetStreamingArchive()
{
	return FileAr.Get();
}

void FInMemoryNetworkReplayStreamer::UpdateTotalDemoTime(uint32 TimeInMS)
{
	check(StreamerState == EStreamerState::Recording);

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	FoundReplay->StreamInfo.LengthInMS = TimeInMS;
}

uint32 FInMemoryNetworkReplayStreamer::GetTotalDemoTime() const
{
	check(StreamerState != EStreamerState::Idle);

	const FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	return FoundReplay->StreamInfo.LengthInMS;
}

bool FInMemoryNetworkReplayStreamer::IsDataAvailable() const
{
	// Assumptions:
	// 1. All streamer instances run on the same thread, not simultaneously
	// 2. A recording DemoNetDriver will write either no frames or entire frames each time it ticks
	return StreamerState == EStreamerState::Playback && FileAr.IsValid() && FileAr->Tell() < FileAr->TotalSize();
}

bool FInMemoryNetworkReplayStreamer::IsLive() const
{
	return IsNamedStreamLive(CurrentStreamName);
}

bool FInMemoryNetworkReplayStreamer::IsNamedStreamLive( const FString& StreamName ) const
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(StreamName);
	if (FoundReplay != nullptr)
	{
		return (*FoundReplay)->StreamInfo.bIsLive;
	}

	return false;
}

void FInMemoryNetworkReplayStreamer::DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate ) const
{
	// Danger! Deleting a stream that is still being read by another streaming instance is not supported!

	// Live streams can't be deleted
	if (IsNamedStreamLive(StreamName))
	{
		UE_LOG(LogMemoryReplay, Log, TEXT("Can't delete network replay stream %s because it is live!"), *StreamName);
		Delegate.ExecuteIfBound(false);
		return;
	}

	const int32 NumRemoved = OwningFactory->Replays.Remove(StreamName);

	Delegate.ExecuteIfBound(NumRemoved > 0);
}

void FInMemoryNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate )
{
	EnumerateStreams( ReplayVersion, UserString, MetaString, TArray< FString >(), Delegate );
}

void FInMemoryNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate )
{
	TArray<FNetworkReplayStreamInfo> Results;

	for ( const auto& StreamPair : OwningFactory->Replays )
	{
		// Check version. NetworkVersion and changelist of 0 will ignore version check.
		const bool NetworkVersionMatches = ReplayVersion.NetworkVersion == StreamPair.Value->NetworkVersion;
		const bool ChangelistMatches = ReplayVersion.Changelist == StreamPair.Value->StreamInfo.Changelist;

		const bool NetworkVersionPasses = ReplayVersion.NetworkVersion == 0 || NetworkVersionMatches;
		const bool ChangelistPasses = ReplayVersion.Changelist == 0 || ChangelistMatches;

		if ( NetworkVersionPasses && ChangelistPasses )
		{
			Results.Add( StreamPair.Value->StreamInfo );
		}
	}

	Delegate.ExecuteIfBound( Results );
}

void FInMemoryNetworkReplayStreamer::AddUserToReplay( const FString& UserString )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::AddUserToReplay is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::AddEvent( const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::AddEvent is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::EnumerateEvents( const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::EnumerateEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::EnumerateEvents(const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::EnumerateEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::RequestEventData(const FString& EventID, const FOnRequestEventDataComplete& RequestEventDataComplete)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::RequestEventData is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::SearchEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::KeepReplay(const FString& ReplayName, const bool bKeep)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::KeepReplay is currently unsupported."));
}

FArchive* FInMemoryNetworkReplayStreamer::GetCheckpointArchive()
{
	// If the archive is null, and the API is being used properly, the caller is writing a checkpoint...
	if ( CheckpointAr.Get() == nullptr )
	{
		check(StreamerState != EStreamerState::Playback);

		UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::GetCheckpointArchive. Creating new checkpoint."));

		FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

		// Free old checkpoints and stream chunks that are older than the threshold.
		if (TimeBufferHintSeconds > 0.0f)
		{
			// Absolute time at which the buffer should start
			const float BufferStartTimeMS = FoundReplay->StreamInfo.LengthInMS - (TimeBufferHintSeconds * 1000.0);

			// Store the found checkpoint's time so that we can line up chunks with it
			uint32 FoundCheckpointTime = 0;

			// Always keep at least one checkpoint
			int32 FirstCheckpointIndexToKeep = 0;

			// Go backwards through the checkpoints and find the one that is before the buffer starts.
			for (int32 i = FoundReplay->Checkpoints.Num() - 1; i >= 0; --i)
			{
				const FInMemoryReplay::FCheckpoint& Checkpoint = FoundReplay->Checkpoints[i];
				if (Checkpoint.TimeInMS <= BufferStartTimeMS)
				{
					FirstCheckpointIndexToKeep = i;
					FoundCheckpointTime = Checkpoint.TimeInMS;
					break;
				}
			}

			// Remove the checkpoints.
			const int32 NumCheckpointsToRemove = FirstCheckpointIndexToKeep;
			FoundReplay->Checkpoints.RemoveAt(0, NumCheckpointsToRemove, false);

			// Always keep at least one chunk
			int32 FirstChunkIndexToKeep = 0;

			// Go backwards through the chunks and find the one that corresponds to the checkpoint we kept (or the beginning of the stream).
			for (int32 i = FoundReplay->StreamChunks.Num() - 1; i >= 0; --i)
			{
				const FInMemoryReplay::FStreamChunk& Chunk = FoundReplay->StreamChunks[i];
				if (Chunk.TimeInMS <= FoundCheckpointTime)
				{
					FirstChunkIndexToKeep = i;
					break;
				}
			}

			// Remove the chunks.
			const int32 NumChunksToRemove = FirstChunkIndexToKeep;
			FoundReplay->StreamChunks.RemoveAt(0, NumChunksToRemove, false);
		}

		// Save to a temporary checkpoint that will moved onto the replay's checkpoint list in FlushCheckpoint().
		CheckpointCurrentlyBeingSaved.Reset();
		CheckpointAr.Reset(new FMemoryWriter(CheckpointCurrentlyBeingSaved.Data));
	}

	return CheckpointAr.Get();
}

void FInMemoryNetworkReplayStreamer::FlushCheckpoint(const uint32 TimeInMS)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::FlushCheckpoint. TimeInMS: %u"), TimeInMS);

	check(FileAr.Get() != nullptr);
	check(CheckpointCurrentlyBeingSaved.Data.Num() != 0);

	// Finalize the checkpoint data.
	CheckpointAr.Reset();
	
	CheckpointCurrentlyBeingSaved.TimeInMS = TimeInMS;
	CheckpointCurrentlyBeingSaved.StreamByteOffset = FileAr->Tell();

	FInMemoryReplay* const FoundReplay = GetCurrentReplayChecked();

	const int32 NewCheckpointIndex = FoundReplay->Checkpoints.Add(MoveTemp(CheckpointCurrentlyBeingSaved));

	// Start a new stream chunk for the new checkpoint
	FInMemoryReplay::FStreamChunk NewChunk;
	if (FoundReplay->StreamChunks.Num() > 0)
	{
		NewChunk.StartIndex = FoundReplay->StreamChunks.Last().StartIndex + FoundReplay->StreamChunks.Last().Data.Num();
		NewChunk.TimeInMS = FoundReplay->StreamInfo.LengthInMS;
	}
	FoundReplay->StreamChunks.Add(NewChunk);
}

void FInMemoryNetworkReplayStreamer::GotoCheckpointIndex(const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate)
{
	GotoCheckpointIndexInternal(CheckpointIndex, Delegate, -1);
}

void FInMemoryNetworkReplayStreamer::GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS)
{
	check( FileAr.Get() != nullptr);

	if ( CheckpointIndex == -1 )
	{
		// Create a dummy checkpoint archive to indicate this is the first checkpoint
		CheckpointAr.Reset(new FArchive);

		FileAr->Seek(0);
		
		Delegate.ExecuteIfBound( true, TimeInMS );
		return;
	}

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	if (!FoundReplay->Checkpoints.IsValidIndex(CheckpointIndex))
	{
		UE_LOG(LogMemoryReplay, Log, TEXT("FNullNetworkReplayStreamer::GotoCheckpointIndex. Index %i is out of bounds."), CheckpointIndex);
		Delegate.ExecuteIfBound( false, TimeInMS );
		return;
	}

	CheckpointAr.Reset(new FMemoryReader(FoundReplay->Checkpoints[CheckpointIndex].Data));

	FileAr->Seek( FoundReplay->Checkpoints[CheckpointIndex].StreamByteOffset );

	Delegate.ExecuteIfBound( true, TimeInMS );
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplay()
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(CurrentStreamName);
	if (FoundReplay != nullptr)
	{
		return FoundReplay->Get();
	}

	return nullptr;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplay() const
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(CurrentStreamName);
	if (FoundReplay != nullptr)
	{
		return FoundReplay->Get();
	}

	return nullptr;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplayChecked()
{
	FInMemoryReplay* FoundReplay = GetCurrentReplay();
	check(FoundReplay != nullptr);
	
	return FoundReplay;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplayChecked() const
{
	FInMemoryReplay* FoundReplay = GetCurrentReplay();
	check(FoundReplay != nullptr);
	
	return FoundReplay;
}

void FInMemoryNetworkReplayStreamer::GotoTimeInMS(const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate)
{
	int32 CheckpointIndex = -1;

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	// Checkpoints are sorted by time. Look backwards through the array
	// to find the one immediately preceding the target time.
	for (int32 i = FoundReplay->Checkpoints.Num() - 1; i >= 0; --i)
	{
		if (FoundReplay->Checkpoints[i].TimeInMS <= TimeInMS)
		{
			CheckpointIndex = i;
			break;
		}
	}

	if (CheckpointIndex == -1)
	{
		// No checkpoint was found. We may be going the beginning of the stream without an explicit
		// checkpoint, but if the target time is before the start time of the first stream chunk,
		// the data was likely discarded due to the TimeBufferHintSeconds value and we can't do anything
		// except report an error.
		if (FoundReplay->StreamChunks.Num() == 0 || FoundReplay->StreamChunks[0].TimeInMS > TimeInMS)
		{
			Delegate.ExecuteIfBound(false, TimeInMS);
			return;
		}
	}

	int32 ExtraSkipTimeInMS = TimeInMS;

	if ( CheckpointIndex >= 0 )
	{
		// Subtract off checkpoint time so we pass in the leftover to the engine to fast forward through for the fine scrubbing part
		ExtraSkipTimeInMS = TimeInMS - FoundReplay->Checkpoints[ CheckpointIndex ].TimeInMS;
	}

	GotoCheckpointIndexInternal( CheckpointIndex, Delegate, ExtraSkipTimeInMS );
}

void FInMemoryNetworkReplayStreamer::Tick(float DeltaSeconds)
{
	
}

TStatId FInMemoryNetworkReplayStreamer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FInMemoryNetworkReplayStreamer, STATGROUP_Tickables);
}

void FInMemoryReplayStreamArchive::Serialize(void* V, int64 Length) 
{
	if (IsLoading() )
	{
		if (Pos + Length > TotalSize())
		{
			ArIsError = true;
			return;
		}
		
		FInMemoryReplay::FStreamChunk* CurrentChunk = GetCurrentChunk();

		if (CurrentChunk == nullptr)
		{
			ArIsError = true;
			return;
		}

		const int32 OffsetIntoChunk = Pos - CurrentChunk->StartIndex;
		FMemory::Memcpy(V, CurrentChunk->Data.GetData() + OffsetIntoChunk, Length);

		Pos += Length;
	}
	else
	{
		check(Pos <= TotalSize());

		FInMemoryReplay::FStreamChunk* CurrentChunk = GetCurrentChunk();

		if (CurrentChunk == nullptr)
		{
			ArIsError = true;
			return;
		}

		const int32 OffsetIntoChunk = Pos - CurrentChunk->StartIndex;
		const int32 SpaceNeeded = Length - (CurrentChunk->Data.Num() - OffsetIntoChunk);

		if (SpaceNeeded > 0)
		{
			CurrentChunk->Data.AddZeroed(SpaceNeeded);
		}

		FMemory::Memcpy(CurrentChunk->Data.GetData() + OffsetIntoChunk, V, Length);

		Pos += Length;
	}
}

int64 FInMemoryReplayStreamArchive::Tell() 
{
	return Pos;
}

int64 FInMemoryReplayStreamArchive::TotalSize()
{
	if (Chunks.Num() == 0)
	{
		return 0;
	}

	return Chunks.Last().StartIndex + Chunks.Last().Data.Num();
}

void FInMemoryReplayStreamArchive::Seek(int64 InPos) 
{
	check(InPos < TotalSize());

	Pos = InPos;
}

bool FInMemoryReplayStreamArchive::AtEnd() 
{
	return Pos >= TotalSize();
}

FInMemoryReplay::FStreamChunk* FInMemoryReplayStreamArchive::GetCurrentChunk() const
{
	// This assumes that the Chunks array is always sorted by StartOffset!
	for (int i = Chunks.Num() - 1; i >= 0; --i)
	{
		if (Chunks[i].StartIndex <= Pos)
		{
			check(Chunks[i].StartIndex + Chunks[i].Data.Num() >= Pos);
			return &Chunks[i];
		}
	}

	return nullptr;
}

IMPLEMENT_MODULE(FInMemoryNetworkReplayStreamingFactory, InMemoryNetworkReplayStreaming)

TSharedPtr<INetworkReplayStreamer> FInMemoryNetworkReplayStreamingFactory::CreateReplayStreamer() 
{
	return TSharedPtr<INetworkReplayStreamer>(new FInMemoryNetworkReplayStreamer(this));
}
