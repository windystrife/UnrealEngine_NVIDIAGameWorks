// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealEngine.h: Unreal engine helper definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderCommandFence.h"
#include "GenericPlatform/GenericWindow.h"
#include "Engine/Engine.h"
#include "SceneTypes.h"

class FViewportClient;
class UFont;
class ULocalPlayer;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogEngine, Log, All);
//
//	FLocalPlayerIterator - Iterates over local players in the game.
//	There are no advantages to using this over GEngine->GetLocalPlayerIterator(GetWorld());
//	
//	Example:
//	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
//	{
//			// Do Stuff
//	}	
//

class FLocalPlayerIterator
{
protected:
	TArray<class ULocalPlayer*>::TConstIterator Iter;

	void GetCurrent()
	{		
		// advance iter while not finished iterating, and while current elemnt in NULL
		while( Iter && *Iter == NULL )
			++Iter;
	}

public:
	FLocalPlayerIterator(UEngine* InEngine, class UWorld* InWorld)
		: Iter(InEngine->GetLocalPlayerIterator(InWorld))
	{
		GetCurrent();
	}

	void operator++()
	{
		++Iter;
		GetCurrent();
	}
	ULocalPlayer* operator*()
	{
		return *Iter;
	}
	ULocalPlayer* operator->()
	{
		return *Iter;
	}
	operator bool()
	{
		return (bool)Iter;
	}
};

//	PlayerControllerIterators
//	This is the safest, most efficient way to iterate over player controllers.
//
//	Examples:
//
//	for (TPlayerControllerIterator<AQAPlayerController>::LocalOnly It(GetWorld()); It; ++It)
//	{
//		// PC is a locally controlled AQAPlayerController.
//		AQAPlayerController * PC = *It;	
//
//		// This PC will always be locally controlled!
//		check(PC->IsLocalController();
//	}
//
//
//
//
//	for (TPlayerControllerIterator<AQAPlayerController>::ServerAll It(GetWorld()); It; ++It)
//	{
//		// PC is a AQAPlayerController. It may local or remotely controlled.
//		AQAPlayerController * PC = *It;	
//	
//		// This can only be done on the server! 
//		// Only the server has player controllers for everyone!
//		check(GetWorld()->GetNetMode() != NM_Client);
//	}



template< class T, bool LocalOnly > class TBasePlayerControllerIterator
{
public:
	TBasePlayerControllerIterator(class UWorld* InWorld)
		: Iter(InWorld->GetPlayerControllerIterator())
	{
		check(!LocalOnly || InWorld->GetNetMode() != NM_Client);	// You should only iterate on non local player controllers if you are the server
		AdvanceCurrent();
	}

	void operator++()
	{
		Next();
	}
	T* operator*()
	{
		return Current;
	}
	T* operator->()
	{
		return Current;
	}
	operator bool()
	{
		return (Current!=NULL);
	}

protected:

	FConstPlayerControllerIterator Iter;
	T* Current;

	void AdvanceCurrent()
	{
		// Look at current Iter
		Current = Iter ? Cast<T>(*Iter) : nullptr;

		// Advance if we have to
		while(Iter && (!Current || (LocalOnly && !Current->IsLocalController())))
		{
			++Iter;
			Current = Iter ? Cast<T>(*Iter) : nullptr;
		}
	}

	void Next()
	{
		// Advance one
		++Iter;

		// Update Current
		AdvanceCurrent();
	}
};

template<class T>
struct TPlayerControllerIterator
{
	typedef TBasePlayerControllerIterator<T, true>	LocalOnly;	// Only iterates locally controlled player controllers - can be used on client or server
	typedef TBasePlayerControllerIterator<T, false> ServerAll;	// Iterates all player controllers - local or remote - only can be used on server
};


/*-----------------------------------------------------------------------------
	Tick/ update stats helper for profiling.
-----------------------------------------------------------------------------*/

/**
 * Helper structure encapsulating all information gathered.
 */
struct FTickStats
{
	/** Object associated with instances. We keep the name because the object might be gone.*/
	FString ObjectPathName;
	/** Result of GetDetailedInfo() on the above.  */
	FString ObjectDetailedInfo;
	/** Result of GetDetailedInfo() on the above.  */
	FName ObjectClassFName;
	/** Index of GC run when the validity of the UObject Pointer was last checked.  */
	int32 GCIndex;
	/** Total accumulative time captured. */
	float TotalTime;
	/** Number of captures this frame. */
	int32 Count;
	/** bForSummary is used for the logging code to know if this should be used for a summary or not **/
	bool bForSummary;

	/** Compare helper for Sort */
	FORCEINLINE bool operator()( const FTickStats& A, const FTickStats& B	) const
	{
		return (B.TotalTime < A.TotalTime);
	}
};

