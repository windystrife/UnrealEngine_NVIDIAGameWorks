// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LevelTick.cpp: Level timer tick function
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/CoreMisc.h"
#include "Stats/Stats.h"
#include "Misc/TimeGuard.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/GarbageCollection.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/PlayerController.h"
#include "SceneUtils.h"
#include "ParticleHelper.h"
#include "Engine/LevelStreaming.h"
#include "Engine/NetConnection.h"
#include "UnrealEngine.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/WorldComposition.h"
#include "Collision.h"
#include "PhysicsPublic.h"
#include "Tickable.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "TimerManager.h"
#include "Camera/CameraPhotography.h"
#include "HAL/LowLevelMemTracker.h"

//#include "SoundDefinitions.h"
#include "FXSystem.h"
#include "TickTaskManagerInterface.h"
#include "HAL/IPlatformFileProfilerWrapper.h"
#if !UE_BUILD_SHIPPING
#include "VisualizerEvents.h"
#include "STaskGraph.h"
#endif
#include "Async/ParallelFor.h"
#include "Engine/CoreSettings.h"

#include "InGamePerformanceTracker.h"
#include "Streaming/TextureStreamingHelpers.h"

#if WITH_EDITOR
	#include "Editor.h"
#endif

// this will log out all of the objects that were ticked in the FDetailedTickStats struct so you can isolate what is expensive
#define LOG_DETAILED_DUMPSTATS 0

#define LOG_DETAILED_PATHFINDING_STATS 0

/** Global boolean to toggle the log of detailed tick stats. */
/** Needs LOG_DETAILED_DUMPSTATS to be 1 **/
bool GLogDetailedDumpStats = true; 

/** Game stats */

// DECLARE_CYCLE_STAT is the reverse of what will be displayed in the game's stat game

DEFINE_STAT(STAT_AsyncWorkWaitTime);
DEFINE_STAT(STAT_PhysicsTime);

DEFINE_STAT(STAT_SpawnActorTime);
DEFINE_STAT(STAT_ActorBeginPlay);

DEFINE_STAT(STAT_GCSweepTime);
DEFINE_STAT(STAT_GCMarkTime);

DEFINE_STAT(STAT_TeleportToTime);
DEFINE_STAT(STAT_MoveComponentTime);
DEFINE_STAT(STAT_MoveComponentSceneComponentTime);
DEFINE_STAT(STAT_UpdateOverlaps);
DEFINE_STAT(STAT_UpdatePhysicsVolume);
DEFINE_STAT(STAT_EndScopedMovementUpdate);

DEFINE_STAT(STAT_PostTickComponentLW);
DEFINE_STAT(STAT_PostTickComponentRecreate);
DEFINE_STAT(STAT_PostTickComponentUpdate);
DEFINE_STAT(STAT_PostTickComponentUpdateWait);

DEFINE_STAT(STAT_TickTime);
DEFINE_STAT(STAT_WorldTickTime);
DEFINE_STAT(STAT_UpdateCameraTime);
DEFINE_STAT(STAT_CharacterMovement);
DEFINE_STAT(STAT_PlayerControllerTick);

DEFINE_STAT(STAT_VolumeStreamingTickTime);
DEFINE_STAT(STAT_VolumeStreamingChecks);

DEFINE_STAT(STAT_NetWorldTickTime);
DEFINE_STAT(STAT_NavWorldTickTime);
DEFINE_STAT(STAT_ResetAsyncTraceTickTime);
DEFINE_STAT(STAT_TickableTickTime);
DEFINE_STAT(STAT_RuntimeMovieSceneTickTime);
DEFINE_STAT(STAT_FinishAsyncTraceTickTime);
DEFINE_STAT(STAT_NetBroadcastTickTime);
DEFINE_STAT(STAT_NetServerRepActorsTime);
DEFINE_STAT(STAT_NetConsiderActorsTime);
DEFINE_STAT(STAT_NetUpdateUnmappedObjectsTime);
DEFINE_STAT(STAT_NetInitialDormantCheckTime);
DEFINE_STAT(STAT_NetPrioritizeActorsTime);
DEFINE_STAT(STAT_NetReplicateActorsTime);
DEFINE_STAT(STAT_NetReplicateDynamicPropTime);
DEFINE_STAT(STAT_NetSkippedDynamicProps);
DEFINE_STAT(STAT_NetSerializeItemDeltaTime);
DEFINE_STAT(STAT_NetUpdateGuidToReplicatorMap);
DEFINE_STAT(STAT_NetReplicateStaticPropTime);
DEFINE_STAT(STAT_NetBroadcastPostTickTime);
DEFINE_STAT(STAT_NetRebuildConditionalTime);
DEFINE_STAT(STAT_PackageMap_SerializeObjectTime);

/*-----------------------------------------------------------------------------
	Externs.
-----------------------------------------------------------------------------*/

extern bool GShouldLogOutAFrameOfMoveComponent;
extern bool GShouldLogOutAFrameOfSetBodyTransform;

/*-----------------------------------------------------------------------------
	FTickableGameObject implementation.
-----------------------------------------------------------------------------*/

/** Static array of tickable objects */
TArray<FTickableGameObject*> FTickableGameObject::TickableObjects;
bool FTickableGameObject::bIsTickingObjects = false;

#if LOG_DETAILED_PATHFINDING_STATS
/** Global detailed pathfinding stats. */
FDetailedTickStats GDetailedPathFindingStats(30, 10, 1, 20, TEXT("pathfinding"));
#endif

/*-----------------------------------------------------------------------------
	Detailed tick stats helper classes.
-----------------------------------------------------------------------------*/

/** Constructor, private on purpose and initializing all members. */
FDetailedTickStats::FDetailedTickStats( int32 InNumObjectsToReport, float InTimeBetweenLogDumps, float InMinTimeBetweenLogDumps, float InTimesToReport, const TCHAR* InOperationPerformed )
:	GCIndex( 0 )
,   GCCallBackRegistered( false )
,	NumObjectsToReport( InNumObjectsToReport )
,	TimeBetweenLogDumps( InTimeBetweenLogDumps )
,	MinTimeBetweenLogDumps( InMinTimeBetweenLogDumps )
,	LastTimeOfLogDump( 0 )
,	TimesToReport( InTimesToReport )
,	OperationPerformed( InOperationPerformed )
{
}

/**  Destructor, unregisters the GC callback */
FDetailedTickStats::~FDetailedTickStats()
{
	// remove callback as we are dead
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(OnPreGarbageCollectDelegateHandle);
}

/**
 * Starts tracking an object and returns whether it's a recursive call or not. If it is recursive
 * the function will return false and EndObject should not be called on the object.
 *
 * @param	Object		Object to track
 * @return	false if object is already tracked and EndObject should NOT be called, true otherwise
 */
bool FDetailedTickStats::BeginObject( UObject* Object )
{
	// If object is already tracked, tell calling code to not track again.
	if( ObjectsInFlight.Contains( Object ) )
	{
		return false;
	}
	// Keep track of the fact that this object is being tracked.
	else
	{
		ObjectsInFlight.Add( Object );
		return true;
	}
}

/**
 * Add instance of object to stats
 *
 * @param Object	Object instance
 * @param DeltaTime	Time operation took this instance
 * @param   bForSummary Object should be used for high level summary
 */
void FDetailedTickStats::EndObject( UObject* Object, float DeltaTime, bool bForSummary )
{
	// Find existing entry and update it if found.
	int32* TickStatIndex = ObjectToStatsMap.Find( Object );
	bool bCreateNewEntry = true;
	if( TickStatIndex )
	{
		FTickStats* TickStats = &AllStats[*TickStatIndex];
		// If GC has occurred since we last checked, we need to validate that this is still the correct object
		if (TickStats->GCIndex == GCIndex || // was checked since last GC
			(Object->GetPathName() == TickStats->ObjectPathName && Object->GetClass()->GetFName() == TickStats->ObjectClassFName)) // still refers to the same object
		{
			TickStats->GCIndex = GCIndex;
			TickStats->TotalTime += DeltaTime;
			TickStats->Count++;
			bCreateNewEntry = false;
		}
		// else this mapping is stale and the memory has been reused for a new object
	}
	// Create new entry.
	if (bCreateNewEntry)		
	{	
		// The GC callback cannot usually be registered at construction because this comes from a static data structure 
		// do it now if need be and it is ready
		if (!GCCallBackRegistered)
		{
			GCCallBackRegistered = true;
			// register callback so that we can avoid finding the wrong stats for new objects reusing memory that used to be associated with a different object
			OnPreGarbageCollectDelegateHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FDetailedTickStats::OnPreGarbageCollect);
		}

		FTickStats NewTickStats;
		NewTickStats.GCIndex			= GCIndex;
		NewTickStats.ObjectPathName		= Object->GetPathName();
		NewTickStats.ObjectDetailedInfo	= Object->GetDetailedInfo();
		NewTickStats.ObjectClassFName	= Object->GetClass()->GetFName();
		if (NewTickStats.ObjectDetailedInfo == TEXT("No_Detailed_Info_Specified"))
		{
			NewTickStats.ObjectDetailedInfo = TEXT(""); // This is a common, useless, case; save memory and clean up report by avoiding storing it
		}

		NewTickStats.Count			= 1;
		NewTickStats.TotalTime		= DeltaTime;
		NewTickStats.bForSummary	= bForSummary;
		int32 Index = AllStats.Add(NewTickStats);
		ObjectToStatsMap.Add( Object, Index );
	}
	// Object no longer is in flight at this point.
	ObjectsInFlight.Remove(Object);
}

