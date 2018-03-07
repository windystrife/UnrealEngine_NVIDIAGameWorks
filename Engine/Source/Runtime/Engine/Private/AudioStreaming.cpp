// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioStreaming.cpp: Implementation of audio streaming classes.
=============================================================================*/

#include "AudioStreaming.h"
#include "Misc/CoreStats.h"
#include "Sound/SoundWave.h"
#include "Sound/AudioSettings.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "AsyncFileHandle.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"

static int32 SpoofFailedStreamChunkLoad = 0;
FAutoConsoleVariableRef CVarSpoofFailedStreamChunkLoad(
	TEXT("au.SpoofFailedStreamChunkLoad"),
	SpoofFailedStreamChunkLoad,
	TEXT("Forces failing to load streamed chunks.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);


/*------------------------------------------------------------------------------
	Streaming chunks from the derived data cache.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/** Initialization constructor. */
FAsyncStreamDerivedChunkWorker::FAsyncStreamDerivedChunkWorker(
	const FString& InDerivedDataKey,
	void* InDestChunkData,
	int32 InChunkSize,
	FThreadSafeCounter* InThreadSafeCounter
	)
	: DerivedDataKey(InDerivedDataKey)
	, DestChunkData(InDestChunkData)
	, ExpectedChunkSize(InChunkSize)
	, bRequestFailed(false)
	, ThreadSafeCounter(InThreadSafeCounter)
{
}

/** Retrieves the derived chunk from the derived data cache. */
void FAsyncStreamDerivedChunkWorker::DoWork()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncStreamDerivedChunkWorker::DoWork"), STAT_AsyncStreamDerivedChunkWorker_DoWork, STATGROUP_StreamingDetails);

	UE_LOG(LogAudio, Verbose, TEXT("Start of ASync DDC Chunk read for key: %s"), *DerivedDataKey);

	TArray<uint8> DerivedChunkData;

	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedChunkData))
	{
		FMemoryReader Ar(DerivedChunkData, true);
		int32 ChunkSize = 0;
		Ar << ChunkSize;
		checkf(ChunkSize == ExpectedChunkSize, TEXT("ChunkSize(%d) != ExpectedSize(%d)"), ChunkSize, ExpectedChunkSize);
		Ar.Serialize(DestChunkData, ChunkSize);
	}
	else
	{
		bRequestFailed = true;
	}
	FPlatformMisc::MemoryBarrier();
	ThreadSafeCounter->Decrement();

	UE_LOG(LogAudio, Verbose, TEXT("End of ASync DDC Chunk read for key: %s"), *DerivedDataKey);
}

#endif // #if WITH_EDITORONLY_DATA

////////////////////////
// FStreamingWaveData //
////////////////////////

FStreamingWaveData::FStreamingWaveData()
	: SoundWave(NULL)
	, IORequestHandle(nullptr)
	, AudioStreamingManager(nullptr)
{
}

FStreamingWaveData::~FStreamingWaveData()
{
	// Make sure there are no pending requests in flight.
	for (int32 Pass = 0; Pass < 3; Pass++)
	{
		BlockTillAllRequestsFinished();
		if (!UpdateStreamingStatus())
		{
			break;
		}
		check(Pass < 2); // we should be done after two passes. Pass 0 will start anything we need and pass 1 will complete those requests
	}
	for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
	{
		FreeLoadedChunk(LoadedChunk);
	}
	if (IORequestHandle)
	{
		delete IORequestHandle;
		IORequestHandle = nullptr;
	}
}

