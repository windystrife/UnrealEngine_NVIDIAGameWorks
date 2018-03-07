// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ContentStreaming.h: Definitions of classes used for content streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class FSoundSource;
class UPrimitiveComponent;
class USoundWave;
class UTexture2D;
struct FStreamingManagerTexture;
struct FWaveInstance;

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

// Forward declarations
class UPrimitiveComponent;
class AActor;
class UTexture2D;
class FSoundSource;
class USoundWave;
struct FWaveInstance;
struct FStreamingManagerTexture;

/** Helper function to flush resource streaming. */
void FlushResourceStreaming();

/*-----------------------------------------------------------------------------
	Base streaming classes.
-----------------------------------------------------------------------------*/

enum EDynamicPrimitiveType
{
	DPT_Level,
	DPT_Spawned,
	DPT_MAX
};

enum ERemoveStreamingViews
{
	/** Removes normal views, but leaves override views. */
	RemoveStreamingViews_Normal,
	/** Removes all views. */
	RemoveStreamingViews_All
};

/**
 * Helper structure containing all relevant information for streaming.
 */
struct FStreamingViewInfo
{
	FStreamingViewInfo( const FVector& InViewOrigin, float InScreenSize, float InFOVScreenSize, float InBoostFactor, bool bInOverrideLocation, float InDuration, TWeakObjectPtr<AActor> InActorToBoost )
	:	ViewOrigin( InViewOrigin )
	,	ScreenSize( InScreenSize )
	,	FOVScreenSize( InFOVScreenSize )
	,	BoostFactor( InBoostFactor )
	,	Duration( InDuration )
	,	bOverrideLocation( bInOverrideLocation )
	,	ActorToBoost( InActorToBoost )
	{
	}
	/** View origin */
	FVector ViewOrigin;
	/** Screen size, not taking FOV into account */
	float	ScreenSize;
	/** Screen size, taking FOV into account */
	float	FOVScreenSize;
	/** A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
	float	BoostFactor;
	/** How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick. */
	float	Duration;
	/** Whether this is an override location, which forces the streaming system to ignore all other regular locations */
	bool	bOverrideLocation;
	/** Optional pointer to an actor who's textures should have their streaming priority boosted */
	TWeakObjectPtr<AActor> ActorToBoost;
};

/**
 * Pure virtual base class of a streaming manager.
 */
struct IStreamingManager
{
	IStreamingManager()
	:	NumWantingResources(0)
	,	NumWantingResourcesCounter(0)
	{
	}

	/** Virtual destructor */
	virtual ~IStreamingManager()
	{}

	ENGINE_API static struct FStreamingManagerCollection& Get();

	/** Same as get but could fail if state not allocated or shutdown. */
	ENGINE_API static struct FStreamingManagerCollection* Get_Concurrent();

	ENGINE_API static void Shutdown();

	/** Checks if the streaming manager has already been shut down. **/
	ENGINE_API static bool HasShutdown();

	/**
	 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	ENGINE_API virtual void Tick( float DeltaTime, bool bProcessEverything=false );

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) = 0;

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 StreamAllResources( float TimeLimit=0.0f )
	{
		return 0;
	}

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) = 0;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() = 0;

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange() = 0;

	/**
	 * Removes streaming views from the streaming manager. This is also called by Tick().
	 *
	 * @param RemovalType	What types of views to remove (all or just the normal views)
	 */
	void RemoveStreamingViews( ERemoveStreamingViews RemovalType );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
	 */
	ENGINE_API void AddViewInformation( const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor=1.0f, bool bOverrideLocation=false, float Duration=0.0f, TWeakObjectPtr<AActor> InActorToBoost = NULL );

	/**
	 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
	 * re-using the screensize and FOV settings.
	 *
	 * @param SlaveLocation			World-space view origin
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 */
	ENGINE_API void AddViewSlaveLocation( const FVector& SlaveLocation, float BoostFactor=1.0f, bool bOverrideLocation=false, float Duration=0.0f );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) = 0;

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) 
	{
		return false;
	}

	/** Adds a ULevel that has already prepared StreamingData to the streaming manager. */
	virtual void AddLevel( class ULevel* Level ) = 0;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) = 0;

	/**
	 * Notifies manager that level primitives were shifted
	 */
	virtual void NotifyLevelOffset( class ULevel* Level, const FVector& Offset ) = 0;

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor )
	{
	}

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor )
	{
	}

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
	{
	}

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
	{
	}

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated_Concurrent( const UPrimitiveComponent* Primitive )
	{
	}

	/** Returns the number of view infos. */
	ENGINE_API int32 GetNumViews() const
	{
		return CurrentViewInfos.Num();
	}

	/** Returns the view info by the specified index. */
	ENGINE_API const FStreamingViewInfo& GetViewInformation( int32 ViewIndex ) const
	{
		return CurrentViewInfos[ ViewIndex ];
	}

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual int32 GetNumWantingResources() const
	{
		return NumWantingResources;
	}

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is incremented every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual int32 GetNumWantingResourcesID() const
	{
		return NumWantingResourcesCounter;
	}

	/** Propagates a change to the active lighting scenario. */
	virtual void PropagateLightingScenarioChange() 
	{
	}