/** Reset stats to clean slate. */
void FDetailedTickStats::Reset()
{
	AllStats.Empty();
	ObjectToStatsMap.Empty();
}

/** Dump gathered stats information to the log. */
void FDetailedTickStats::DumpStats()
{
	// Determine whether we should dump to the log.
	bool bShouldDump = false;
	
	// Dump request due to interval.
	if( FApp::GetCurrentTime() > (LastTimeOfLogDump + TimeBetweenLogDumps) )
	{
		bShouldDump = true;
	}
	
	// Dump request due to low framerate.
	float TotalTime = 0;
	for( TArray<FTickStats>::TIterator It(AllStats); It; ++It )
	{
		const FTickStats& TickStat = *It;
		if( TickStat.bForSummary == true )
		{
			TotalTime += TickStat.TotalTime;
		}
	}
	if( TotalTime * 1000 > TimesToReport )
	{
		bShouldDump = true;
	}

	// Only dump every TimeBetweenLogDumps seconds.
	if( bShouldDump 
	&& ((FApp::GetCurrentTime() - LastTimeOfLogDump) > MinTimeBetweenLogDumps) )
	{
		LastTimeOfLogDump = FApp::GetCurrentTime();

		// Array of stats, used for sorting.
		TArray<FTickStats> SortedTickStats;
		TArray<FTickStats> SortedTickStatsDetailed;
		// Populate from TArray in unsorted fashion.
		for( TArray<FTickStats>::TIterator It(AllStats); It; ++It )
		{
			const FTickStats& TickStat = *It;
			if(TickStat.bForSummary == true )
			{
				SortedTickStats.Add( TickStat );
			}
			else
			{
				SortedTickStatsDetailed.Add( TickStat );
			}
		}
		// Sort stats by total time spent.
		SortedTickStats.Sort( FTickStats() );
		SortedTickStatsDetailed.Sort( FTickStats() );

		// Keep track of totals.
		FTickStats Totals;
		Totals.TotalTime	= 0;
		Totals.Count		= 0;

		// Dump tick stats sorted by total time.
		UE_LOG(LogLevel, Log, TEXT("Per object stats, frame # %llu"), (uint64)GFrameCounter);
		for( int32 i=0; i<SortedTickStats.Num(); i++ )
		{
			const FTickStats& TickStats = SortedTickStats[i];
			if( i<NumObjectsToReport )
			{
				UE_LOG(LogLevel, Log, TEXT("%5.2f ms, %4i instances, avg cost %5.3f, %s"), 1000 * TickStats.TotalTime, TickStats.Count, (TickStats.TotalTime/TickStats.Count) * 1000, *TickStats.ObjectPathName ); 
			}
			Totals.TotalTime += TickStats.TotalTime;
			Totals.Count	 += TickStats.Count;
		}
		UE_LOG(LogLevel, Log, TEXT("Total time spent %s %4i instances: %5.2f"), *OperationPerformed, Totals.Count, Totals.TotalTime * 1000 );

#if LOG_DETAILED_DUMPSTATS
		if (GLogDetailedDumpStats)
		{
			Totals.TotalTime	= 0;
			Totals.Count		= 0;

			UE_LOG(LogLevel, Log, TEXT("Detailed object stats, frame # %i"), GFrameCounter);
			for( int32 i=0; i<SortedTickStatsDetailed.Num(); i++ )
			{
				const FTickStats& TickStats = SortedTickStatsDetailed(i);
				if( i<NumObjectsToReport*10 )
				{
					UE_LOG(LogLevel, Log, TEXT("avg cost %5.3f, %s %s"),(TickStats.TotalTime/TickStats.Count) * 1000, *TickStats.ObjectPathName, *TickStats.ObjectDetailedInfo ); 
				}
				Totals.TotalTime += TickStats.TotalTime;
				Totals.Count	 += TickStats.Count;
			}
			UE_LOG(LogLevel, Log, TEXT("Total time spent %s %4i instances: %5.2f"), *OperationPerformed, Totals.Count, Totals.TotalTime * 1000 );
		}
#endif // LOG_DETAILED_DUMPSTATS

	}
}


/**
 * Constructor, keeping track of object's class and start time.
 */
FScopedDetailTickStats::FScopedDetailTickStats( FDetailedTickStats& InDetailedTickStats, UObject* InObject )
:	Object( InObject )
,	StartCycles( FPlatformTime::Cycles() )
,	DetailedTickStats( InDetailedTickStats )
{
	bShouldTrackObjectClass = DetailedTickStats.BeginObject( Object->GetClass() );
	bShouldTrackObject = DetailedTickStats.BeginObject( Object );
}

/**
 * Destructor, calculating delta time and updating global helper.
 */
FScopedDetailTickStats::~FScopedDetailTickStats()
{
	const float DeltaTime = FPlatformTime::ToSeconds(FPlatformTime::Cycles() - StartCycles);	
	if( bShouldTrackObject )
	{
		DetailedTickStats.EndObject( Object, DeltaTime, false );
	}
	if( bShouldTrackObjectClass )
	{
		DetailedTickStats.EndObject( Object->GetClass(), DeltaTime, true );
	}
}




/* Controller Tick
Controllers are never animated, and do not look for an owner to be ticked before them
Non-player controllers don't support being an autonomous proxy
*/
void AController::TickActor( float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction )
{
	//root of tick hierarchy

	if (TickType == LEVELTICK_ViewportsOnly)
	{
		return;
	}

	if( !IsPendingKill() )
	{
		Tick(DeltaSeconds);	// perform any tick functions unique to an actor subclass
	}
}

////////////
// Timing //
////////////


/*-----------------------------------------------------------------------------
	Network client tick.
-----------------------------------------------------------------------------*/

void UWorld::TickNetClient( float DeltaSeconds )
{
	SCOPE_TIME_GUARD(TEXT("UWorld::TickNetClient"));

	// If our net driver has lost connection to the server,
	// and there isn't a PendingNetGame, throw a network failure error.
	if( NetDriver->ServerConnection->State == USOCK_Closed )
	{
		if (GEngine->PendingNetGameFromWorld(this) == NULL)
		{
			const FString Error = NSLOCTEXT("Engine", "ConnectionFailed", "Your connection to the host has been lost.").ToString();
			GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::ConnectionLost, Error);
		}
	}
}

/*-----------------------------------------------------------------------------
	Main level timer tick handler.
-----------------------------------------------------------------------------*/


bool UWorld::IsPaused() const
{
	// pause if specifically set or if we're waiting for the end of the tick to perform streaming level loads (so actors don't fall through the world in the meantime, etc)
	const AWorldSettings* Info = GetWorldSettings();
	return ( (Info && Info->Pauser != NULL && TimeSeconds >= PauseDelay) ||
				(bRequestedBlockOnAsyncLoading && GetNetMode() == NM_Client) ||
				(GEngine->ShouldCommitPendingMapChange(this)) ||
				(IsPlayInEditor() && bDebugPauseExecution) );
}


bool UWorld::IsCameraMoveable() const
{
	bool bIsCameraMoveable = (!IsPaused() || bIsCameraMoveableWhenPaused || IsPlayingReplay());
#if WITH_EDITOR
	// to fix UE-17047 Motion Blur exaggeration when Paused in Simulate:
	// Simulate is excluded as the camera can move which invalidates motionblur
	bIsCameraMoveable = bIsCameraMoveable || (GEditor && GEditor->bIsSimulatingInEditor);
#endif
	return bIsCameraMoveable;
}

/**
 * Streaming settings for levels which are determined visible by level streaming volumes.
 */