bool FStreamingWaveData::Initialize(USoundWave* InSoundWave, FAudioStreamingManager* InAudioStreamingManager)
{
	check(!IORequestHandle);

	if (!InSoundWave || !InSoundWave->RunningPlatformData->Chunks.Num())
	{
		UE_LOG(LogAudio, Error, TEXT("Failed to initialize streaming wave data due to lack of serialized stream chunks. Error during stream cooking."));
		return false;
	}

	SoundWave = InSoundWave;
	AudioStreamingManager = InAudioStreamingManager;

	// Always get the first chunk of data so we can play immediately
	check(LoadedChunks.Num() == 0);
	check(LoadedChunkIndices.Num() == 0);

	// Prepare 4 chunks of streaming wave data in loaded chunks array
	LoadedChunks.Reset(4);

	const int32 FirstLoadedChunkIndex = AddNewLoadedChunk(SoundWave->RunningPlatformData->Chunks[0].DataSize);

	FLoadedAudioChunk* FirstChunk = &LoadedChunks[FirstLoadedChunkIndex];
	FirstChunk->Index = 0;

	// If we fail here, we'll just fail the streaming wave data altogether.
	if (!SoundWave->GetChunkData(0, &FirstChunk->Data))
	{
		// Error/warning logging will have already been performed in the GetChunkData function
		return false;
	}

	// Set up the loaded/requested indices to be identical
	LoadedChunkIndices.Add(0);
	CurrentRequest.RequiredIndices.Add(0);

	return true;
}

bool FStreamingWaveData::UpdateStreamingStatus()
{
	if (!SoundWave)
	{
		return false;
	}

	bool	bHasPendingRequestInFlight = true;
	int32	RequestStatus = PendingChunkChangeRequestStatus.GetValue();
	TArray<uint32> IndicesToLoad;
	TArray<uint32> IndicesToFree;

	if (!HasPendingRequests(IndicesToLoad, IndicesToFree))
	{
		check(RequestStatus == AudioState_ReadyFor_Requests);
		bHasPendingRequestInFlight = false;
	}
	// Pending request in flight, though we might be able to finish it.
	else
	{
		if (RequestStatus == AudioState_ReadyFor_Finalization)
		{
			if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
			{
				FString LogString = FString::Printf(TEXT("Finalised loading of chunk(s) %d"), IndicesToLoad[0]);
				for (int32 Index = 1; Index < IndicesToLoad.Num(); ++Index)
				{
					LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
				}
				LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetName());
				UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
			}

			bool bFailedRequests = false;
#if WITH_EDITORONLY_DATA
			bFailedRequests = FinishDDCRequests();
#endif //WITH_EDITORONLY_DATA

			// could maybe iterate over the things we know are done, but I couldn't tell if that was IndicesToLoad or not.
			for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
			{
				if (LoadedChunk.IORequest && LoadedChunk.IORequest->PollCompletion())
				{
					LoadedChunk.IORequest->WaitCompletion();
					delete LoadedChunk.IORequest;
					LoadedChunk.IORequest = nullptr;
				}
			}

			PendingChunkChangeRequestStatus.Decrement();
			bHasPendingRequestInFlight = false;
			LoadedChunkIndices = CurrentRequest.RequiredIndices;
		}
		else if (RequestStatus == AudioState_ReadyFor_Requests) // odd that this is an else, probably we should start requests right now
		{
			BeginPendingRequests(IndicesToLoad, IndicesToFree);
		}
	}

	return bHasPendingRequestInFlight;
}

void FStreamingWaveData::UpdateChunkRequests(FWaveRequest& InWaveRequest)
{
	// Might change this but ensures chunk 0 stays loaded for now
	check(InWaveRequest.RequiredIndices.Contains(0));
	check(PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests);

	CurrentRequest = InWaveRequest;

}

bool FStreamingWaveData::HasPendingRequests(TArray<uint32>& IndicesToLoad, TArray<uint32>& IndicesToFree) const
{
	IndicesToLoad.Empty();
	IndicesToFree.Empty();

	// Find indices that aren't loaded
	for (auto NeededIndex : CurrentRequest.RequiredIndices)
	{
		if (!LoadedChunkIndices.Contains(NeededIndex))
		{
			IndicesToLoad.AddUnique(NeededIndex);
		}
	}

	// Find indices that aren't needed anymore
	for (auto CurrentIndex : LoadedChunkIndices)
	{
		if (!CurrentRequest.RequiredIndices.Contains(CurrentIndex))
		{
			IndicesToFree.AddUnique(CurrentIndex);
		}
	}

	return IndicesToLoad.Num() > 0 || IndicesToFree.Num() > 0;
}