protected:

	/**
	 * Sets up the CurrentViewInfos array based on PendingViewInfos, LastingViewInfos and SlaveLocations.
	 * Removes out-dated LastingViewInfos.
	 *
	 * @param DeltaTime		Time since last call in seconds
	 */
	void SetupViewInfos( float DeltaTime );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ViewInfos				Array to add the view to
	 * @param ViewOrigin			View origin
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
	 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
	 */
	static void AddViewInfoToArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor, bool bOverrideLocation, float Duration, TWeakObjectPtr<AActor> InActorToBoost );

	/**
	 * Remove view infos with the same location from the given array.
	 *
	 * @param ViewInfos				[in/out] Array to remove the view from
	 * @param ViewOrigin			View origin
	 */
	static void RemoveViewInfoFromArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin );

	struct FSlaveLocation
	{
		FSlaveLocation( const FVector& InLocation, float InBoostFactor, bool bInOverrideLocation, float InDuration )
		:	Location( InLocation )
		,	BoostFactor( InBoostFactor )
		,	Duration( InDuration )
		,	bOverrideLocation( bInOverrideLocation )
		{
		}
		/** A location to use for distance-based heuristics next Tick(). */
		FVector		Location;
		/** A boost factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
		float		BoostFactor;
		/** How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick. */
		float		Duration;
		/** Whether this is an override location, which forces the streaming system to ignore all other locations */
		bool		bOverrideLocation;
	};

	/** Current collection of views that need to be taken into account for streaming. Emptied every frame. */
	ENGINE_API static TArray<FStreamingViewInfo> CurrentViewInfos;

	/** Pending views. Emptied every frame. */
	static TArray<FStreamingViewInfo> PendingViewInfos;

	/** Views that stick around for a while. Override views are ignored if no movie is playing. */
	static TArray<FStreamingViewInfo> LastingViewInfos;

	/** Collection of view locations that will be added at the next call to AddViewInformation. */
	static TArray<FSlaveLocation> SlaveLocations;

	/** Set when Tick() has been called. The first time a new view is added, it will clear out all old views. */
	static bool bPendingRemoveViews;

	/** Number of resources that currently wants to be streamed in. */
	int32		NumWantingResources;

	/**
	 * The current counter for NumWantingResources.
	 * This counter is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current counter with
	 * what it was when the changes were made.
	 */
	int32		NumWantingResourcesCounter;
};

/**
 * Interface to add functions specifically related to texture streaming
 */
struct ITextureStreamingManager : public IStreamingManager
{
	/**
	* Updates streaming for an individual texture, taking into account all view infos.
	*
	* @param Texture		Texture to update
	*/
	virtual void UpdateIndividualTexture(UTexture2D* Texture) = 0;

	/**
	* Temporarily boosts the streaming distance factor by the specified number.
	* This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	*/
	virtual void BoostTextures(AActor* Actor, float BoostFactor) = 0;

	/**
	*	Try to stream out texture mip-levels to free up more memory.
	*	@param RequiredMemorySize	- Required minimum available texture memory
	*	@return						- Whether it succeeded or not
	**/
	virtual bool StreamOutTextureData(int64 RequiredMemorySize) = 0;

	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture(UTexture2D* Texture) = 0;

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture(UTexture2D* Texture) = 0;

	virtual int64 GetMemoryOverBudget() const = 0;

	/** Pool size for streaming. */
	virtual int64 GetPoolSize() const = 0;

	/** Max required textures ever seen in bytes. */
	virtual int64 GetMaxEverRequired() const = 0;

	/** Resets the max ever required textures.  For possibly when changing resolutions or screen pct. */
	virtual void ResetMaxEverRequired() = 0;

	/** Set current pause state for texture streaming */
	virtual void PauseTextureStreaming(bool bInShouldPause) = 0;

	/** Return all bounds related to the ref object */
	ENGINE_API virtual void GetObjectReferenceBounds(const UObject* RefObject, TArray<FBox>& AssetBoxes) = 0;
};

/**
 * Interface to add functions specifically related to audio streaming
 */