class FVisibleLevelStreamingSettings
{
public:
	FVisibleLevelStreamingSettings()
	{
		bShouldBeVisible		= false;
		bShouldBlockOnLoad		= false;
		bShouldChangeVisibility	= false;
	}

	FVisibleLevelStreamingSettings( EStreamingVolumeUsage Usage )
	{
		switch( Usage )
		{
		case SVB_Loading:
			bShouldBeVisible		= false;
			bShouldBlockOnLoad		= false;
			bShouldChangeVisibility	= false;
			break;
		case SVB_LoadingNotVisible:
			bShouldBeVisible		= false;
			bShouldBlockOnLoad		= false;
			bShouldChangeVisibility	= true;
			break;
		case SVB_LoadingAndVisibility:
			bShouldBeVisible		= true;
			bShouldBlockOnLoad		= false;
			bShouldChangeVisibility	= true;
			break;
		case SVB_VisibilityBlockingOnLoad:
			bShouldBeVisible		= true;
			bShouldBlockOnLoad		= true;
			bShouldChangeVisibility	= true;
			break;
		case SVB_BlockingOnLoad:
			bShouldBeVisible		= false;
			bShouldBlockOnLoad		= true;
			bShouldChangeVisibility	= false;
			break;
		default:
			UE_LOG(LogLevel, Fatal,TEXT("Unsupported usage %i"),(int32)Usage);
		}
	}

	FVisibleLevelStreamingSettings& operator|=(const FVisibleLevelStreamingSettings& B)
	{
		bShouldBeVisible		|= B.bShouldBeVisible;
		bShouldBlockOnLoad		|= B.bShouldBlockOnLoad;
		bShouldChangeVisibility	|= B.bShouldChangeVisibility;
		return *this;
	}

	bool AllSettingsEnabled() const
	{
		return bShouldBeVisible && bShouldBlockOnLoad;
	}

	bool ShouldBeVisible( bool bCurrentShouldBeVisible ) const
	{
		if( bShouldChangeVisibility )
		{
			return bShouldBeVisible;
		}
		else
		{
			return bCurrentShouldBeVisible;
		}
	}

	bool ShouldBlockOnLoad() const
	{
		return bShouldBlockOnLoad;
	}

private:
	/** Whether level should be visible.						*/
	bool bShouldBeVisible;
	/** Whether level should block on load.						*/
	bool bShouldBlockOnLoad;
	/** Whether existing visibility settings should be changed. */
	bool bShouldChangeVisibility;
};

/**
 * Issues level streaming load/unload requests based on whether
 * players are inside/outside level streaming volumes.
 */
void UWorld::ProcessLevelStreamingVolumes(FVector* OverrideViewLocation)
{
	if (GetWorldSettings()->bUseClientSideLevelStreamingVolumes != (GetNetMode() == NM_Client))
	{
		return;
	}

	// if we are delaying using streaming volumes, return now
	if( StreamingVolumeUpdateDelay > 0 )
	{
		StreamingVolumeUpdateDelay--;
		return;
	}
	// Option to skip indefinitely.
	else if( StreamingVolumeUpdateDelay == INDEX_NONE )
	{
		return;
	}

	SCOPE_CYCLE_COUNTER( STAT_VolumeStreamingTickTime );

	// Begin by assembling a list of kismet streaming objects that have non-EditorPreVisOnly volumes associated with them.
	// @todo DB: Cache this, e.g. level startup.
	TArray<ULevelStreaming*> LevelStreamingObjectsWithVolumes;
	TMap<ULevelStreaming*,bool> LevelStreamingObjectsWithVolumesOtherThanBlockingLoad;
	for( int32 LevelIndex = 0 ; LevelIndex < StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* LevelStreamingObject = StreamingLevels[LevelIndex];
		if( LevelStreamingObject )
		{
			for ( int32 i = 0 ; i < LevelStreamingObject->EditorStreamingVolumes.Num() ; ++i )
			{
				ALevelStreamingVolume* StreamingVolume = LevelStreamingObject->EditorStreamingVolumes[i];
				if( StreamingVolume 
				&& !StreamingVolume->bEditorPreVisOnly 
				&& !StreamingVolume->bDisabled )
				{
					LevelStreamingObjectsWithVolumes.Add( LevelStreamingObject );
					if( StreamingVolume->StreamingUsage != SVB_BlockingOnLoad )
					{
						LevelStreamingObjectsWithVolumesOtherThanBlockingLoad.Add( LevelStreamingObject, true );
					}
					break;
				}
			}
		}
	}

	// The set of levels with volumes whose volumes current contain player viewpoints.
	TMap<ULevelStreaming*,FVisibleLevelStreamingSettings> VisibleLevelStreamingObjects;

	// Iterate over all players and build a list of level streaming objects with
	// volumes that contain player viewpoints.
	bool bStreamingVolumesAreRelevant = false;
	for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerActor = Iterator->Get();
		if (PlayerActor->bIsUsingStreamingVolumes)
		{
			bStreamingVolumesAreRelevant = true;

			FVector ViewLocation(0,0,0);
			// let the caller override the location to check for volumes
			if (OverrideViewLocation)
			{
				ViewLocation = *OverrideViewLocation;
			}
			else
			{
				FRotator ViewRotation(0,0,0);
				PlayerActor->GetPlayerViewPoint( ViewLocation, ViewRotation );
			}

			TMap<AVolume*,bool> VolumeMap;

			// Iterate over streaming levels with volumes and compute whether the
			// player's ViewLocation is in any of their volumes.
			for( int32 LevelIndex = 0 ; LevelIndex < LevelStreamingObjectsWithVolumes.Num() ; ++LevelIndex )
			{
				ULevelStreaming* LevelStreamingObject = LevelStreamingObjectsWithVolumes[ LevelIndex ];

				// StreamingSettings is an OR of all level streaming settings of volumes containing player viewpoints.
				FVisibleLevelStreamingSettings StreamingSettings;

				// See if level streaming settings were computed for other players.
				FVisibleLevelStreamingSettings* ExistingStreamingSettings = VisibleLevelStreamingObjects.Find( LevelStreamingObject );
				if ( ExistingStreamingSettings )
				{
					// Stop looking for viewpoint-containing volumes once all streaming settings have been enabled for the level.
					if ( ExistingStreamingSettings->AllSettingsEnabled() )
					{
						continue;
					}

					// Initialize the level's streaming settings with settings that were computed for other players.
					StreamingSettings = *ExistingStreamingSettings;
				}

				// For each streaming volume associated with this level . . .
				for ( int32 i = 0 ; i < LevelStreamingObject->EditorStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* StreamingVolume = LevelStreamingObject->EditorStreamingVolumes[i];
					if ( StreamingVolume && !StreamingVolume->bEditorPreVisOnly && !StreamingVolume->bDisabled )
					{
						bool bViewpointInVolume;
						bool* bResult = VolumeMap.Find(StreamingVolume);
						if ( bResult )
						{
							// This volume has already been considered for another level.
							bViewpointInVolume = *bResult;
						}
						else
						{						
							// Compute whether the viewpoint is inside the volume and cache the result.
							bViewpointInVolume = StreamingVolume->EncompassesPoint( ViewLocation );								
						
							VolumeMap.Add( StreamingVolume, bViewpointInVolume );
							INC_DWORD_STAT( STAT_VolumeStreamingChecks );
						}

						if ( bViewpointInVolume )
						{
							// Copy off the streaming settings for this volume.
							StreamingSettings |= FVisibleLevelStreamingSettings( (EStreamingVolumeUsage) StreamingVolume->StreamingUsage );

							// Update the streaming settings for the level.
							// This also marks the level as "should be loaded".
							VisibleLevelStreamingObjects.Add( LevelStreamingObject, StreamingSettings );

							// Stop looking for viewpoint-containing volumes once all streaming settings have been enabled.
							if ( StreamingSettings.AllSettingsEnabled() )
							{
								break;
							}
						}
					}
				}
			} // for each streaming level 
		} // bIsUsingStreamingVolumes
	} // for each PlayerController

	// do nothing if no players are using streaming volumes
	if (bStreamingVolumesAreRelevant)
	{
		// Iterate over all streaming levels and set the level's loading status based
		// on whether it was found to be visible by a level streaming volume.
		for( int32 LevelIndex = 0 ; LevelIndex < LevelStreamingObjectsWithVolumes.Num() ; ++LevelIndex )
		{
			ULevelStreaming* LevelStreamingObject = LevelStreamingObjectsWithVolumes[LevelIndex];

			// Figure out whether level should be loaded and keep track of original state for notifications on change.
			FVisibleLevelStreamingSettings* NewStreamingSettings= VisibleLevelStreamingObjects.Find( LevelStreamingObject );
			bool bShouldAffectLoading							= LevelStreamingObjectsWithVolumesOtherThanBlockingLoad.Find( LevelStreamingObject ) != NULL;
			bool bShouldBeLoaded								= (NewStreamingSettings != NULL);
			bool bOriginalShouldBeLoaded						= LevelStreamingObject->bShouldBeLoaded;
			bool bOriginalShouldBeVisible						= LevelStreamingObject->bShouldBeVisible;
			bool bOriginalShouldBlockOnLoad						= LevelStreamingObject->bShouldBlockOnLoad;
			int32 bOriginalLODIndex								= LevelStreamingObject->LevelLODIndex;

			if( bShouldBeLoaded || bShouldAffectLoading )
			{
				if( bShouldBeLoaded )
				{
					// Loading.
					LevelStreamingObject->bShouldBeLoaded		= true;
					LevelStreamingObject->bShouldBeVisible		= NewStreamingSettings->ShouldBeVisible( bOriginalShouldBeVisible );
					LevelStreamingObject->bShouldBlockOnLoad	= NewStreamingSettings->ShouldBlockOnLoad();
				}
				// Prevent unload request flood.  The additional check ensures that unload requests can still be issued in the first UnloadCooldownTime seconds of play.
				else 
				if( TimeSeconds - LevelStreamingObject->LastVolumeUnloadRequestTime > LevelStreamingObject->MinTimeBetweenVolumeUnloadRequests 
				||  LevelStreamingObject->LastVolumeUnloadRequestTime < 0.1f )
				{
					//UE_LOG(LogLevel, Warning, TEXT("Unloading") );
					if( GetPlayerControllerIterator() )
					{
						LevelStreamingObject->LastVolumeUnloadRequestTime	= TimeSeconds;
						LevelStreamingObject->bShouldBeLoaded				= false;
						LevelStreamingObject->bShouldBeVisible				= false;						
					}
				}
			
				// Notify players of the change.
				if( bOriginalShouldBeLoaded		!= LevelStreamingObject->bShouldBeLoaded
				||	bOriginalShouldBeVisible	!= LevelStreamingObject->bShouldBeVisible 
				||	bOriginalShouldBlockOnLoad	!= LevelStreamingObject->bShouldBlockOnLoad
				||  bOriginalLODIndex			!= LevelStreamingObject->LevelLODIndex)
				{
					for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
					{
						APlayerController* PlayerController = Iterator->Get();
						PlayerController->LevelStreamingStatusChanged( 
								LevelStreamingObject, 
								LevelStreamingObject->bShouldBeLoaded, 
								LevelStreamingObject->bShouldBeVisible,
								LevelStreamingObject->bShouldBlockOnLoad,
								LevelStreamingObject->LevelLODIndex);
					}
				}
			}
		}
	}
}