void FStreamingWaveData::BeginPendingRequests(const TArray<uint32>& IndicesToLoad, const TArray<uint32>& IndicesToFree)
{
	if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
	{
		FString LogString = FString::Printf(TEXT("Requesting ASync load of chunk(s) %d"), IndicesToLoad[0]);
		for (int32 Index = 1; Index < IndicesToLoad.Num(); ++Index)
		{
			LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
		}
		LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetName());
		UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
	}

	TArray<uint32> FreeChunkIndices;

	// Mark Chunks for removal in case they can be reused
	{
		for (auto Index : IndicesToFree)
		{
			for (int32 ChunkIndex = 0; ChunkIndex < LoadedChunks.Num(); ++ChunkIndex)
			{
				if (LoadedChunks[ChunkIndex].Index == Index)
				{
					FreeLoadedChunk(LoadedChunks[ChunkIndex]);
					LoadedChunks.RemoveAt(ChunkIndex);
					break;
				}
			}
		}
	}

	if (IndicesToLoad.Num() > 0)
	{
		PendingChunkChangeRequestStatus.Set(AudioState_InProgress_Loading);

		// Set off all IO Requests
		for (auto Index : IndicesToLoad)
		{
			const FStreamedAudioChunk& Chunk = SoundWave->RunningPlatformData->Chunks[Index];
			int32 ChunkSize = Chunk.DataSize;

			int32 LoadedChunkStorageIndex = AddNewLoadedChunk(ChunkSize);
			FLoadedAudioChunk* ChunkStorage = &LoadedChunks[LoadedChunkStorageIndex];

			ChunkStorage->Index = Index;

			check(LoadedChunkStorageIndex != INDEX_NONE);

			// Pass the request on to the async io manager after increasing the request count. The request count 
			// has been pre-incremented before fielding the update request so we don't have to worry about file
			// I/O immediately completing and the game thread kicking off again before this function
			// returns.
			PendingChunkChangeRequestStatus.Increment();

			EAsyncIOPriority AsyncIOPriority = AIOP_High;

			// Load and decompress async.
#if WITH_EDITORONLY_DATA
			if (Chunk.DerivedDataKey.IsEmpty() == false)
			{
				ChunkStorage->MemorySize = ChunkSize;
				ChunkStorage->Data = static_cast<uint8*>(FMemory::Malloc(ChunkSize));
				INC_DWORD_STAT_BY(STAT_AudioMemorySize, ChunkSize);
				INC_DWORD_STAT_BY(STAT_AudioMemory, ChunkSize);

				FAsyncStreamDerivedChunkTask* Task = new(PendingAsyncStreamDerivedChunkTasks)FAsyncStreamDerivedChunkTask(
					Chunk.DerivedDataKey,
					ChunkStorage->Data,
					ChunkSize,
					&PendingChunkChangeRequestStatus
					);
				Task->StartBackgroundTask();
			}
			else
#endif // #if WITH_EDITORONLY_DATA
			{
				check(Chunk.BulkData.GetFilename().Len());
				UE_CLOG(Chunk.BulkData.IsStoredCompressedOnDisk(), LogAudio, Fatal, TEXT("Package level compression is no longer supported."));
				check(!ChunkStorage->IORequest);
				if (!IORequestHandle)
				{
					IORequestHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*Chunk.BulkData.GetFilename());
					check(IORequestHandle); // this generally cannot fail because it is async
				}
				check(Chunk.BulkData.GetBulkDataSize() == ChunkStorage->DataSize);

				FAsyncFileCallBack AsyncFileCallBack =
					[this, LoadedChunkStorageIndex](bool bWasCancelled, IAsyncReadRequest* Req)
				{
					AudioStreamingManager->OnAsyncFileCallback(this, LoadedChunkStorageIndex, Req);

					PendingChunkChangeRequestStatus.Decrement();
				};

				check(!ChunkStorage->Data);
				ChunkStorage->IORequest = IORequestHandle->ReadRequest(Chunk.BulkData.GetBulkDataOffsetInFile(), ChunkStorage->DataSize, AsyncIOPriority, &AsyncFileCallBack);
				if (!ChunkStorage->IORequest)
				{
					UE_LOG(LogAudio, Error, TEXT("Audio streaming read request failed."));

					// we failed for some reason; file not found I guess.
					PendingChunkChangeRequestStatus.Decrement();
				}
			}
		}

		// Decrement the state to AudioState_InProgress_Loading + NumChunksCurrentLoading - 1.
		PendingChunkChangeRequestStatus.Decrement();
	}
	else
	{
		// Skip straight to finalisation
		PendingChunkChangeRequestStatus.Set(AudioState_ReadyFor_Finalization);
	}

}