struct IAudioStreamingManager : public IStreamingManager
{
	/** Adds a new Sound Wave to the streaming manager. */
	virtual void AddStreamingSoundWave(USoundWave* SoundWave) = 0;

	/** Removes a Sound Wave from the streaming manager. */
	virtual void RemoveStreamingSoundWave(USoundWave* SoundWave) = 0;

	/** Returns true if this is a Sound Wave that is managed by the streaming manager. */
	virtual bool IsManagedStreamingSoundWave(const USoundWave* SoundWave) const = 0;

	/** Returns true if this Sound Wave is currently streaming a chunk. */
	virtual bool IsStreamingInProgress(const USoundWave* SoundWave) = 0;

	virtual bool CanCreateSoundSource(const FWaveInstance* WaveInstance) const = 0;

	/** Adds a new Sound Source to the streaming manager. */
	virtual void AddStreamingSoundSource(FSoundSource* SoundSource) = 0;

	/** Removes a Sound Source from the streaming manager. */
	virtual void RemoveStreamingSoundSource(FSoundSource* SoundSource) = 0;

	/** Returns true if this is a streaming Sound Source that is managed by the streaming manager. */
	virtual bool IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const = 0;

	/**
	 * Gets a pointer to a chunk of audio data
	 *
	 * @param SoundWave		SoundWave we want a chunk from
	 * @param ChunkIndex	Index of the chunk we want
	 * @return Either the desired chunk or NULL if it's not loaded
	 */
	virtual const uint8* GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex, uint32* OutChunkSize = NULL) const = 0;
};

/**
 * Streaming manager collection, routing function calls to streaming managers that have been added
 * via AddStreamingManager.
 */
struct FStreamingManagerCollection : public IStreamingManager
{
	/** Default constructor, initializing all member variables. */
	ENGINE_API FStreamingManagerCollection();

	/**
	 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	ENGINE_API virtual void Tick( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 StreamAllResources( float TimeLimit=0.0f ) override;

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual int32 GetNumWantingResources() const override;

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual int32 GetNumWantingResourcesID() const override;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() override;

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange() override;

	/**
	 * Checks whether any kind of streaming is active
	 */
	ENGINE_API bool IsStreamingEnabled() const;

	/**
	 * Checks whether texture streaming is active
	 */
	virtual bool IsTextureStreamingEnabled() const;

	/**
	 * Gets a reference to the Texture Streaming Manager interface
	 */
	ENGINE_API ITextureStreamingManager& GetTextureStreamingManager() const;

	/**
	 * Gets a reference to the Audio Streaming Manager interface
	 */
	ENGINE_API IAudioStreamingManager& GetAudioStreamingManager() const;

	/**
	 * Adds a streaming manager to the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to add
	 */
	ENGINE_API void AddStreamingManager( IStreamingManager* StreamingManager );

	/**
	 * Removes a streaming manager from the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to remove
	 */
	ENGINE_API void RemoveStreamingManager( IStreamingManager* StreamingManager );

	/**
	 * Sets the number of iterations to use for the next time UpdateResourceStreaming is being called. This 
	 * is being reset to 1 afterwards.
	 *
	 * @param NumIterations	Number of iterations to perform the next time UpdateResourceStreaming is being called.
	 */
	void SetNumIterationsForNextFrame( int32 NumIterations );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;

	/**
	 * Disables resource streaming. Enable with EnableResourceStreaming. Disable/enable can be called multiple times nested
	 */
	void DisableResourceStreaming();

	/**
	 * Enables resource streaming, previously disabled with enableResourceStreaming. Disable/enable can be called multiple times nested
	 * (this will only actually enable when all disables are matched with enables)
	 */
	void EnableResourceStreaming();

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level ) override;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) override;
	
	/* Notifies manager that level primitives were shifted. */
	virtual void NotifyLevelOffset( class ULevel* Level, const FVector& Offset ) override;

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

	/** Propagates a change to the active lighting scenario. */
	void PropagateLightingScenarioChange() override;

protected:

	virtual void AddOrRemoveTextureStreamingManagerIfNeeded(bool bIsInit=false);

	/** Array of streaming managers to route function calls to */
	TArray<IStreamingManager*> StreamingManagers;
	/** Number of iterations to perform. Gets reset to 1 each frame. */
	int32 NumIterations;

	/** Count of how many nested DisableResourceStreaming's were called - will enable when this is 0 */
	int32 DisableResourceStreamingCount;

	/** Maximum number of seconds to block in StreamAllResources(), by default (.ini setting). */
	float LoadMapTimeLimit;

	/** The currently added texture streaming manager. Can be NULL*/
	FStreamingManagerTexture* TextureStreamingManager;

	/** The audio streaming manager, should always exist */
	IAudioStreamingManager* AudioStreamingManager;
};

