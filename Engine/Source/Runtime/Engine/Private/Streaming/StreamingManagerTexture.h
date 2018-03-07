// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ContentStreaming.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Streaming/StreamingTexture.h"
#include "Streaming/LevelTextureManager.h"
#include "Streaming/TextureInstanceTask.h"

class AActor;
class FAsyncTextureStreamingTask;
class UPrimitiveComponent;

template<typename TTask> class FAsyncTask;

#define STATS_FAST 0

/*-----------------------------------------------------------------------------
	Texture streaming.
-----------------------------------------------------------------------------*/

/**
 * Streaming manager dealing with textures.
 */
struct FStreamingManagerTexture : public ITextureStreamingManager
{
	/** Constructor, initializing all members */
	FStreamingManagerTexture();

	virtual ~FStreamingManagerTexture();

	/** Called before GC to clear pending kill levels. */
	void OnPreGarbageCollect();

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Updates streaming for an individual texture, taking into account all view infos.
	 *
	 * @param Texture	Texture to update
	 */
	virtual void UpdateIndividualTexture( UTexture2D* Texture ) override;

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() override;

	/**
	 * Notifies manager of "level" change so it can prioritize character textures for a few frames.
	 */
	virtual void NotifyLevelChange() override;

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;

	/**
	 *	Try to stream out texture mip-levels to free up more memory.
	 *	@param RequiredMemorySize	- Required minimum available texture memory
	 *	@return						- Whether it succeeded or not
	 **/
	virtual bool StreamOutTextureData( int64 RequiredMemorySize ) override;

	virtual int64 GetMemoryOverBudget() const override { return MemoryOverBudget; }

	/** Pool size for streaming. */
	virtual int64 GetPoolSize() const override { return GTexturePoolSize;  }

	virtual int64 GetMaxEverRequired() const override { return MaxEverRequired; }

	virtual void ResetMaxEverRequired() override { MaxEverRequired = 0; }

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/**
	 * Exec command handlers
	 */
#if STATS_FAST
	bool HandleDumpTextureStreamingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // STATS_FAST
#if !UE_BUILD_SHIPPING
	bool HandleListStreamingTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleResetMaxEverRequiredTexturesCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleLightmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCancelTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleShadowmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNumStreamedMipsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDebugTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleUntrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStreamOutCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePauseTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStreamingManagerMemoryCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleTextureGroupsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleInvestigateTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
#endif // !UE_BUILD_SHIPPING
	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture( UTexture2D* Texture ) override;

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture( UTexture2D* Texture ) override;

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level ) override;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) override;

	/* Notifies manager that level primitives were shifted */
	virtual void NotifyLevelOffset(ULevel* Level, const FVector& Offset) override;
	
	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor ) override;

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor ) override;

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType ) override;

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive ) override;

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated_Concurrent( const UPrimitiveComponent* Primitive ) override;

	/** Returns the corresponding FStreamingTexture for a UTexture2D. */
	FStreamingTexture* GetStreamingTexture( const UTexture2D* Texture2D );

	/** Set current pause state for texture streaming */
	virtual void PauseTextureStreaming(bool bInShouldPause) override
	{
		bPauseTextureStreaming = bInShouldPause;
	}

	/** Return all bounds related to the ref object */
	virtual void GetObjectReferenceBounds(const UObject* RefObject, TArray<FBox>& AssetBoxes) override;

	/** Propagates a change to the active lighting scenario. */
	void PropagateLightingScenarioChange() override;

protected:
//BEGIN: Thread-safe functions and data
		friend class FAsyncTextureStreamingTask;

		/** Remove any references in level managers to this component */
		void RemoveStaticReferences(const UPrimitiveComponent* Primitive);

		/**
		 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
		 * allowing their streaming state to progress.
		 *
		 * @param StageIndex		Current stage index
		 * @param NumUpdateStages	Number of texture update stages
		 */
		void UpdateStreamingTextures( int32 StageIndex, int32 NumStages, bool bWaitForMipFading );

		void ProcessRemovedTextures();
		void ProcessAddedTextures();
		void ConditionalUpdateStaticData();

		/** Adds new textures and level data on the gamethread (while the worker thread isn't active). */
		void PrepareAsyncTask( bool bProcessEverything );

		/** Checks for updates in the user settings (CVars, etc). */
		void CheckUserSettings();

		/**
		 * Temporarily boosts the streaming distance factor by the specified number.
		 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
		 */
		void BoostTextures( AActor* Actor, float BoostFactor ) override;

		/**
		 * Stream textures in/out, based on the priorities calculated by the async work.
		 *
		 * @param bProcessEverything	Whether we're processing all textures in one go
		 */
		void StreamTextures( bool bProcessEverything );

		/** All streaming UTexture2D objects. */
		TArray<FStreamingTexture> StreamingTextures;

		/** All the textures referenced in StreamingTextures. Used to handled deleted textures.  */
		TSet<const UTexture2D*> ReferencedTextures;

		/** Index of the StreamingTexture that will be updated next by UpdateStreamingTextures(). */
		int32 CurrentUpdateStreamingTextureIndex;