bool FStreamingWaveData::BlockTillAllRequestsFinished(float TimeLimit)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStreamingWaveData_BlockTillAllRequestsFinished);
	if (TimeLimit == 0.0f)
	{
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				LoadedChunk.IORequest->WaitCompletion();
				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	else
	{
		double EndTime = FPlatformTime::Seconds() + TimeLimit;
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!LoadedChunk.IORequest->WaitCompletion(ThisTimeLimit))
				{
					return false;
				}
				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	return true;
}

#if WITH_EDITORONLY_DATA
bool FStreamingWaveData::FinishDDCRequests()
{
	bool bRequestFailed = false;
	if (PendingAsyncStreamDerivedChunkTasks.Num())
	{
		for (int32 TaskIndex = 0; TaskIndex < PendingAsyncStreamDerivedChunkTasks.Num(); ++TaskIndex)
		{
			FAsyncStreamDerivedChunkTask& Task = PendingAsyncStreamDerivedChunkTasks[TaskIndex];
			Task.EnsureCompletion();
			bRequestFailed |= Task.GetTask().DidRequestFail();
		}
		PendingAsyncStreamDerivedChunkTasks.Empty();
	}
	return bRequestFailed;
}
#endif //WITH_EDITORONLY_DATA

int32 FStreamingWaveData::AddNewLoadedChunk(int32 ChunkSize)
{
	int32 NewIndex = LoadedChunks.Num();
	LoadedChunks.AddDefaulted();

	LoadedChunks[NewIndex].DataSize = ChunkSize;

	return NewIndex;
}

void FStreamingWaveData::FreeLoadedChunk(FLoadedAudioChunk& LoadedChunk)
{
	if (LoadedChunk.IORequest)
	{
		LoadedChunk.IORequest->Cancel();
		LoadedChunk.IORequest->WaitCompletion();
		delete LoadedChunk.IORequest;
		LoadedChunk.IORequest = nullptr;

		// Process pending async requests after iorequest finishes
		AudioStreamingManager->ProcessPendingAsyncFileResults();
	}

	if (LoadedChunk.Data != NULL)
	{
		FMemory::Free(LoadedChunk.Data);

		DEC_DWORD_STAT_BY(STAT_AudioMemorySize, LoadedChunk.DataSize);
		DEC_DWORD_STAT_BY(STAT_AudioMemory, LoadedChunk.DataSize);
	}
	LoadedChunk.Data = NULL;
	LoadedChunk.DataSize = 0;
	LoadedChunk.MemorySize = 0;
	LoadedChunk.Index = 0;
}

////////////////////////////
// FAudioStreamingManager //
////////////////////////////

FAudioStreamingManager::FAudioStreamingManager()
{
}

FAudioStreamingManager::~FAudioStreamingManager()
{
}