/**
	* Run a tick group, ticking all actors and components
	* @param Group - Ticking group to run
	* @param bBlockTillComplete - if true, do not return until all ticks are complete
	*/
void UWorld::RunTickGroup(ETickingGroup Group, bool bBlockTillComplete = true)
{
	check(TickGroup == Group); // this should already be at the correct value, but we want to make sure things are happening in the right order
	FTickTaskManagerInterface::Get().RunTickGroup(Group, bBlockTillComplete);
	TickGroup = ETickingGroup(TickGroup + 1); // new actors go into the next tick group because this one is already gone
}

static TAutoConsoleVariable<int32> CVarAllowAsyncRenderThreadUpdates(
	TEXT("AllowAsyncRenderThreadUpdates"),
	1,
	TEXT("Used to control async renderthread updates. Also gated on FApp::ShouldUseThreadingForPerformance()."));

static TAutoConsoleVariable<int32> CVarAllowAsyncRenderThreadUpdatesDuringGamethreadUpdates(
	TEXT("AllowAsyncRenderThreadUpdatesDuringGamethreadUpdates"),
	1,
	TEXT("If > 0 then we do the gamethread updates _while_ doing parallel updates."));

static TAutoConsoleVariable<int32> CVarAllowAsyncRenderThreadUpdatesEditor(
	TEXT("AllowAsyncRenderThreadUpdatesEditor"),
	0,
	TEXT("Used to control async renderthread updates in the editor."));

namespace EComponentMarkedForEndOfFrameUpdateState
{
	enum Type
	{
		Unmarked,
		Marked,
		MarkedForGameThread,
	};
}

// Utility struct to allow world direct access to UActorComponent::MarkedForEndOfFrameUpdateState without friending all of UActorComponent
struct FMarkComponentEndOfFrameUpdateState
{
	friend class UWorld;

private:
	FORCEINLINE static void Set(UActorComponent* Component, const EComponentMarkedForEndOfFrameUpdateState::Type UpdateState)
	{
		checkSlow(UpdateState < 4); // Only 2 bits are allocated to store this value
		Component->MarkedForEndOfFrameUpdateState = UpdateState;
	}
};

void UWorld::UpdateActorComponentEndOfFrameUpdateState(UActorComponent* Component) const
{
	TWeakObjectPtr<UActorComponent> WeakComponent(Component);
	if (ComponentsThatNeedEndOfFrameUpdate.Contains(WeakComponent))
	{
		FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::Marked);
	}
	else if (ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Contains(WeakComponent))
	{
		FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::MarkedForGameThread);
	}
	else
	{
		FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
	}
}

void UWorld::ClearActorComponentEndOfFrameUpdate(UActorComponent* Component)
{
	check(!bPostTickComponentUpdate); // can't call this while we are doing the updates

	const uint32 CurrentState = Component->GetMarkedForEndOfFrameUpdateState();

	if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::Marked)
	{
		TWeakObjectPtr<UActorComponent> WeakComponent(Component);
		verify(ComponentsThatNeedEndOfFrameUpdate.Remove(WeakComponent) == 1);
	}
	else if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::MarkedForGameThread)
	{
		TWeakObjectPtr<UActorComponent> WeakComponent(Component);
		verify(ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Remove(WeakComponent) == 1);
	}
	FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
}

void UWorld::MarkActorComponentForNeededEndOfFrameUpdate(UActorComponent* Component, bool bForceGameThread)
{
	check(!bPostTickComponentUpdate); // can't call this while we are doing the updates

	uint32 CurrentState = Component->GetMarkedForEndOfFrameUpdateState();
	TWeakObjectPtr<UActorComponent> WeakComponent(Component);

	// force game thread can be turned on later, but we are not concerned about that, those are only cvars and constants; if those are changed during a frame, they won't fully kick in till next frame.
	if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::Marked && bForceGameThread)
	{
		verify(ComponentsThatNeedEndOfFrameUpdate.Remove(WeakComponent) == 1);
		CurrentState = EComponentMarkedForEndOfFrameUpdateState::Unmarked;
	}
	// it is totally ok if it is currently marked for the gamethread but now they are not forcing game thread. It will run on the game thread this frame.

	if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::Unmarked)
	{
		if (!bForceGameThread)
		{
			bool bAllowConcurrentUpdates = FApp::ShouldUseThreadingForPerformance() && 
				(GIsEditor ? !!CVarAllowAsyncRenderThreadUpdatesEditor.GetValueOnGameThread() : !!CVarAllowAsyncRenderThreadUpdates.GetValueOnGameThread());
			bForceGameThread = !bAllowConcurrentUpdates;
		}

		if (bForceGameThread)
		{
			ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Add(WeakComponent);
			FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::MarkedForGameThread);
		}
		else
		{
			ComponentsThatNeedEndOfFrameUpdate.Add(WeakComponent);
			FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::Marked);
		}
	}
}

bool UWorld::HasEndOfFrameUpdates()
{
	return ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Num() > 0 || ComponentsThatNeedEndOfFrameUpdate.Num() > 0;
}

TDrawEvent<FRHICommandList>* BeginSendEndOfFrameUpdatesDrawEvent()
{
	TDrawEvent<FRHICommandList>* DrawEvent = NULL;

#if WANTS_DRAW_MESH_EVENTS
	DrawEvent = new TDrawEvent<FRHICommandList>();

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		BeginDrawEventCommand,
		TDrawEvent<FRHICommandList>*,DrawEvent,DrawEvent,
	{
		BEGIN_DRAW_EVENTF(
			RHICmdList, 
			SendAllEndOfFrameUpdates, 
			(*DrawEvent),
			TEXT("SendAllEndOfFrameUpdates"));
	});

