// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioStreaming.h: Definitions of classes used for audio streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/IndirectArray.h"
#include "Containers/Queue.h"
#include "Stats/Stats.h"
#include "ContentStreaming.h"
#include "Async/AsyncWork.h"
#include "Async/AsyncFileHandle.h"

class FSoundSource;
class USoundWave;
struct FAudioStreamingManager;
struct FWaveInstance;

/** Lists possible states used by Thread-safe counter. */
enum EAudioStreamingState
{
	// There are no pending requests/ all requests have been fulfilled.
	AudioState_ReadyFor_Requests = 0,
	// Initial request has completed and finalization needs to be kicked off.
	AudioState_ReadyFor_Finalization = 1,
	// We're currently loading in chunk data.
	AudioState_InProgress_Loading = 2,
	// ...
	// States InProgress_Loading+N-1 means we're currently loading in N chunks
	// ...
};

/**
 * Async worker to stream audio chunks from the derived data cache.
 */
class FAsyncStreamDerivedChunkWorker : public FNonAbandonableTask
{
public:
	/** Initialization constructor. */
	FAsyncStreamDerivedChunkWorker(
		const FString& InDerivedDataKey,
		void* InDestChunkData,
		int32 InChunkSize,
		FThreadSafeCounter* InThreadSafeCounter
		);
	
	/**
	 * Retrieves the derived chunk from the derived data cache.
	 */
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncStreamDerivedChunkWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	/**
	 * Returns true if the streaming mip request failed.
	 */
	bool DidRequestFail() const
	{
		return bRequestFailed;
	}

private:
	/** Key for retrieving chunk data from the derived data cache. */
	FString DerivedDataKey;
	/** The location to which the chunk data should be copied. */
	void* DestChunkData;
	/** The size of the chunk in bytes. */
	int32 ExpectedChunkSize;
	/** true if the chunk data was not present in the derived data cache. */
	bool bRequestFailed;
	/** Thread-safe counter to decrement when data has been copied. */
	FThreadSafeCounter* ThreadSafeCounter;
};

/** Async task to stream chunks from the derived data cache. */
typedef FAsyncTask<FAsyncStreamDerivedChunkWorker> FAsyncStreamDerivedChunkTask;

/**
 * Contains a request to load chunks of a sound wave
 */
struct FWaveRequest
{
	TArray<uint32>	RequiredIndices;
	bool			bPrioritiseRequest;
};

/**
 * Stores info about an audio chunk once it's been loaded
 */
struct FLoadedAudioChunk
{
	uint8*	Data;
	class IAsyncReadRequest* IORequest;
	int32	MemorySize; 
	int32	DataSize;
	uint32	Index;

	FLoadedAudioChunk()
		: Data(nullptr)
		, IORequest(nullptr)
		, MemorySize(0)
		, DataSize(0)
		, Index(0)
	{
	}

	~FLoadedAudioChunk()
	{
		checkf(Data == nullptr, TEXT("Audio chunk Data ptr not null (%p), DataSize: %d"), Data, DataSize);
	}
};

/**
 * Contains everything that will be needed by a SoundWave that's streaming in data
 */
struct FStreamingWaveData final
{
	FStreamingWaveData();
	~FStreamingWaveData();

	/**
	 * Sets up the streaming wave data and loads the first chunk of audio for instant play
	 *
	 * @param SoundWave	The SoundWave we are managing
	 */
	bool Initialize(USoundWave* SoundWave, FAudioStreamingManager* InStreamingManager);

	/**
	 * Updates the streaming status of the sound wave and performs finalization when appropriate. The function returns
	 * true while there are pending requests in flight and updating needs to continue.
	 *
	 * @return					true if there are requests in flight, false otherwise
	 */
	bool UpdateStreamingStatus();

	/**
	 * Tells the SoundWave which chunks are currently required so that it can start loading any needed
	 *
	 * @param InChunkIndices The Chunk Indices that are currently needed by all sources using this sound
	 * @param bShouldPrioritizeAsyncIORequest Whether request should have higher priority than usual
	 */
	void UpdateChunkRequests(FWaveRequest& InWaveRequest);

		/**
	 * Checks whether the requested chunk indices differ from those loaded
	 *
	 * @param IndicesToLoad		List of chunk indices that should be loaded
	 * @param IndicesToFree		List of chunk indices that should be freed
	 * @return Whether any changes to loaded chunks are required
	 */
	bool HasPendingRequests(TArray<uint32>& IndicesToLoad, TArray<uint32>& IndicesToFree) const;

	/**
	 * Kicks off any pending requests
	 */
	void BeginPendingRequests(const TArray<uint32>& IndicesToLoad, const TArray<uint32>& IndicesToFree);