/**
 * Helper struct for gathering detailed per object tick stats.
 */
struct FDetailedTickStats
{
	/** Constructor, initializing all members. */
	FDetailedTickStats( int32 InNumClassesToReport, float InTimeBetweenLogDumps, float InMinTimeBetweenLogDumps, float InTimesToReport, const TCHAR* InOperationPerformed );

	/** Destructor */
	virtual ~FDetailedTickStats();

	/**
	 * Starts tracking an object and returns whether it's a recursive call or not. If it is recursive
	 * the function will return false and EndObject should not be called on the object.
	 *
	 * @param	Object		Object to track
	 * @return	false if object is already tracked and EndObject should NOT be called, true otherwise
	 */
	bool BeginObject( UObject* Object );

	/**
	 * Finishes tracking the object and updates the time spent.
	 *
	 * @param	Object		Object to track
	 * @param	DeltaTime	Time we've been tracking it
	 * @param   bForSummary Object should be used for high level summary
	 */
	void EndObject( UObject* Object, float DeltaTime, bool bForSummary );

	/**
	 * Reset stats to clean slate.
	 */
	void Reset();

	/**
	 * Dump gathered stats informatoin to the log.
	 */
	void DumpStats();

	/** 
	 * Delegate handler for pre garbage collect event
	 **/
	void OnPreGarbageCollect()
	{
		GCIndex++;
		check(ObjectsInFlight.Num() == 0); // probably not required, but we shouldn't have anything in flight when we GC
	}

private:
	/** This is the collection of stats; some refer to objects that are long gone. */
	TArray<FTickStats> AllStats;
	/** Mapping from class to an index into the AllStats array. */
	TMap<const UObject*,int32> ObjectToStatsMap;
	/** Set of objects currently being tracked. Needed to correctly handle recursion. */
	TSet<const UObject*> ObjectsInFlight;

	/** Index of GC run. This is used to invalidate UObject pointers to make the whole system GC safe.  */
	int32 GCIndex;
	/** The GC callback cannot usually be registered at construction because this comes from a static data structure  */
	bool GCCallBackRegistered;
	/** Number of objects to report. Top X */
	int32	NumObjectsToReport;
	/** Time between dumping to the log in seconds. */
	float TimeBetweenLogDumps;
	/** Minimum time between log dumps, used for e.g. slow frames dumping. */
	float MinTimeBetweenLogDumps;
	/** Last time stats where dumped to the log. */
	double LastTimeOfLogDump;
	/** Tick time in ms to report if above. */
	float TimesToReport;
	/** Name of operation performed that is being tracked. */
	FString OperationPerformed;
	/** Handle to the registered OnPreGarbageCollect delegate */
	FDelegateHandle OnPreGarbageCollectDelegateHandle;
};

/** Scoped helper structure for capturing tick time. */
struct FScopedDetailTickStats
{
	/**
	 * Constructor, keeping track of object and start time.
	 */
	FScopedDetailTickStats( FDetailedTickStats& InDetailedTickStats, UObject* ObjectToTrack );

	/**
	 * Destructor, calculating delta time and updating global helper.
	 */
	~FScopedDetailTickStats();


private:
	/** Object to track. 
		Not GC safe, but we won't have anything in-flight during GC so that should be moot
	*/
	UObject* Object;
	/** Tick start time. */
	uint32 StartCycles;
	/** Detailed tick stats to update. */
	FDetailedTickStats& DetailedTickStats;
	/** Whether object should be tracked. false e.g. when recursion is involved. */
	bool bShouldTrackObject;
	/** Whether object class should be tracked. false e.g. when recursion is involved. */
	bool bShouldTrackObjectClass;
};


/** Delegate for switching between PIE and Editor worlds without having access to them */
DECLARE_DELEGATE_OneParam( FOnSwitchWorldForPIE, bool );

#if WITH_EDITOR
/**
 * When created, tells the viewport client to set the appropriate GWorld
 * When destroyed, tells the viewport client to reset the GWorld back to what it was
 */
class ENGINE_API FScopedConditionalWorldSwitcher
{
public:
	FScopedConditionalWorldSwitcher( class FViewportClient* InViewportClient );
	~FScopedConditionalWorldSwitcher();

	/** Delegate to call to switch worlds for PIE viewports.  Is not called when simulating (non-gameviewportclient) */
	static FOnSwitchWorldForPIE SwitchWorldForPIEDelegate;
private:
	/** Viewport client used to set the world */
	FViewportClient* ViewportClient;
	/** World to reset when this is destoyed.  Can be null if nothing needs to be reset */	
	UWorld* OldWorld;
};
#else
// does nothing outside of the editor
class ENGINE_API FScopedConditionalWorldSwitcher
{
public:
	FScopedConditionalWorldSwitcher( class FViewportClient* InViewportClient ){}
	~FScopedConditionalWorldSwitcher() {}
};