//END: Thread-safe functions and data

	/**
	 * Mark the textures with a timestamp. They're about to lose their location-based heuristic and we don't want them to
	 * start using LastRenderTime heuristic for a few seconds until they are garbage collected!
	 *
	 * @param RemovedTextures	List of removed textures.
	 */
	void	SetTexturesRemovedTimestamp(const FRemovedTextureArray& RemovedTextures);

	void	DumpTextureGroupStats( bool bDetailedStats );

	void	SetLastUpdateTime();
	void	UpdateStats();
	void	LogViewLocationChange();

	void	IncrementalUpdate( float Percentage, bool bUpdateDynamicComponents);

	/**
	 * Update all pending states.
	 *
	 * @param bUpdateDynamicComponents		Whether dynamic component state should also be updated.
	 */
	void	UpdatePendingStates(bool bUpdateDynamicComponents);

	/**
	 * Complete all pending async work and complete all pending state updates.
	 *
	 * @param bCompleteFullUpdateCycle		Whether to complete the full update cycle usually spread accross several frames.
	 */
	void	SyncStates(bool bCompleteFullUpdateCycle);

	/** Next sync, dump texture group stats. */
	bool	bTriggerDumpTextureGroupStats;

	/** Whether to the dumped texture group stats should contain extra information. */
	bool	bDetailedDumpTextureGroupStats;

	/** Cached from the system settings. */
	int32 NumStreamedMips[TEXTUREGROUP_MAX];

	FTextureStreamingSettings Settings;

	/** Async work for calculating priorities for all textures. */
	FAsyncTask<FAsyncTextureStreamingTask>*	AsyncWork;

	/** Async work for texture instance managers. */
	TRefCountPtr<TextureInstanceTask::FDoWorkAsyncTask> TextureInstanceAsyncWork;

	/** Textures from dynamic primitives. Owns the data for all levels. */
	FDynamicTextureInstanceManager DynamicComponentManager;

	/** New textures, before they've been added to the thread-safe container. */
	TArray<UTexture2D*>	PendingStreamingTextures;

	/** The list of indices with null texture in StreamingTextures. */
	TArray<int32>	RemovedTextureIndices;

	/** Level data */
	TIndirectArray<FLevelTextureManager> LevelTextureManagers;

	/** Stages [0,N-2] is non-threaded data collection, Stage N-1 is wait-for-AsyncWork-and-finalize. */
	int32					ProcessingStage;

	/** Total number of processing stages (N). */
	int32					NumTextureProcessingStages;

	/** Whether to support texture instance streaming for dynamic (movable/spawned) objects. */
	bool					bUseDynamicStreaming;

	float					BoostPlayerTextures;

	/** Amount of memory to leave free in the texture pool. */
	int64					MemoryMargin;

	/** Minimum number of bytes to evict when we need to stream out textures because of a failed allocation. */
	int64					MinEvictSize;

	/** The actual memory pool size available to stream textures, excludes non-streaming texture, temp memory (for streaming mips), memory margin (allocator overhead). */
	int64					EffectiveStreamingPoolSize;

	// Stats we need to keep across frames as we only iterate over a subset of textures.

	int64 MemoryOverBudget;
	int64 MaxEverRequired;

	/** Whether texture streaming is paused or not. When paused, it won't stream any textures in or out. */
	bool bPauseTextureStreaming;

	/** Last time all data were fully updated. Instances are considered visible if they were rendered between that last time and the current time. */
	float LastWorldUpdateTime;

	FTextureStreamingStats DisplayedStats;
	FTextureStreamingStats GatheredStats;

	TArray<int32> InflightTextures;

#if STATS_FAST
	uint64 MaxStreamingTexturesSize;
	uint64 MaxOptimalTextureSize;
	int64 MaxStreamingOverBudget;
	uint64 MaxTexturePoolAllocatedSize;
	uint32 MaxNumWantingTextures;
#endif
	
	volatile int32 ConcurrentLockState;

	friend bool TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, bool bForceMipLevelsToBeResident, const FStreamingManagerTexture* Manager);
};