#endif

	return DrawEvent;
}

void EndSendEndOfFrameUpdatesDrawEvent(TDrawEvent<FRHICommandList>* DrawEvent)
{
#if WANTS_DRAW_MESH_EVENTS
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		EndDrawEventCommand,
		TDrawEvent<FRHICommandList>*,DrawEvent,DrawEvent,
	{
		STOP_DRAW_EVENT((*DrawEvent));
		delete DrawEvent;
	});
#endif
}

/**
	* Send all render updates to the rendering thread.
	*/
void UWorld::SendAllEndOfFrameUpdates()
{
	SCOPE_CYCLE_COUNTER(STAT_PostTickComponentUpdate);
	if (!HasEndOfFrameUpdates())
	{
		return;
	}

	// Issue a GPU event to wrap GPU work done during SendAllEndOfFrameUpdates, like skin cache updates
	TDrawEvent<FRHICommandList>* DrawEvent = BeginSendEndOfFrameUpdatesDrawEvent();

	// update all dirty components. 
	TGuardValue<bool> GuardIsFlushedGlobal( bPostTickComponentUpdate, true ); 

	static TArray<TWeakObjectPtr<UActorComponent>> LocalComponentsThatNeedEndOfFrameUpdate; 
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostTickComponentUpdate_Gather);
		check(IsInGameThread() && !LocalComponentsThatNeedEndOfFrameUpdate.Num());
		LocalComponentsThatNeedEndOfFrameUpdate.Reserve(ComponentsThatNeedEndOfFrameUpdate.Num());
		for (TWeakObjectPtr<UActorComponent> Elem : ComponentsThatNeedEndOfFrameUpdate)
		{
			LocalComponentsThatNeedEndOfFrameUpdate.Add(Elem);
		}
	}

	auto ParallelWork = 
		[](int32 Index) 
		{
			UActorComponent* NextComponent = LocalComponentsThatNeedEndOfFrameUpdate[Index].Get(/*bEvenIfPendingKill*/true);
			if (NextComponent)
			{
				if (NextComponent->IsRegistered() && !NextComponent->IsTemplate() && !NextComponent->IsPendingKill())
				{
					FScopeCycleCounterUObject ComponentScope(NextComponent);
					FScopeCycleCounterUObject AdditionalScope(STATS ? NextComponent->AdditionalStatObject() : NULL);
					NextComponent->DoDeferredRenderUpdates_Concurrent();
				}
				check(NextComponent->GetMarkedForEndOfFrameUpdateState() == EComponentMarkedForEndOfFrameUpdateState::Marked);
				FMarkComponentEndOfFrameUpdateState::Set(NextComponent, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
			}
		};
	auto GTWork = 
		[this]()
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_PostTickComponentUpdate_ForcedGameThread);
			for (TWeakObjectPtr<UActorComponent> Elem : ComponentsThatNeedEndOfFrameUpdate_OnGameThread)
			{
				UActorComponent* Component = Elem.Get(/*bEvenIfPendingKill*/true);
				if (Component)
				{
					if (Component->IsRegistered() && !Component->IsTemplate() && !Component->IsPendingKill())
					{
						FScopeCycleCounterUObject ComponentScope(Component);
						FScopeCycleCounterUObject AdditionalScope(STATS ? Component->AdditionalStatObject() : NULL);
						Component->DoDeferredRenderUpdates_Concurrent();
					}
					check(Component->GetMarkedForEndOfFrameUpdateState() == EComponentMarkedForEndOfFrameUpdateState::MarkedForGameThread);
					FMarkComponentEndOfFrameUpdateState::Set(Component, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
				}
			}
			ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Reset();
			ComponentsThatNeedEndOfFrameUpdate.Reset();
	};

	if (CVarAllowAsyncRenderThreadUpdatesDuringGamethreadUpdates.GetValueOnGameThread() > 0)
	{
		ParallelForWithPreWork(LocalComponentsThatNeedEndOfFrameUpdate.Num(), ParallelWork, GTWork);
	}
	else
	{
		GTWork();
		ParallelFor(LocalComponentsThatNeedEndOfFrameUpdate.Num(), ParallelWork);
	}
	LocalComponentsThatNeedEndOfFrameUpdate.Reset();

	EndSendEndOfFrameUpdatesDrawEvent(DrawEvent);
}


#if !UE_BUILD_SHIPPING
static class FFileProfileWrapperExec: private FSelfRegisteringExec
{
	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if( FParse::Command( &Cmd, TEXT("Profile") ) )
		{
			if( FParse::Command( &Cmd, TEXT("File") ) ) // if they didn't use the list command, we will show usage
			{
				FProfiledPlatformFile* ProfilePlatformFile = (FProfiledPlatformFile*)FPlatformFileManager::Get().FindPlatformFile(TProfiledPlatformFile<FProfiledFileStatsFileDetailed>::GetTypeName());
				if (ProfilePlatformFile == NULL)
				{
					// Try 'simple' profiler file.
					ProfilePlatformFile = (FProfiledPlatformFile*)FPlatformFileManager::Get().FindPlatformFile(TProfiledPlatformFile<FProfiledFileStatsFileSimple>::GetTypeName());
				}				

				if( ProfilePlatformFile != NULL)
				{
					DisplayProfileData( ProfilePlatformFile->GetStats() );
				}
				return true;
			}
		}
		return false;
	}

	void DisplayProfileData( const TMap< FString, TSharedPtr< struct FProfiledFileStatsFileBase > >& InProfileData )
	{
		TArray< TSharedPtr<FProfiledFileStatsFileBase> > ProfileData;
		InProfileData.GenerateValueArray( ProfileData );

		// Single root data is required for bar visualizer to work properly
		TSharedPtr< FVisualizerEvent > RootEvent( new FVisualizerEvent( 0.0, 1.0, 0.0, 0, TEXT("I/O") ) );

		// Calculate Start time first
		double StartTimeMs = FPlatformTime::Seconds() * 1000.0; // All timings happened before now
		double EndTimeMs = 0.0;

		for( int32 Index = 0; Index < ProfileData.Num(); Index++ )
		{
			TSharedPtr<FProfiledFileStatsFileBase> FileStat = ProfileData[ Index ];
			double FileDurationMs = 0.0;
			for( int32 ChildIndex = 0; ChildIndex < FileStat->Children.Num(); ChildIndex++ )
			{
				TSharedPtr<FProfiledFileStatsOp> FileOpStat = FileStat->Children[ ChildIndex ];
				if( FileOpStat->Duration > 0.0 )
				{
					StartTimeMs = FMath::Min( FileOpStat->StartTime, StartTimeMs );
					EndTimeMs = FMath::Max( FileOpStat->StartTime + FileOpStat->Duration, EndTimeMs );
					FileDurationMs += FileOpStat->Duration;
				}
			}

			// Create an event for each of the files
			TSharedPtr< FVisualizerEvent > FileEvent( new FVisualizerEvent( 0.0, 1.0, FileDurationMs, Index, FileStat->Name ) );
			FileEvent->ParentEvent = RootEvent;
			RootEvent->Children.Add( FileEvent );
		}

		const double TotalTimeMs = EndTimeMs - StartTimeMs;
		RootEvent->DurationMs = TotalTimeMs;

		for( int32 FileIndex = 0; FileIndex < ProfileData.Num(); FileIndex++ )
		{
			TSharedPtr<FProfiledFileStatsFileBase> FileStat = ProfileData[ FileIndex ];
			TSharedPtr<FVisualizerEvent> FileEvent( RootEvent->Children[ FileIndex ] );

			for( int32 ChildIndex = 0; ChildIndex < FileStat->Children.Num(); ChildIndex++ )
			{
				TSharedPtr<FProfiledFileStatsOp> FileOpStat = FileStat->Children[ ChildIndex ];
				if( FileOpStat->Duration > 0.0 )
				{
					FString EventName;
					switch( FileOpStat->Type )
					{
						case FProfiledFileStatsOp::EOpType::Tell:
							EventName = TEXT("Tell"); break;
						case FProfiledFileStatsOp::EOpType::Seek:
							EventName = TEXT("Seek"); break;
						case FProfiledFileStatsOp::EOpType::Read:
							EventName = FString::Printf( TEXT("Read (%lld)"), FileOpStat->Bytes ); break;
						case FProfiledFileStatsOp::EOpType::Write:
							EventName = FString::Printf( TEXT("Write (%lld)"), FileOpStat->Bytes ); break;
						case FProfiledFileStatsOp::EOpType::Size:
							EventName = TEXT("Size"); break;
						case FProfiledFileStatsOp::EOpType::OpenRead:
							EventName = TEXT("OpenRead"); break;
						case FProfiledFileStatsOp::EOpType::OpenWrite:
							EventName = TEXT("OpenWrite"); break;
						case FProfiledFileStatsOp::EOpType::Exists:
							EventName = TEXT("Exists"); break;
						case FProfiledFileStatsOp::EOpType::Delete:
							EventName = TEXT("Delete"); break;
						case FProfiledFileStatsOp::EOpType::Move:
							EventName = TEXT("Move"); break;
						case FProfiledFileStatsOp::EOpType::IsReadOnly:
							EventName = TEXT("IsReadOnly"); break;
						case FProfiledFileStatsOp::EOpType::SetReadOnly:
							EventName = TEXT("SetReadOnly"); break;
						case FProfiledFileStatsOp::EOpType::GetTimeStamp:
							EventName = TEXT("GetTimeStamp"); break;
						case FProfiledFileStatsOp::EOpType::SetTimeStamp:
							EventName = TEXT("SetTimeStamp"); break;
						case FProfiledFileStatsOp::EOpType::Create:
							EventName = TEXT("Create"); break;
						case FProfiledFileStatsOp::EOpType::Copy:
							EventName = TEXT("Copy"); break;
						case FProfiledFileStatsOp::EOpType::Iterate:
							EventName = TEXT("Iterate"); break;
						default:
							EventName = TEXT("Unknown"); break;
					}

					const double StartTime = ( FileOpStat->StartTime - StartTimeMs ) / TotalTimeMs;
					const double DurationTime = FileOpStat->Duration / TotalTimeMs;
					TSharedPtr<FVisualizerEvent> ChildEvent( new FVisualizerEvent( StartTime, DurationTime, FileOpStat->Duration, FileIndex, EventName ) );
					ChildEvent->ParentEvent = FileEvent;
					FileEvent->Children.Add( ChildEvent );
				}
			}	
		}

		static FName TaskGraphModule(TEXT("TaskGraph"));
		if (FModuleManager::Get().IsModuleLoaded(TaskGraphModule))
		{
			IProfileVisualizerModule& ProfileVisualizer = FModuleManager::GetModuleChecked<IProfileVisualizerModule>(TaskGraphModule);
			ProfileVisualizer.DisplayProfileVisualizer( RootEvent, TEXT("I/O") );
		}
	}