	/**
	* Blocks till all pending requests are fulfilled.
	*
	* @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	* @return				Return true if there are no requests left in flight, false if the time limit was reached before they were finished.
	*/
	bool BlockTillAllRequestsFinished(float TimeLimit = 0.0f);

#if WITH_EDITORONLY_DATA
	/**
	 * Finishes any Derived Data Cache requests that may be in progress
	 *
	 * @return Whether any of the requests failed.
	 */
	bool FinishDDCRequests();
#endif //WITH_EDITORONLY_DATA

private:
	// Don't allow copy construction as it could free shared memory
	FStreamingWaveData(const FStreamingWaveData& that);
	FStreamingWaveData& operator=(FStreamingWaveData const&);

	// Creates a new chunk, returns the chunk index
	int32 AddNewLoadedChunk(int32 ChunkSize);
	void FreeLoadedChunk(FLoadedAudioChunk& LoadedChunk);

public:
	/** SoundWave this streaming data is for */
	USoundWave* SoundWave;

	/** Thread-safe counter indicating the audio streaming state. */
	mutable FThreadSafeCounter	PendingChunkChangeRequestStatus;

	/* Contains pointers to Chunks of audio data that have been streamed in */
	TArray<FLoadedAudioChunk> LoadedChunks;

	class IAsyncReadFileHandle* IORequestHandle;

	/** Indices of chunks that are currently loaded */
	TArray<uint32>	LoadedChunkIndices;

	/** Indices of chunks we want to have loaded */
	FWaveRequest	CurrentRequest;

#if WITH_EDITORONLY_DATA
	/** Pending async derived data streaming tasks */
	TIndirectArray<FAsyncStreamDerivedChunkTask> PendingAsyncStreamDerivedChunkTasks;
#endif // #if WITH_EDITORONLY_DATA

	/** Ptr to owning audio streaming manager. */
	FAudioStreamingManager* AudioStreamingManager;
};

/** Struct used to store results of an async file load. */
struct FASyncAudioChunkLoadResult
{
	// Place to safely copy the ptr of a loaded audio chunk when load result is finished
	uint8* DataResults;

	// Actual storage of the loaded audio chunk, will be filled on audio thread.
	FStreamingWaveData* StreamingWaveData;

	// Loaded audio chunk index
	int32 LoadedAudioChunkIndex;

	FASyncAudioChunkLoadResult()
		: DataResults(nullptr)
		, StreamingWaveData(nullptr)
		, LoadedAudioChunkIndex(INDEX_NONE)
	{}
};


/**
* Streaming manager dealing with audio.
*/
struct FAudioStreamingManager : public IAudioStreamingManager
{
	/** Constructor, initializing all members */
	FAudioStreamingManager();

	virtual ~FAudioStreamingManager();

	// IStreamingManager interface
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;
	virtual void CancelForcedResources() override;
	virtual void NotifyLevelChange() override;
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;
	virtual void AddLevel( class ULevel* Level ) override;
	virtual void RemoveLevel( class ULevel* Level ) override;
	virtual void NotifyLevelOffset( class ULevel* Level, const FVector& Offset ) override;
	// End IStreamingManager interface

	// IAudioStreamingManager interface
	virtual void AddStreamingSoundWave(USoundWave* SoundWave) override;
	virtual void RemoveStreamingSoundWave(USoundWave* SoundWave) override;
	virtual bool IsManagedStreamingSoundWave(const USoundWave* SoundWave) const override;
	virtual bool IsStreamingInProgress(const USoundWave* SoundWave) override;
	virtual bool CanCreateSoundSource(const FWaveInstance* WaveInstance) const override;
	virtual void AddStreamingSoundSource(FSoundSource* SoundSource) override;
	virtual void RemoveStreamingSoundSource(FSoundSource* SoundSource) override;
	virtual bool IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const override;
	virtual const uint8* GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex, uint32* OutChunkSize = NULL) const override;
	// End IAudioStreamingManager interface

	/** Called when an async callback is made on an async loading audio chunk request. */
	void OnAsyncFileCallback(FStreamingWaveData* StreamingWaveData, int32 LoadedAudioChunkIndex, IAsyncReadRequest* ReadRequest);

	/** Processes pending async file IO results. */
	void ProcessPendingAsyncFileResults();

protected:

	/**
	 * Gets Wave request associated with a specific wave
	 *
	 * @param SoundWave		SoundWave we want request for
	 * @return Existing or new request structure
	 */
	FWaveRequest& GetWaveRequest(USoundWave* SoundWave);

	/** Sound Waves being managed. */
	TMap<USoundWave*, FStreamingWaveData*> StreamingSoundWaves;

	/** Sound Sources being managed. */
	TArray<FSoundSource*>	StreamingSoundSources;

	/** Map of requests to make next time sound waves are ready */
	TMap<USoundWave*, FWaveRequest> WaveRequests;

	/** Results of async loading audio chunks. */
	TQueue<FASyncAudioChunkLoadResult*> AsyncAudioStreamChunkResults;

	/** Critical section to protect usage of shared gamethread/audiothread members */
	mutable FCriticalSection CriticalSection;
};