void FAudioStreamingManager::OnAsyncFileCallback(FStreamingWaveData* StreamingWaveData, int32 LoadedAudioChunkIndex, IAsyncReadRequest* ReadRequest)
{
	// Check to see if we successfully managed to load anything
	uint8* Mem = ReadRequest->GetReadResults();
	if (Mem)
	{
		// Create a new chunk load result object. Will be deleted on audio thread when TQueue is pumped.
		FASyncAudioChunkLoadResult* NewAudioChunkLoadResult = new FASyncAudioChunkLoadResult();

		// Copy the ptr which we will use to place the results of the read on the audio thread upon pumping.
		NewAudioChunkLoadResult->StreamingWaveData = StreamingWaveData;

		// Grab the loaded chunk memory ptr since it will be invalid as soon as this callback finishes
		NewAudioChunkLoadResult->DataResults = Mem;

		// The chunk index to load the results into
		NewAudioChunkLoadResult->LoadedAudioChunkIndex = LoadedAudioChunkIndex;

		// Safely enqueue the results of the async file callback into a queue to be pumped on audio thread
		AsyncAudioStreamChunkResults.Enqueue(NewAudioChunkLoadResult);
	}
}

void FAudioStreamingManager::ProcessPendingAsyncFileResults()
{
	// Pump the results of any async file loads in a protected critical section 
	FASyncAudioChunkLoadResult* AudioChunkLoadResult = nullptr;
	while (AsyncAudioStreamChunkResults.Dequeue(AudioChunkLoadResult))
	{
		// Copy the results to the chunk storage safely
		const int32 LoadedAudioChunkIndex = AudioChunkLoadResult->LoadedAudioChunkIndex;

		check(AudioChunkLoadResult->StreamingWaveData != nullptr);
		check(LoadedAudioChunkIndex != INDEX_NONE);
		check(LoadedAudioChunkIndex < AudioChunkLoadResult->StreamingWaveData->LoadedChunks.Num());

		FLoadedAudioChunk* ChunkStorage = &AudioChunkLoadResult->StreamingWaveData->LoadedChunks[LoadedAudioChunkIndex];

		checkf(!ChunkStorage->Data, TEXT("Chunk storage already has data. (0x%p), datasize: %d"), ChunkStorage->Data, ChunkStorage->DataSize);

		ChunkStorage->Data = AudioChunkLoadResult->DataResults;

		DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, ChunkStorage->DataSize);
		INC_DWORD_STAT_BY(STAT_AudioMemorySize, ChunkStorage->DataSize);
		INC_DWORD_STAT_BY(STAT_AudioMemory, ChunkStorage->DataSize);

		// Cleanup the chunk load results
		delete AudioChunkLoadResult;
		AudioChunkLoadResult = nullptr;
	}
}

void FAudioStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything /*= false*/)
{
	LLM_SCOPE(ELLMTag::Audio);

	FScopeLock Lock(&CriticalSection);

	for (auto& WavePair : StreamingSoundWaves)
	{
		WavePair.Value->UpdateStreamingStatus();
	}

	// Process any async file requests after updating the stream status
	ProcessPendingAsyncFileResults();

	for (auto Source : StreamingSoundSources)
	{
		const FWaveInstance* WaveInstance = Source->GetWaveInstance();
		USoundWave* Wave = WaveInstance ? WaveInstance->WaveData : nullptr;
		if (Wave)
		{
			FStreamingWaveData** WaveDataPtr = StreamingSoundWaves.Find(Wave);
	
			if (WaveDataPtr && (*WaveDataPtr)->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
			{
				FStreamingWaveData* WaveData = *WaveDataPtr;
				// Request the chunk the source is using and the one after that
				FWaveRequest& WaveRequest = GetWaveRequest(Wave);
				const FSoundBuffer* SoundBuffer = Source->GetBuffer();
				if (SoundBuffer)
				{
					int32 SourceChunk = SoundBuffer->GetCurrentChunkIndex();
					if (SourceChunk >= 0 && SourceChunk < Wave->RunningPlatformData->NumChunks)
					{
						WaveRequest.RequiredIndices.AddUnique(SourceChunk);
						WaveRequest.RequiredIndices.AddUnique((SourceChunk + 1) % Wave->RunningPlatformData->NumChunks);
						if (!WaveData->LoadedChunkIndices.Contains(SourceChunk)
							|| SoundBuffer->GetCurrentChunkOffset() > Wave->RunningPlatformData->Chunks[SourceChunk].DataSize / 2)
						{
							// currently not loaded or already read over half, request is high priority
							WaveRequest.bPrioritiseRequest = true;
						}
					}
					else
					{
						UE_LOG(LogAudio, Log, TEXT("Invalid chunk request curIndex=%d numChunks=%d\n"), SourceChunk, Wave->RunningPlatformData->NumChunks);
					}
				}
			}
		}
	}

	for (auto Iter = WaveRequests.CreateIterator(); Iter; ++Iter)
	{
		USoundWave* Wave = Iter.Key();
		FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(Wave);

		if (WaveData && WaveData->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
		{
			WaveData->UpdateChunkRequests(Iter.Value());
			WaveData->UpdateStreamingStatus();
			Iter.RemoveCurrent();
		}
	}

	// Process any async file requests after updating the streaming wave data stream statuses
	ProcessPendingAsyncFileResults();
}

int32 FAudioStreamingManager::BlockTillAllRequestsFinished(float TimeLimit, bool)
{
	{
		FScopeLock Lock(&CriticalSection);

		QUICK_SCOPE_CYCLE_COUNTER(FAudioStreamingManager_BlockTillAllRequestsFinished);
		int32 Result = 0;

		if (TimeLimit == 0.0f)
		{
			for (auto& WavePair : StreamingSoundWaves)
			{
				WavePair.Value->BlockTillAllRequestsFinished();
			}
		}
		else
		{
			double EndTime = FPlatformTime::Seconds() + TimeLimit;
			for (auto& WavePair : StreamingSoundWaves)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!WavePair.Value->BlockTillAllRequestsFinished(ThisTimeLimit))
				{
					Result = 1; // we don't report the actual number, just 1 for any number of outstanding requests
					break;
				}
			}
		}
		
		// After blocking to process all requests, pump the queue
		ProcessPendingAsyncFileResults();

		return Result;
	}

	// Not sure yet whether this will work the same as textures - aside from just before destroying
	return 0;
}

void FAudioStreamingManager::CancelForcedResources()
{
}

void FAudioStreamingManager::NotifyLevelChange()
{
}

void FAudioStreamingManager::SetDisregardWorldResourcesForFrames(int32 NumFrames)
{
}

void FAudioStreamingManager::AddLevel(class ULevel* Level)
{
}

void FAudioStreamingManager::RemoveLevel(class ULevel* Level)
{
}

void FAudioStreamingManager::NotifyLevelOffset(class ULevel* Level, const FVector& Offset)
{
}

void FAudioStreamingManager::AddStreamingSoundWave(USoundWave* SoundWave)
{
	if (FPlatformProperties::SupportsAudioStreaming() && SoundWave->IsStreaming())
	{
		FScopeLock Lock(&CriticalSection);
		if (StreamingSoundWaves.FindRef(SoundWave) == nullptr)
		{
			FStreamingWaveData* NewStreamingWaveData = new FStreamingWaveData;
			if (NewStreamingWaveData->Initialize(SoundWave, this))
			{
				StreamingSoundWaves.Add(SoundWave, NewStreamingWaveData);
			}
			else
			{
				// Failed to initialize, don't add to list of streaming sound waves
				delete NewStreamingWaveData;
			}
		}
	}
}

void FAudioStreamingManager::RemoveStreamingSoundWave(USoundWave* SoundWave)
{
	FScopeLock Lock(&CriticalSection);
	FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		StreamingSoundWaves.Remove(SoundWave);
		delete WaveData;
	}
	WaveRequests.Remove(SoundWave);
}

bool FAudioStreamingManager::IsManagedStreamingSoundWave(const USoundWave* SoundWave) const
{
	FScopeLock Lock(&CriticalSection);
	return StreamingSoundWaves.FindRef(SoundWave) != NULL;
}

bool FAudioStreamingManager::IsStreamingInProgress(const USoundWave* SoundWave)
{
	FScopeLock Lock(&CriticalSection);
	FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		return WaveData->UpdateStreamingStatus();
	}
	return false;
}