public:

	FFileProfileWrapperExec()
	{}

} FileProfileWrapperExec;
#endif // !UE_BUILD_SHIPPING

#if ENABLE_COLLISION_ANALYZER
#include "ICollisionAnalyzer.h"
#include "CollisionAnalyzerModule.h"
extern bool GCollisionAnalyzerIsRecording;
#endif // ENABLE_COLLISION_ANALYZER

DECLARE_CYCLE_STAT(TEXT("TG_PrePhysics"), STAT_TG_PrePhysics, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("TG_StartPhysics"), STAT_TG_StartPhysics, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("Start TG_DuringPhysics"), STAT_TG_DuringPhysics, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("TG_EndPhysics"), STAT_TG_EndPhysics, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("TG_PostPhysics"), STAT_TG_PostPhysics, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("TG_PostUpdateWork"), STAT_TG_PostUpdateWork, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("TG_LastDemotable"), STAT_TG_LastDemotable, STATGROUP_TickGroups);

#include "GameFramework/SpawnActorTimer.h"

TDrawEvent<FRHICommandList>* BeginTickDrawEvent()
{
	TDrawEvent<FRHICommandList>* TickDrawEvent = new TDrawEvent<FRHICommandList>();

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		BeginDrawEventCommand,
		TDrawEvent<FRHICommandList>*,TickDrawEvent,TickDrawEvent,
	{
		BEGIN_DRAW_EVENTF(
			RHICmdList, 
			WorldTick, 
			(*TickDrawEvent),
			TEXT("WorldTick"));
	});

	return TickDrawEvent;
}

void EndTickDrawEvent(TDrawEvent<FRHICommandList>* TickDrawEvent)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		EndDrawEventCommand,
		TDrawEvent<FRHICommandList>*,TickDrawEvent,TickDrawEvent,
	{
		STOP_DRAW_EVENT((*TickDrawEvent));
		delete TickDrawEvent;
	});
}


void FTickableGameObject::TickObjects(UWorld* World, const int32 InTickType, const bool bIsPaused, const float DeltaSeconds)
{
	if (TickableObjects.Num() > 0)
	{
		check(!bIsTickingObjects);
		bIsTickingObjects = true;

		bool bNeedsCleanup = false;
		const ELevelTick TickType = (ELevelTick)InTickType;

		for (int32 i = 0; i < TickableObjects.Num(); ++i)
		{
			if (FTickableGameObject* TickableObject = TickableObjects[i])
			{
				// If it is tickable and in this world
				if (TickableObject->IsTickable() && (TickableObject->GetTickableGameObjectWorld() == World))
				{
					const bool bIsGameWorld = InTickType == LEVELTICK_All || (World && World->IsGameWorld());
					// If we are in editor and it is editor tickable, always tick
					// If this is a game world then tick if we are not doing a time only (paused) update and we are not paused or the object is tickable when paused
					if ((GIsEditor && TickableObject->IsTickableInEditor()) ||
						(bIsGameWorld && ((!bIsPaused && TickType != LEVELTICK_TimeOnly) || (bIsPaused && TickableObject->IsTickableWhenPaused()))))
					{
						STAT(FScopeCycleCounter Context(TickableObject->GetStatId());)
						TickableObject->Tick(DeltaSeconds);

						// In case it was removed during tick
						if (TickableObjects[i] == nullptr)
						{
							bNeedsCleanup = true;
						}
					}
				}
			}
			else
			{
				bNeedsCleanup = true;
			}
		}

		if (bNeedsCleanup)
		{
			TickableObjects.RemoveAll([](FTickableGameObject* Object) { return Object == nullptr; });
		}

		bIsTickingObjects = false;
	}
}

/**
 * Update the level after a variable amount of time, DeltaSeconds, has passed.
 * All child actors are ticked after their owners have been ticked.
 */
void UWorld::Tick( ELevelTick TickType, float DeltaSeconds )
{
	SCOPE_TIME_GUARD(TEXT("UWorld::Tick"));

	SCOPED_NAMED_EVENT(UWorld_Tick, FColor::Orange);
	if (GIntraFrameDebuggingGameThread)
	{
		return;
	}

	TDrawEvent<FRHICommandList>* TickDrawEvent = BeginTickDrawEvent();

	FWorldDelegates::OnWorldTickStart.Broadcast(TickType, DeltaSeconds);

	//Tick game and other thread trackers.
	for (int32 Tracker = 0; Tracker < (int32)EInGamePerfTrackers::Num; ++Tracker)
	{
		PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::GameThread).Tick();
		PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::OtherThread).Tick();
	}

#if LOG_DETAILED_PATHFINDING_STATS
	GDetailedPathFindingStats.Reset();
#endif

	SCOPE_CYCLE_COUNTER(STAT_WorldTickTime);

	// @todo vreditor: In the VREditor, this isn't actually wrapping the whole frame.  That would have to happen in EditorEngine.cpp's Tick.  However, it didn't seem to affect anything when I tried that.
	if (GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->OnStartGameFrame( GEngine->GetWorldContextFromWorldChecked( this ) );
	}

#if ENABLE_SPAWNACTORTIMER
	FSpawnActorTimer& SpawnTimer = FSpawnActorTimer::Get();
	SpawnTimer.IncrementFrameCount();
#endif

#if ENABLE_COLLISION_ANALYZER
	// Tick collision analyzer (only if level is really ticking)
	if(TickType == LEVELTICK_All || TickType == LEVELTICK_ViewportsOnly)
	{
		ICollisionAnalyzer* Analyzer = FCollisionAnalyzerModule::Get();
		Analyzer->TickAnalyzer(this);
		GCollisionAnalyzerIsRecording = Analyzer->IsRecording();
	}