#endif

/**
 * This function will look at the given command line and see if we have passed in a map to load.
 * If we have then use that.
 * If we have not then use the DefaultMap which is stored in the Engine.ini
 * 
 * @see UGameEngine::Init() for this method of getting the correct start up map
 *
 * @param CommandLine Commandline to use to get startup map (NULL or "" will return default startup map)
 *
 * @return Name of the startup map without an extension (so can be used as a package name)
 */
ENGINE_API FString appGetStartupMap(const TCHAR* CommandLine);

/**
 * Get a list of all packages that may be needed at startup, and could be
 * loaded async in the background when doing seek free loading
 *
 * @param PackageNames The output list of package names
 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
 */
ENGINE_API void appGetAllPotentialStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename, bool bIsCreatingHashes);

// Calculate the average frame time by using the stats system.
inline void CalculateFPSTimings()
{
	extern ENGINE_API float GAverageFPS;
	extern ENGINE_API float GAverageMS;
	// Calculate the average frame time via continued averaging.
	static double LastTime	= 0;
	double CurrentTime		= FPlatformTime::Seconds();
	float FrameTime			= (CurrentTime - LastTime) * 1000;
	// A 3/4, 1/4 split gets close to a simple 10 frame moving average
	GAverageMS				= GAverageMS * 0.75f + FrameTime * 0.25f;
	LastTime				= CurrentTime;
	// Calculate average framerate.
	GAverageFPS = 1000.f / GAverageMS;
}

/** @return The font to use for rendering stats display. */
extern ENGINE_API UFont* GetStatsFont();

/*-----------------------------------------------------------------------------
	Frame end sync object implementation.
-----------------------------------------------------------------------------*/

/**
 * Special helper class for frame end sync. It respects a passed in option to allow one frame
 * of lag between the game and the render thread by using two events in round robin fashion.
 */
class FFrameEndSync
{
	/** Pair of fences. */
	FRenderCommandFence Fence[2];
	/** Current index into events array. */
	int32 EventIndex;
public:
	/**
	 * Syncs the game thread with the render thread. Depending on passed in bool this will be a total
	 * sync or a one frame lag.
	 */
	ENGINE_API void Sync( bool bAllowOneFrameThreadLag );
};


/** Public interface to FEngineLoop so we can call it from editor or editor code */
class IEngineLoop
{
public:

	virtual int32 Init() = 0;

	virtual void Tick() = 0;

	/** Removes references to any objects pending cleanup by deleting them. */
	virtual void ClearPendingCleanupObjects() = 0;
};

/**
 * Cache some of the scalability CVars to avoid some virtual function calls (no longer the case with the new console variable system, but we only have 1 render thread)
 * and to detect changes and act accordingly if needed.
 * read by rendering thread[s], written by main thread, uses FlushRenderingCommands() to avoid conflict
 */
struct FCachedSystemScalabilityCVars
{
	bool bInitialized;
	int32 DetailMode;
	EMaterialQualityLevel::Type MaterialQualityLevel;
	int32 MaxShadowResolution;
	int32 MaxCSMShadowResolution;
	float ViewDistanceScale;
	float ViewDistanceScaleSquared;

	FCachedSystemScalabilityCVars();

protected:
	// This isn't public as it's only used to detect the change. Use ComputeAnisotropyRT()
	int32 MaxAnisotropy;

	friend void ScalabilityCVarsSinkCallback();
};

ENGINE_API bool AllowHighQualityLightmaps(ERHIFeatureLevel::Type FeatureLevel);

ENGINE_API const FCachedSystemScalabilityCVars& GetCachedScalabilityCVars();

struct ENGINE_API FSystemResolution
{
	int32 ResX;
	int32 ResY;
	EWindowMode::Type WindowMode;
	bool bForceRefresh;

	// Helper function for changing system resolution via the r.setres console command
	// This function will set r.setres, which will trigger a resolution change later on
	// when the console variable sinks are called
	static void RequestResolutionChange(int32 InResX, int32 InResY, EWindowMode::Type InWindowMode);

	FSystemResolution()
		: ResX(0)
		, ResY(0)
		, WindowMode(EWindowMode::Windowed)
		, bForceRefresh(false)
	{

	}

	void ForceRefresh()
	{
		RequestResolutionChange(ResX, ResY, WindowMode);
		bForceRefresh = true;
	}
};

ENGINE_API extern FSystemResolution GSystemResolution;
