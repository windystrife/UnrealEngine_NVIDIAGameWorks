// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Engine/EngineBaseTypes.h"

class FTickTaskLevel;
class ULevel;

DECLARE_STATS_GROUP(TEXT("TickGroups"), STATGROUP_TickGroups, STATCAT_Advanced);

/** 
 * Interface for the tick task manager
 **/
class FTickTaskManagerInterface
{
public:
	virtual ~FTickTaskManagerInterface()
	{
	}

	/** Allocate a new ticking structure for a ULevel **/
	virtual FTickTaskLevel* AllocateTickTaskLevel() = 0;

	/** Free a ticking structure for a ULevel **/
	virtual void FreeTickTaskLevel(FTickTaskLevel* TickTaskLevel) = 0;

	/**
	 * Queue all of the ticks for a frame
	 *
	 * @param World	- World currently ticking
	 * @param DeltaSeconds - time in seconds since last tick
	 * @param TickType - type of tick (viewports only, time only, etc)
	 */
	virtual void StartFrame(UWorld* InWorld, float DeltaSeconds, ELevelTick TickType, const TArray<ULevel*>& LevelsToTick) = 0;

	/**
	 * Run all of the ticks for a pause frame synchronously on the game thread.
	 * The capability of pause ticks are very limited. There are no dependencies or ordering or tick groups.
	 * @param World	- World currently ticking
	 * @param DeltaSeconds - time in seconds since last tick
	 * @param TickType - type of tick (viewports only, time only, etc)
	 */
	virtual void RunPauseFrame(UWorld* InWorld, float DeltaSeconds, ELevelTick TickType, const TArray<ULevel*>& LevelsToTick) = 0;

	/**
		* Run a tick group, ticking all actors and components
		* @param Group - Ticking group to run
		* @param bBlockTillComplete - if true, do not return until all ticks are complete
	*/
	virtual void RunTickGroup(ETickingGroup Group, bool bBlockTillComplete ) = 0;

	/** Finish a frame of ticks **/
	virtual void EndFrame() = 0;

	/** Dumps all registered tick functions to output device. */
	virtual void DumpAllTickFunctions(FOutputDevice& Ar, UWorld* InWorld, bool bEnabled, bool bDisabled) = 0;

	/**
	 * Singleton to retrieve the GLOBAL tick task manager
	 *
	 * @return Reference to the global cache tick task manager
	 */
	static ENGINE_API FTickTaskManagerInterface& Get();

};