#endif // ENABLE_COLLISION_ANALYZER

	AWorldSettings* Info = GetWorldSettings();
	FMemMark Mark(FMemStack::Get());
	GInitRunaway();
	bInTick=true;
	bool bIsPaused = IsPaused();

	{
		SCOPE_CYCLE_COUNTER(STAT_NetWorldTickTime);
		SCOPE_TIME_GUARD(TEXT("UWorld::Tick - NetTick"));
		LLM_SCOPE(ELLMTag::Networking);
		// Update the net code and fetch all incoming packets.
		BroadcastTickDispatch(DeltaSeconds);

		if( NetDriver && NetDriver->ServerConnection )
		{
			TickNetClient( DeltaSeconds );
		}
	}

	// Update time.
	RealTimeSeconds += DeltaSeconds;

	// Audio always plays at real-time regardless of time dilation, but only when NOT paused
	if( !bIsPaused )
	{
		AudioTimeSeconds += DeltaSeconds;
	}

	// Save off actual delta
	float RealDeltaSeconds = DeltaSeconds;

	// apply time multipliers
	DeltaSeconds *= Info->GetEffectiveTimeDilation();

	// Handle clamping of time to an acceptable value
	const float GameDeltaSeconds = Info->FixupDeltaSeconds(DeltaSeconds, RealDeltaSeconds);
	check(GameDeltaSeconds >= 0.0f);

	DeltaSeconds = GameDeltaSeconds;
	DeltaTimeSeconds = DeltaSeconds;

	UnpausedTimeSeconds += DeltaSeconds;

	if ( !bIsPaused )
	{
		TimeSeconds += DeltaSeconds;
	}

	if( bPlayersOnly )
	{
		TickType = LEVELTICK_ViewportsOnly;
	}

	// give the async loading code more time if we're performing a high priority load or are in seamless travel
	if (Info->bHighPriorityLoading || Info->bHighPriorityLoadingLocal || IsInSeamlessTravel())
	{
		// Force it to use the entire time slice, even if blocked on I/O
		ProcessAsyncLoading(true, true, GPriorityAsyncLoadingExtraTime / 1000.0f);
	}

	// Translate world origin if requested
	if (OriginLocation != RequestedOriginLocation)
	{
		SetNewWorldOrigin(RequestedOriginLocation);
	}
	else
	{
		OriginOffsetThisFrame = FVector::ZeroVector;
	}
	
	// update world's subsystems (NavigationSystem for now)
	if ( !bIsPaused )
	{
		if (NavigationSystem != NULL)
		{
			SCOPE_CYCLE_COUNTER(STAT_NavWorldTickTime);
			NavigationSystem->Tick(DeltaSeconds);
		}
	}

	bool bDoingActorTicks = 
		(TickType!=LEVELTICK_TimeOnly)
		&&	!bIsPaused
		&&	(!NetDriver || !NetDriver->ServerConnection || NetDriver->ServerConnection->State==USOCK_Open);

	FLatentActionManager& CurrentLatentActionManager = GetLatentActionManager();

	// Reset the list of objects the LatentActionManager has processed this frame
	CurrentLatentActionManager.BeginFrame();
	
	if (bDoingActorTicks)
	{
		// Reset Async Trace before Tick starts 
		SCOPE_CYCLE_COUNTER(STAT_ResetAsyncTraceTickTime);
		ResetAsyncTrace();
	}

	for (int32 i = 0; i < LevelCollections.Num(); ++i)
	{
		// Build a list of levels from the collection that are also in the world's Levels array.
		// Collections may contain levels that aren't loaded in the world at the moment.
		TArray<ULevel*> LevelsToTick;
		for (ULevel* CollectionLevel : LevelCollections[i].GetLevels())
		{
			if (Levels.Contains(CollectionLevel))
			{
				LevelsToTick.Add(CollectionLevel);
			}
		}

		// Set up context on the world for this level collection
		FScopedLevelCollectionContextSwitch LevelContext(i, this);

		// If caller wants time update only, or we are paused, skip the rest.
		if (bDoingActorTicks)
		{
			// Actually tick actors now that context is set up
			SetupPhysicsTickFunctions(DeltaSeconds);
			TickGroup = TG_PrePhysics; // reset this to the start tick group
			FTickTaskManagerInterface::Get().StartFrame(this, DeltaSeconds, TickType, LevelsToTick);

			SCOPE_CYCLE_COUNTER(STAT_TickTime);
			{
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_PrePhysics"), 10);
				SCOPE_CYCLE_COUNTER(STAT_TG_PrePhysics);
				RunTickGroup(TG_PrePhysics);
			}
			bInTick = false;
			EnsureCollisionTreeIsBuilt();
			bInTick = true;
			{
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_StartPhysics"), 10);
				RunTickGroup(TG_StartPhysics); 
			}
			{
				SCOPE_CYCLE_COUNTER(STAT_TG_DuringPhysics);
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_DuringPhysics"), 10);
				RunTickGroup(TG_DuringPhysics, false); // No wait here, we should run until idle though. We don't care if all of the async ticks are done before we start running post-phys stuff
			}

#if WITH_FLEX

			// tick Flex asynchronously over the course of the whole frame (adds 1 frame latency)
			// this is called an 'inverted' tick because it must first of all wait() and then tick()
			const bool bInvertedFlexTick = true;

			// only tick Flex for source levels
			if (LevelCollections[i].GetType() == ELevelCollectionType::DynamicSourceLevels)
			{
				if (bInvertedFlexTick)
				{
					if (PhysicsScene != NULL)
					{
						// wait for Flex GPU update to finish		
						PhysicsScene->WaitFlexScenes();

						// all Flex buffer modifications should occur after this point
						// and before TickFlexScenes() call below
					}

					TickGroup = TG_EndPhysics; // set this here so the current tick group is correct during collision notifies, though I am not sure it matters. 'cause of the false up there^^^
					{
						SCOPE_CYCLE_COUNTER(STAT_TG_EndPhysics);
						RunTickGroup(TG_EndPhysics);
					}

					if (PhysicsScene != NULL)
					{
						// kick of flex work async to rest of frame
						FGraphEventRef dummy;
						PhysicsScene->TickFlexScenes(ENamedThreads::AnyThread, dummy, DeltaSeconds);
					}
				}
				else
				{
					TickGroup = TG_EndPhysics; // set this here so the current tick group is correct during collision notifies, though I am not sure it matters. 'cause of the false up there^^^
					{
						SCOPE_CYCLE_COUNTER(STAT_TG_EndPhysics);
						RunTickGroup(TG_EndPhysics);
					}

					// synchronous Flex update
					if (PhysicsScene != NULL)
					{
						FGraphEventRef dummy;
						PhysicsScene->TickFlexScenes(ENamedThreads::AnyThread, dummy, DeltaSeconds);
						PhysicsScene->WaitFlexScenes();
					}		
				}
			}
			else
			{
				TickGroup = TG_EndPhysics; // set this here so the current tick group is correct during collision notifies, though I am not sure it matters. 'cause of the false up there^^^
				{
					SCOPE_CYCLE_COUNTER(STAT_TG_EndPhysics);
					RunTickGroup(TG_EndPhysics);
				}
			}

#else
			TickGroup = TG_EndPhysics; // set this here so the current tick group is correct during collision notifies, though I am not sure it matters. 'cause of the false up there^^^
			{
				SCOPE_CYCLE_COUNTER(STAT_TG_EndPhysics);
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_EndPhysics"), 10);
				RunTickGroup(TG_EndPhysics);
			}
#endif

			{
				SCOPE_CYCLE_COUNTER(STAT_TG_PostPhysics);
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_PostPhysics"), 10);
				RunTickGroup(TG_PostPhysics);
			}
	
		}
		else if( bIsPaused )
		{
			FTickTaskManagerInterface::Get().RunPauseFrame(this, DeltaSeconds, LEVELTICK_PauseTick, LevelsToTick);
		}
		
		// We only want to run the following once, so only run it for the source level collection.
		if (LevelCollections[i].GetType() == ELevelCollectionType::DynamicSourceLevels)
		{
			// Process any remaining latent actions
			if( !bIsPaused )
			{
				// This will process any latent actions that have not been processed already
				CurrentLatentActionManager.ProcessLatentActions(NULL, DeltaSeconds);
			}
#if 0 // if you need to debug physics drawing in editor, use this. If you type pxvis collision, it will work. 
			else
			{
				// Tick our async work (physics, etc.) and tick with no elapsed time for playersonly
				TickAsyncWork(0.f);
				// Wait for async work to come back
				WaitForAsyncWork();
			}
#endif

			{
				SCOPE_CYCLE_COUNTER(STAT_TickableTickTime);

				if (TickType != LEVELTICK_TimeOnly && !bIsPaused)
				{
					SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TimerManager"), 5);
					STAT(FScopeCycleCounter Context(GetTimerManager().GetStatId());)
					GetTimerManager().Tick(DeltaSeconds);
				}

				{
					SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TickObjects"), 5);
					FTickableGameObject::TickObjects(this, TickType, bIsPaused, DeltaSeconds);
				}
			}

			// Update cameras and streaming volumes
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateCameraTime);
				// Update cameras last. This needs to be done before NetUpdates, and after all actors have been ticked.
				for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
				{
					APlayerController* PlayerController = Iterator->Get();
					if (!bIsPaused || PlayerController->ShouldPerformFullTickWhenPaused())
					{
						PlayerController->UpdateCameraManager(DeltaSeconds);
					}
					else if (PlayerController->PlayerCameraManager && FCameraPhotographyManager::IsSupported(this))
					{
						PlayerController->PlayerCameraManager->UpdateCameraPhotographyOnly();
					}
				}

				if( !bIsPaused )
				{
					// Issues level streaming load/unload requests based on local players being inside/outside level streaming volumes.
					if (IsGameWorld())
					{
						ProcessLevelStreamingVolumes();

						if (WorldComposition)
						{
							WorldComposition->UpdateStreamingState();
						}
					}
				}
			}
		}

		if (bDoingActorTicks)
		{
			SCOPE_CYCLE_COUNTER(STAT_TickTime);
			{
				SCOPE_CYCLE_COUNTER(STAT_TG_PostUpdateWork);
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - PostUpdateWork"), 5);
				RunTickGroup(TG_PostUpdateWork);
			}
			{
				SCOPE_CYCLE_COUNTER(STAT_TG_LastDemotable);
				SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - TG_LastDemotable"), 5);
				RunTickGroup(TG_LastDemotable);
			}

			FTickTaskManagerInterface::Get().EndFrame(); 
		}
	}

	if (bDoingActorTicks)
	{
		SCOPE_CYCLE_COUNTER(STAT_TickTime);

		FWorldDelegates::OnWorldPostActorTick.Broadcast(this, TickType, DeltaSeconds);

		if ( PhysicsScene != NULL )
		{
			GPhysCommandHandler->Flush();
		}
		
		// All tick is done, execute async trace
		{
			SCOPE_CYCLE_COUNTER(STAT_FinishAsyncTraceTickTime);
			SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - FinishAsyncTrace"), 5);
			FinishAsyncTrace();
		}
	}

	{
		STAT(FParticleMemoryStatManager::UpdateStats());
	}

	// Update net and flush networking.
    // Tick all net drivers
	{
		SCOPE_CYCLE_COUNTER(STAT_NetBroadcastTickTime);
		BroadcastTickFlush(RealDeltaSeconds); // note: undilated time is being used here
	}
	
     // PostTick all net drivers
	{
		SCOPE_CYCLE_COUNTER(STAT_NetBroadcastPostTickTime);
		BroadcastPostTickFlush(RealDeltaSeconds); // note: undilated time is being used here
	}

	if( Scene )
	{
		// Update SpeedTree wind objects.
		Scene->UpdateSpeedTreeWind(TimeSeconds);
	}

	// Tick the FX system.
	if (!bIsPaused && FXSystem != NULL)
	{
		SCOPE_TIME_GUARD_MS(TEXT("UWorld::Tick - FX"), 5);
		FXSystem->Tick(DeltaSeconds);
	}
	
	// Finish up.
	if(bDebugFrameStepExecution)
	{
		bDebugPauseExecution = true;
		bDebugFrameStepExecution = false;
	}


	bInTick = false;
	Mark.Pop();

	GEngine->ConditionalCollectGarbage();

	// players only request from last frame
	if (bPlayersOnlyPending)
	{
		bPlayersOnly = bPlayersOnlyPending;
		bPlayersOnlyPending = false;
	}