bool FAudioStreamingManager::CanCreateSoundSource(const FWaveInstance* WaveInstance) const
{
	check(WaveInstance);
	check(WaveInstance->IsStreaming());

	int32 MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;

	FScopeLock Lock(&CriticalSection);

	// If the sound wave hasn't been added, or failed when trying to add during sound wave post load, we can't create a streaming sound source with this sound wave
	if (!WaveInstance->WaveData || !StreamingSoundWaves.Contains(WaveInstance->WaveData))
	{
		return false;
	}

	if ( StreamingSoundSources.Num() < MaxStreams )
	{
		return true;
	}

	for (int32 Index = 0; Index < StreamingSoundSources.Num(); ++Index)
	{
		const FSoundSource* ExistingSource = StreamingSoundSources[Index];
		const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
		if (!ExistingWaveInst || !ExistingWaveInst->WaveData
			|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
		{
			return Index < MaxStreams;
		}
	}

	return false;
}

void FAudioStreamingManager::AddStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->IsStreaming())
	{
		int32 MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;

		FScopeLock Lock(&CriticalSection);

		// Add source sorted by priority so we can easily iterate over the amount of streams
		// that are allowed
		int32 OrderedIndex = -1;
		for (int32 Index = 0; Index < StreamingSoundSources.Num() && Index < MaxStreams; ++Index)
		{
			const FSoundSource* ExistingSource = StreamingSoundSources[Index];
			const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
			if (!ExistingWaveInst || !ExistingWaveInst->WaveData
				|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
			{
				OrderedIndex = Index;
				break;
			}
		}
		if (OrderedIndex != -1)
		{
			StreamingSoundSources.Insert(SoundSource, OrderedIndex);
		}
		else if (StreamingSoundSources.Num() < MaxStreams)
		{
			StreamingSoundSources.AddUnique(SoundSource);
		}

		for (int32 Index = StreamingSoundSources.Num()-1; Index >= MaxStreams; --Index)
		{
			StreamingSoundSources[Index]->Stop();
		}
	}
}

void FAudioStreamingManager::RemoveStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->WaveData && WaveInstance->WaveData->IsStreaming())
	{
		FScopeLock Lock(&CriticalSection);

		// Make sure there is a request so that unused chunks
		// can be cleared if this was the last playing instance
		GetWaveRequest(WaveInstance->WaveData);
		StreamingSoundSources.Remove(SoundSource);
	}
}

bool FAudioStreamingManager::IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const
{
	FScopeLock Lock(&CriticalSection);
	return StreamingSoundSources.FindByKey(SoundSource) != NULL;
}

const uint8* FAudioStreamingManager::GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex, uint32* OutChunkSize) const
{
	FScopeLock Lock(&CriticalSection);

	// Check for the spoof of failing to load a stream chunk
	if (SpoofFailedStreamChunkLoad > 0)
	{
		return nullptr;
	}

	const FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		if (WaveData->LoadedChunkIndices.Contains(ChunkIndex))
		{
			for (int32 Index = 0; Index < WaveData->LoadedChunks.Num(); ++Index)
			{
				if (WaveData->LoadedChunks[Index].Index == ChunkIndex)
				{
					if(OutChunkSize != NULL)
					{
						*OutChunkSize = WaveData->LoadedChunks[Index].DataSize;
					}
					
					return WaveData->LoadedChunks[Index].Data;
				}
			}
		}
	}
	return NULL;
}

FWaveRequest& FAudioStreamingManager::GetWaveRequest(USoundWave* SoundWave)
{
	FWaveRequest* WaveRequest = WaveRequests.Find(SoundWave);
	if (!WaveRequest)
	{
		// Setup the new request so it always asks for chunk 0
		WaveRequest = &WaveRequests.Add(SoundWave);
		WaveRequest->RequiredIndices.AddUnique(0);
		WaveRequest->bPrioritiseRequest = false;
	}
	return *WaveRequest;
}