#if LOG_DETAILED_PATHFINDING_STATS
	GDetailedPathFindingStats.DumpStats();
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	GShouldLogOutAFrameOfMoveComponent = false;
	GShouldLogOutAFrameOfSetBodyTransform = false;

#if LOOKING_FOR_PERF_ISSUES || !WITH_EDITORONLY_DATA
	extern TArray<FString>	ThisFramePawnSpawns;
	if(ThisFramePawnSpawns.Num() > 1 && IsGameWorld() && !GIsServer && GEngine->bCheckForMultiplePawnsSpawnedInAFrame )
	{
		const FString WarningMessage = FString::Printf( TEXT("%d PAWN SPAWNS THIS FRAME! "), ThisFramePawnSpawns.Num() );

		UE_LOG(LogLevel, Warning, TEXT("%s"), *WarningMessage );
		// print out the pawns that were spawned
		for(int32 i=0; i<ThisFramePawnSpawns.Num(); i++)
		{
			UE_LOG(LogLevel, Warning, TEXT("%s"), *ThisFramePawnSpawns[i]);
		}

		if( IsGameWorld() && GAreScreenMessagesEnabled && ThisFramePawnSpawns.Num() > GEngine->NumPawnsAllowedToBeSpawnedInAFrame )
		{
			GEngine->AddOnScreenDebugMessage((uint64)((PTRINT)this), 5.f, FColor::Red, *WarningMessage);

			for(int32 i=0; i<ThisFramePawnSpawns.Num(); i++)
			{
				GEngine->AddOnScreenDebugMessage((uint64)((PTRINT)+i), 5.f, FColor::Red, *ThisFramePawnSpawns[i]);
			}
		}
	}
	ThisFramePawnSpawns.Empty();
#endif
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if WITH_EDITOR
	if(GIsEditor && bDoDelayedUpdateCullDistanceVolumes)
	{
		bDoDelayedUpdateCullDistanceVolumes = false;
		UpdateCullDistanceVolumes();
	}
#endif // WITH_EDITOR

	// Dump the viewpoints with which we were rendered last frame. They will be updated when the world is next rendered.
	ViewLocationsRenderedLastFrame.Reset();

	if (GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->OnEndGameFrame( GEngine->GetWorldContextFromWorldChecked( this ) );
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		TickInGamePerfTrackersRT,
		UWorld*, WorldParam, this,
		{
		//Tick game and other thread trackers.
		for (int32 Tracker = 0; Tracker < (int32)EInGamePerfTrackers::Num; ++Tracker)
		{
			WorldParam->PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::RenderThread).Tick();
		}
	}
	);

	EndTickDrawEvent(TickDrawEvent);
}

/**
 *  Requests a one frame delay of Garbage Collection
 */
void UWorld::DelayGarbageCollection()
{
	GEngine->DelayGarbageCollection();
}

void UWorld::ForceGarbageCollection( bool bFullPurge)
{
	GEngine->ForceGarbageCollection(bFullPurge);
}

void UWorld::SetTimeUntilNextGarbageCollection(const float MinTimeUntilNextPass)
{
	GEngine->SetTimeUntilNextGarbageCollection(MinTimeUntilNextPass);
}

float UWorld::GetTimeBetweenGarbageCollectionPasses() const
{
	return GEngine->GetTimeBetweenGarbageCollectionPasses();
}

/**
 *  Interface to allow WorldSettings to request immediate garbage collection
 */
void UWorld::PerformGarbageCollectionAndCleanupActors()
{
	GEngine->PerformGarbageCollectionAndCleanupActors();
}

void UWorld::CleanupActors()
{
	// Remove NULL entries from actor list. Only does so for dynamic actors to avoid resorting; in theory static 
	// actors shouldn't be deleted during gameplay.
	for (ULevel* Level : Levels)
	{
		// Don't compact actors array for levels that are currently in the process of being made visible as the
		// code that spreads this work across several frames relies on the actor count not changing as it keeps
		// an index into the array.
		if( CurrentLevelPendingVisibility != Level )
		{
			// Actor 0 (world info) and 1 (default brush) are special and should never be removed from the actor array even if NULL
			const int32 FirstDynamicIndex = 2;
			int32 NumActorsToRemove = 0;
			// Remove NULL entries from array, we're iterating backwards to avoid unnecessary memcpys during removal.
			for( int32 ActorIndex=Level->Actors.Num()-1; ActorIndex>=FirstDynamicIndex; ActorIndex-- )
			{
				// To avoid shuffling things down repeatedly when not necessary count nulls and then remove in bunches
				if (Level->Actors[ActorIndex] == nullptr)
				{
					++NumActorsToRemove;
				}
				else if (NumActorsToRemove > 0)
				{
					Level->Actors.RemoveAt(ActorIndex+1, NumActorsToRemove, false);
					NumActorsToRemove = 0;
				}
			}
			if (NumActorsToRemove > 0)
			{
				// If our FirstDynamicIndex (and any immediately following it) were null it won't get caught in the loop, so do a cleanup pass here
				Level->Actors.RemoveAt(FirstDynamicIndex, NumActorsToRemove, false);
			}
		}
	}
}
