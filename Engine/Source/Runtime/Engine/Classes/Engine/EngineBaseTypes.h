// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *	This file is for shared structs and enums that need to be declared before the rest of Engine.
 *  The typical use case is for structs used in the renderer and also in script code.
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/CoreMisc.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineBaseTypes.generated.h"

class UActorComponent;
struct FTickContext;

//
//	EInputEvent
//
UENUM()
enum EInputEvent
{
	IE_Pressed              =0,
	IE_Released             =1,
	IE_Repeat               =2,
	IE_DoubleClick          =3,
	IE_Axis                 =4,
	IE_MAX                  =5,
};

UENUM()
enum class EMouseCaptureMode : uint8
{
	/** Do not capture the mouse at all */
	NoCapture,
	/** Capture the mouse permanently when the viewport is clicked, and consume the initial mouse down that caused the capture so it isn't processed by player input */
	CapturePermanently,
	/** Capture the mouse permanently when the viewport is clicked, and allow player input to process the mouse down that caused the capture */
	CapturePermanently_IncludingInitialMouseDown,
	/** Capture the mouse during a mouse down, releases on mouse up */
	CaptureDuringMouseDown,
	/** Capture only when the right mouse button is down, not any of the other mouse buttons */
	CaptureDuringRightMouseDown,
};

UENUM()
enum class EMouseLockMode : uint8
{
	/** Do not lock the mouse cursor to the viewport */
	DoNotLock,
	/** Only lock the mouse cursor to the viewport when the mouse is captured */
	LockOnCapture,
	/** Always lock the mouse cursor to the viewport */
	LockAlways,
};

/** Type of tick we wish to perform on the level */
enum ELevelTick
{
	/** Update the level time only. */
	LEVELTICK_TimeOnly = 0,
	/** Update time and viewports. */
	LEVELTICK_ViewportsOnly = 1,
	/** Update all. */
	LEVELTICK_All = 2,
	/** Delta time is zero, we are paused. Components don't tick. */
	LEVELTICK_PauseTick = 3,
};

/** Determines which ticking group a tick function belongs to. */
UENUM(BlueprintType)
enum ETickingGroup
{
	/** Any item that needs to be executed before physics simulation starts. */
	TG_PrePhysics UMETA(DisplayName="Pre Physics"),

	/** Special tick group that starts physics simulation. */							
	TG_StartPhysics UMETA(Hidden, DisplayName="Start Physics"),

	/** Any item that can be run in parallel with our physics simulation work. */
	TG_DuringPhysics UMETA(DisplayName="During Physics"),

	/** Special tick group that ends physics simulation. */
	TG_EndPhysics UMETA(Hidden, DisplayName="End Physics"),

	/** Any item that needs rigid body and cloth simulation to be complete before being executed. */
	TG_PostPhysics UMETA(DisplayName="Post Physics"),

	/** Any item that needs the update work to be done before being ticked. */
	TG_PostUpdateWork UMETA(DisplayName="Post Update Work"),

	/** Catchall for anything demoted to the end. */
	TG_LastDemotable UMETA(Hidden, DisplayName = "Last Demotable"),

	/** Special tick group that is not actually a tick group. After every tick group this is repeatedly re-run until there are no more newly spawned items to run. */
	TG_NewlySpawned UMETA(Hidden, DisplayName="Newly Spawned"),

	TG_MAX,
};

/**
 * This is small structure to hold prerequisite tick functions
 */
USTRUCT()
struct FTickPrerequisite
{
	GENERATED_USTRUCT_BODY()

	/** Tick functions live inside of UObjects, so we need a separate weak pointer to the UObject solely for the purpose of determining if PrerequisiteTickFunction is still valid. */
	TWeakObjectPtr<class UObject> PrerequisiteObject;

	/** Pointer to the actual tick function and must be completed prior to our tick running. */
	struct FTickFunction*		PrerequisiteTickFunction;

	/** Noop constructor. */
	FTickPrerequisite()
	: PrerequisiteTickFunction(nullptr)
	{
	}
	/** 
		* Constructor
		* @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
		* @param TargetTickFunction - Actual tick function to use as a prerequisite
	**/
	FTickPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
	: PrerequisiteObject(TargetObject)
	, PrerequisiteTickFunction(&TargetTickFunction)
	{
		check(PrerequisiteTickFunction);
	}
	/** Equality operator, used to prevent duplicates and allow removal by value. */
	bool operator==(const FTickPrerequisite& Other) const
	{
		return PrerequisiteObject == Other.PrerequisiteObject &&
			PrerequisiteTickFunction == Other.PrerequisiteTickFunction;
	}
	/** Return the tick function, if it is still valid. Can be null if the tick function was null or the containing UObject has been garbage collected. */
	struct FTickFunction* Get()
	{
		if (PrerequisiteObject.IsValid(true))
		{
			return PrerequisiteTickFunction;
		}
		return nullptr;
	}
	
	const struct FTickFunction* Get() const
	{
		if (PrerequisiteObject.IsValid(true))
		{
			return PrerequisiteTickFunction;
		}
		return nullptr;
	}
};

/** 
* Abstract Base class for all tick functions.
**/
USTRUCT()
struct ENGINE_API FTickFunction
{
	GENERATED_USTRUCT_BODY()

public:
	// The following UPROPERTYs are for configuration and inherited from the CDO/archetype/blueprint etc

	/**
	 * Defines the minimum tick group for this tick function. These groups determine the relative order of when objects tick during a frame update.
	 * Given prerequisites, the tick may be delayed.
	 *
	 * @see ETickingGroup 
	 * @see FTickFunction::AddPrerequisite()
	 */
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	TEnumAsByte<enum ETickingGroup> TickGroup;

	/**
	 * Defines the tick group that this tick function must finish in. These groups determine the relative order of when objects tick during a frame update.
	 *
	 * @see ETickingGroup 
	 */
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	TEnumAsByte<enum ETickingGroup> EndTickGroup;

protected:
	/** Internal data that indicates the tick group we actually started in (it may have been delayed due to prerequisites) **/
	TEnumAsByte<enum ETickingGroup> ActualStartTickGroup;

	/** Internal data that indicates the tick group we actually started in (it may have been delayed due to prerequisites) **/
	TEnumAsByte<enum ETickingGroup> ActualEndTickGroup;

public:
	/** Bool indicating that this function should execute even if the game is paused. Pause ticks are very limited in capabilities. **/
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	uint8 bTickEvenWhenPaused:1;

	/** If false, this tick function will never be registered and will never tick. Only settable in defaults. */
	UPROPERTY()
	uint8 bCanEverTick:1;

	/** If true, this tick function will start enabled, but can be disabled later on. */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	uint8 bStartWithTickEnabled:1;

	/** If we allow this tick to run on a dedicated server */
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	uint8 bAllowTickOnDedicatedServer:1;

	/** Run this tick first within the tick group, presumably to start async tasks that must be completed with this tick group, hiding the latency. */
	uint8 bHighPriority:1;

	/** If false, this tick will run on the game thread, otherwise it will run on any thread in parallel with the game thread and in parallel with other "async ticks" **/
	uint8 bRunOnAnyThread:1;

private:
	/** If true, means that this tick function is in the master array of tick functions */
	bool bRegistered;

	/** Cache whether this function was rescheduled as an interval function during StartParallel */
	bool bWasInterval;

	enum class ETickState : uint8
	{
		Disabled,
		Enabled,
		CoolingDown
	};

	/** 
	 * If Disabled, this tick will not fire
	 * If CoolingDown, this tick has an interval frequency that is being adhered to currently
	 * CAUTION: Do not set this directly
	 **/
	ETickState TickState;

	/** Internal data to track if we have started visiting this tick function yet this frame **/
	int32 TickVisitedGFrameCounter;

	/** Internal data to track if we have finshed visiting this tick function yet this frame **/
	int32 TickQueuedGFrameCounter;

	/** Pointer to the task, only used during setup. This is often stale. **/
	void *TaskPointer;

	/** Prerequisites for this tick function **/
	TArray<struct FTickPrerequisite> Prerequisites;

	/** The next function in the cooling down list for ticks with an interval*/
	FTickFunction* Next;

	/** 
	  * If TickFrequency is greater than 0 and tick state is CoolingDown, this is the time, 
	  * relative to the element ahead of it in the cooling down list, remaining until the next time this function will tick 
	  */
	float RelativeTickCooldown;

	/** 
	* The last world game time at which we were ticked. Game time used is dependent on bTickEvenWhenPaused
	* Valid only if we've been ticked at least once since having a tick interval; otherwise set to -1.f
	*/
	float LastTickGameTimeSeconds;

public:

	/** The frequency in seconds at which this tick function will be executed.  If less than or equal to 0 then it will tick every frame */
	UPROPERTY(EditDefaultsOnly, Category="Tick", meta=(DisplayName="Tick Interval (secs)"))
	float TickInterval;

	/** Back pointer to the FTickTaskLevel containing this tick function if it is registered **/
	class FTickTaskLevel*						TickTaskLevel;

	/** Default constructor, intitalizes to reasonable defaults **/
	FTickFunction();
	/** Destructor, unregisters the tick function **/
	virtual ~FTickFunction();

	/** 
	 * Adds the tick function to the master list of tick functions. 
	 * @param Level - level to place this tick function in
	 **/
	void RegisterTickFunction(class ULevel* Level);
	/** Removes the tick function from the master list of tick functions. **/
	void UnRegisterTickFunction();
	/** See if the tick function is currently registered */
	bool IsTickFunctionRegistered() const { return bRegistered; }

	/** Enables or disables this tick function. **/
	void SetTickFunctionEnable(bool bInEnabled);
	/** Returns whether the tick function is currently enabled */
	bool IsTickFunctionEnabled() const { return TickState != ETickState::Disabled; }
	/** Returns whether it is valid to access this tick function's completion handle */
	bool IsCompletionHandleValid() const { return TaskPointer != nullptr; }
	/**
	* Gets the current completion handle of this tick function, so it can be delayed until a later point when some additional
	* tasks have been completed.  Only valid after TG_PreAsyncWork has started and then only until the TickFunction finishes
	* execution
	**/
	FGraphEventRef GetCompletionHandle() const;

	/** 
	* Gets the action tick group that this function will be elligible to start in.
	* Only valid after TG_PreAsyncWork has started through the end of the frame.
	**/
	TEnumAsByte<enum ETickingGroup> GetActualTickGroup() const
	{
		return ActualStartTickGroup;
	}

	/** 
	* Gets the action tick group that this function will be required to end in.
	* Only valid after TG_PreAsyncWork has started through the end of the frame.
	**/
	TEnumAsByte<enum ETickingGroup> GetActualEndTickGroup() const
	{
		return ActualEndTickGroup;
	}


	/** 
	 * Adds a tick function to the list of prerequisites...in other words, adds the requirement that TargetTickFunction is called before this tick function is 
	 * @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
	 * @param TargetTickFunction - Actual tick function to use as a prerequisite
	 **/
	void AddPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);
	/** 
	 * Removes a prerequisite that was previously added.
	 * @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
	 * @param TargetTickFunction - Actual tick function to use as a prerequisite
	 **/
	void RemovePrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);
	/** 
	 * Sets this function to hipri and all prerequisites recursively
	 * @param bInHighPriority - priority to set
	 **/
	void SetPriorityIncludingPrerequisites(bool bInHighPriority);

	/**
	 * @return a reference to prerequisites for this tick function.
	 */
	TArray<struct FTickPrerequisite>& GetPrerequisites()
	{
		return Prerequisites;
	}

	/**
	 * @return a reference to prerequisites for this tick function (const).
	 */
	const TArray<struct FTickPrerequisite>& GetPrerequisites() const
	{
		return Prerequisites;
	}

private:
	/**
	 * Queues a tick function for execution from the game thread
	 * @param TickContext - context to tick in
	 */
	void QueueTickFunction(class FTickTaskSequencer& TTS, const FTickContext& TickContext);

	/**
	 * Queues a tick function for execution from the game thread
	 * @param TickContext - context to tick in
	 * @param StackForCycleDetection - Stack For Cycle Detection
	 */
	void QueueTickFunctionParallel(const FTickContext& TickContext, TArray<FTickFunction*, TInlineAllocator<8> >& StackForCycleDetection);

	/** Returns the delta time to use when ticking this function given the TickContext */
	float CalculateDeltaTime(const FTickContext& TickContext);

	/** 
	 * Logs the prerequisites
	 */
	void ShowPrerequistes(int32 Indent = 1);

	/** 
	 * Abstract function actually execute the tick. 
	 * @param DeltaTime - frame time to advance, in seconds
	 * @param TickType - kind of tick for this frame
	 * @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
	 * @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	 **/
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(0); // you cannot make this pure virtual in script because it wants to create constructors.
	}
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage()
	{
		check(0); // you cannot make this pure virtual in script because it wants to create constructors.
		return FString(TEXT("invalid"));
	}
	
	friend class FTickTaskSequencer;
	friend class FTickTaskManager;
	friend class FTickTaskLevel;
	friend class FTickFunctionTask;

	// It is unsafe to copy FTickFunctions and any subclasses of FTickFunction should specify the type trait WithCopy = false
	FTickFunction& operator=(const FTickFunction&) = delete;
};

template<>
struct TStructOpsTypeTraits<FTickFunction> : public TStructOpsTypeTraitsBase2<FTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/** 
* Tick function that calls AActor::TickActor
**/
USTRUCT()
struct FActorTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/**  AActor  that is the target of this tick **/
	class AActor*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	ENGINE_API virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	ENGINE_API virtual FString DiagnosticMessage() override;
};

template<>
struct TStructOpsTypeTraits<FActorTickFunction> : public TStructOpsTypeTraitsBase2<FActorTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/** 
* Tick function that calls UActorComponent::ConditionalTick
**/
USTRUCT()
struct FActorComponentTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/**  AActor  component that is the target of this tick **/
	class UActorComponent*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	ENGINE_API virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	ENGINE_API virtual FString DiagnosticMessage() override;


	/**
	 * Conditionally calls ExecuteTickFunc if bRegistered == true and a bunch of other criteria are met
	 * @param Target - the actor component we are ticking
	 * @param bTickInEditor - whether the target wants to tick in the editor
	 * @param DeltaTime - The time since the last tick.
	 * @param TickType - Type of tick that we are running
	 * @param ExecuteTickFunc - the lambda that ultimately calls tick on the actor component
	 */

	//NOTE: This already creates a UObject stat so don't double count in your own functions

	template <typename ExecuteTickLambda>
	static void ExecuteTickHelper(UActorComponent* Target, bool bTickInEditor, float DeltaTime, ELevelTick TickType, const ExecuteTickLambda& ExecuteTickFunc);	
};


template<>
struct TStructOpsTypeTraits<FActorComponentTickFunction> : public TStructOpsTypeTraitsBase2<FActorComponentTickFunction>
{
	enum
	{
		WithCopy = false
	};
};
/** 
* Tick function that calls UPrimitiveComponent::PostPhysicsTick
**/

//DEPRECATED: This struct has been deprecated. Please use your own tick functions if you need something other than the primary tick function
USTRUCT()
struct FPrimitiveComponentPostPhysicsTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** PrimitiveComponent component that is the target of this tick **/
	class UPrimitiveComponent*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage() override;
};

template<>
struct TStructOpsTypeTraits<FPrimitiveComponentPostPhysicsTickFunction> : public TStructOpsTypeTraitsBase2<FPrimitiveComponentPostPhysicsTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/** Types of network failures broadcast from the engine */
UENUM(BlueprintType)
namespace ENetworkFailure
{
	enum Type
	{
		/** A relevant net driver has already been created for this service */
		NetDriverAlreadyExists,
		/** The net driver creation failed */
		NetDriverCreateFailure,
		/** The net driver failed its Listen() call */
		NetDriverListenFailure,
		/** A connection to the net driver has been lost */
		ConnectionLost,
		/** A connection to the net driver has timed out */
		ConnectionTimeout,
		/** The net driver received an NMT_Failure message */
		FailureReceived,
		/** The client needs to upgrade their game */
		OutdatedClient,
		/** The server needs to upgrade their game */
		OutdatedServer,
		/** There was an error during connection to the game */
		PendingConnectionFailure,
		/** NetGuid mismatch */
		NetGuidMismatch,
		/** Network checksum mismatch */
		NetChecksumMismatch
	};
}


namespace ENetworkFailure
{
	inline const TCHAR* ToString(ENetworkFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case NetDriverAlreadyExists:
			return TEXT("NetDriverAlreadyExists");
		case NetDriverCreateFailure:
			return TEXT("NetDriverCreateFailure");
		case NetDriverListenFailure:
			return TEXT("NetDriverListenFailure");
		case ConnectionLost:
			return TEXT("ConnectionLost");
		case ConnectionTimeout:
			return TEXT("ConnectionTimeout");
		case FailureReceived:
			return TEXT("FailureReceived");
		case OutdatedClient:
			return TEXT("OutdatedClient");
		case OutdatedServer:
			return TEXT("OutdatedServer");
		case PendingConnectionFailure:
			return TEXT("PendingConnectionFailure");
		case NetGuidMismatch:
			return TEXT("NetGuidMismatch");
		case NetChecksumMismatch:
			return TEXT("NetChecksumMismatch");
		}
		return TEXT("Unknown ENetworkFailure error occurred.");
	}
}

UENUM()
namespace ENetworkLagState
{
	enum Type
	{
		/** The net driver is operating normally or it is not possible to tell if it is lagging */
		NotLagging,
		/** The net driver is in the process of timing out all of the client connections */
		Lagging
	};
}


namespace ENetworkLagState
{
	inline const TCHAR* ToString(ENetworkLagState::Type LagType)
	{
		switch (LagType)
		{
			case NotLagging:
				return TEXT("NotLagging");
			case Lagging:
				return TEXT("Lagging");
		}
		return TEXT("Unknown lag type occurred.");
	}
}

/** Types of server travel failures broadcast by the engine */
UENUM(BlueprintType)
namespace ETravelFailure
{
	enum Type
	{
		/** No level found in the loaded package */
		NoLevel,
		/** LoadMap failed on travel (about to Browse to default map) */
		LoadMapFailure,
		/** Invalid URL specified */
		InvalidURL,
		/** A package is missing on the client */
		PackageMissing,
		/** A package version mismatch has occurred between client and server */
		PackageVersion,
		/** A package is missing and the client is unable to download the file */
		NoDownload,
		/** General travel failure */
		TravelFailure,
		/** Cheat commands have been used disabling travel */
		CheatCommands,
		/** Failed to create the pending net game for travel */
		PendingNetGameCreateFailure,
		/** Failed to save before travel */
		CloudSaveFailure,
		/** There was an error during a server travel to a new map */
		ServerTravelFailure,
		/** There was an error during a client travel to a new map */
		ClientTravelFailure,
	};
}

namespace ETravelFailure
{
	inline const TCHAR* ToString(ETravelFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case NoLevel:
			return TEXT("NoLevel");
		case LoadMapFailure:
			return TEXT("LoadMapFailure");
		case InvalidURL:
			return TEXT("InvalidURL");
		case PackageMissing:
			return TEXT("PackageMissing");
		case PackageVersion:
			return TEXT("PackageVersion");
		case NoDownload:
			return TEXT("NoDownload");
		case TravelFailure:
			return TEXT("TravelFailure");
		case CheatCommands:
			return TEXT("CheatCommands");
		case PendingNetGameCreateFailure:
			return TEXT("PendingNetGameCreateFailure");
		case ServerTravelFailure:
			return TEXT("ServerTravelFailure");
		case ClientTravelFailure:
			return TEXT("ClientTravelFailure");
		case CloudSaveFailure:
			return TEXT("CloudSaveFailure");
		}
		return TEXT("Unknown ETravelFailure error occurred.");
	}
}

// Traveling from server to server.
UENUM()
enum ETravelType
{
	/** Absolute URL. */
	TRAVEL_Absolute,
	/** Partial (carry name, reset server). */
	TRAVEL_Partial,
	/** Relative URL. */
	TRAVEL_Relative,
	TRAVEL_MAX,
};

/** Types of demo play failures broadcast from the engine */
UENUM(BlueprintType)
namespace EDemoPlayFailure
{
	enum Type
	{
		/** A Generic failure. */
		Generic,
		/** Demo was not found. */
		DemoNotFound,
		/** Demo is corrupt. */
		Corrupt,
		/** Invalid version. */
		InvalidVersion,
	};
}

namespace EDemoPlayFailure
{
	inline const TCHAR* ToString(EDemoPlayFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case Generic:
			return TEXT("Gneric");
		case DemoNotFound:
			return TEXT("DemoNotFound");
		case Corrupt:
			return TEXT("Corrupt");
		case InvalidVersion:
			return TEXT("InvalidVersion");
		}

		return TEXT("Unknown EDemoPlayFailure error occurred.");
	}
}

//URL structure.
USTRUCT()
struct ENGINE_API FURL
{
	GENERATED_USTRUCT_BODY()

	// Protocol, i.e. "unreal" or "http".
	UPROPERTY()
	FString Protocol;

	// Optional hostname, i.e. "204.157.115.40" or "unreal.epicgames.com", blank if local.
	UPROPERTY()
	FString Host;

	// Optional host port.
	UPROPERTY()
	int32 Port;

	// Map name, i.e. "SkyCity", default is "Entry".
	UPROPERTY()
	FString Map;

	// Optional place to download Map if client does not possess it
	UPROPERTY()
	FString RedirectURL;

	// Options.
	UPROPERTY()
	TArray<FString> Op;

	// Portal to enter through, default is "".
	UPROPERTY()
	FString Portal;

	UPROPERTY()
	int32 Valid;

	// Statics.
	static FUrlConfig UrlConfig;
	static bool bDefaultsInitialized;

	/**
	 * Prevent default from being generated.
	 */
	explicit FURL( ENoInit ) { }

	/**
	 * Construct a purely default, local URL from an optional filename.
	 */
	FURL( const TCHAR* Filename=nullptr );

	/**
	 * Construct a URL from text and an optional relative base.
	 */
	FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type );

	static void StaticInit();
	static void StaticExit();

	/**
	 * Static: Removes any special URL characters from the specified string
	 *
	 * @param Str String to be filtered
	 */
	static void FilterURLString( FString& Str );

	/**
	 * Returns whether this URL corresponds to an internal object, i.e. an Unreal
	 * level which this app can try to connect to locally or on the net. If this
	 * is false, the URL refers to an object that a remote application like Internet
	 * Explorer can execute.
	 */
	bool IsInternal() const;

	/**
	 * Returns whether this URL corresponds to an internal object on this local 
	 * process. In this case, no Internet use is necessary.
	 */
	bool IsLocalInternal() const;

	/**
	 * Tests if the URL contains an option string.
	 */
	bool HasOption( const TCHAR* Test ) const;

	/**
	 * Returns the value associated with an option.
	 *
	 * @param Match The name of the option to get.
	 * @param Default The value to return if the option wasn't found.
	 *
	 * @return The value of the named option, or Default if the option wasn't found.
	 */
	const TCHAR* GetOption( const TCHAR* Match, const TCHAR* Default ) const;

	/**
	 * Load URL from config.
	 */
	void LoadURLConfig( const TCHAR* Section, const FString& Filename=GGameIni );

	/**
	 * Save URL to config.
	 */
	void SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const FString& Filename=GGameIni ) const;

	/**
	 * Add a unique option to the URL, replacing any existing one.
	 */
	void AddOption( const TCHAR* Str );

	/**
	 * Remove an option from the URL
	 */
	void RemoveOption( const TCHAR* Key, const TCHAR* Section = nullptr, const FString& Filename = GGameIni);

	/**
	 * Convert this URL to text.
	 */
	FString ToString( bool FullyQualified=0 ) const;

	/**
	 * Serializes a FURL to or from an archive.
	 */
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FURL& U );

	/**
	 * Compare two URLs to see if they refer to the same exact thing.
	 */
	bool operator==( const FURL& Other ) const;
};

/**
 * The network mode the game is currently running.
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Overview/
 */
enum ENetMode
{
	/** Standalone: a game without networking, with one or more local players. Still considered a server because it has all server functionality. */
	NM_Standalone,

	/** Dedicated server: server with no local players. */
	NM_DedicatedServer,

	/** Listen server: a server that also has a local player who is hosting the game, available to other players on the network. */
	NM_ListenServer,

	/**
	 * Network client: client connected to a remote server.
	 * Note that every mode less than this value is a kind of server, so checking NetMode < NM_Client is always some variety of server.
	 */
	NM_Client,

	NM_MAX,
};

/**
 * Define view modes to get specific show flag settings (some on, some off and some are not altered)
 * Don't change the order, the ID is serialized with the editor
 */
UENUM()
enum EViewModeIndex 
{
	/** Wireframe w/ brushes. */
	VMI_BrushWireframe = 0,
	/** Wireframe w/ BSP. */
	VMI_Wireframe = 1,
	/** Unlit. */
	VMI_Unlit = 2,
	/** Lit. */
	VMI_Lit = 3,
	VMI_Lit_DetailLighting = 4,
	/** Lit wo/ materials. */
	VMI_LightingOnly = 5,
	/** Colored according to light count. */
	VMI_LightComplexity = 6,
	/** Colored according to shader complexity. */
	VMI_ShaderComplexity = 8,
	/** Colored according to world-space LightMap texture density. */
	VMI_LightmapDensity = 9,
	/** Colored according to light count - showing lightmap texel density on texture mapped objects. */
	VMI_LitLightmapDensity = 10,
	VMI_ReflectionOverride = 11,
	VMI_VisualizeBuffer = 12,
	//	VMI_VoxelLighting = 13,

	/** Colored according to stationary light overlap. */
	VMI_StationaryLightOverlap = 14,

	VMI_CollisionPawn = 15, 
	VMI_CollisionVisibility = 16, 
	//VMI_UNUSED = 17,
	/** Colored according to the current LOD index. */
	VMI_LODColoration = 18,
	/** Colored according to the quad coverage. */
	VMI_QuadOverdraw = 19,
	/** Visualize the accuracy of the primitive distance computed for texture streaming. */
	VMI_PrimitiveDistanceAccuracy = 20,
	/** Visualize the accuracy of the mesh UV densities computed for texture streaming. */
	VMI_MeshUVDensityAccuracy = 21,
	/** Colored according to shader complexity, including quad overdraw. */
	VMI_ShaderComplexityWithQuadOverdraw = 22,
	/** Colored according to the current HLOD index. */
	VMI_HLODColoration = 23,
	/** Group item for LOD and HLOD coloration*/
	VMI_GroupLODColoration = 24,
	/** Visualize the accuracy of the material texture scales used for texture streaming. */
	VMI_MaterialTextureScaleAccuracy = 25,
	/** Compare the required texture resolution to the actual resolution. */
	VMI_RequiredTextureResolution = 26,

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	VMI_VxgiOpacityVoxels = 27,
	VMI_VxgiEmittanceVoxels = 28,
	VMI_VxgiIrradianceVoxels = 29,
#endif
	// NVCHANGE_END: Add VXGI

	VMI_Max UMETA(Hidden),

	VMI_Unknown = 255 UMETA(Hidden),
};


/** Settings to allow designers to override the automatic expose */
USTRUCT()
struct FExposureSettings
{
	GENERATED_USTRUCT_BODY()

	FExposureSettings()
		: LogOffset(0), bFixed(false)
	{
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d"), LogOffset, bFixed ? 1 : 0);
	}

	void SetFromString(const TCHAR *In)
	{
		// set to default
		*this = FExposureSettings();

		const TCHAR* Comma = FCString::Strchr(In, TEXT(','));
		check(Comma);

		const int32 BUFFER_SIZE = 128;
		TCHAR Buffer[BUFFER_SIZE];
		check((Comma-In)+1 < BUFFER_SIZE);
		
		FCString::Strncpy(Buffer, In, (Comma-In)+1);
		LogOffset = FCString::Atoi(Buffer);
		bFixed = !!FCString::Atoi(Comma+1);
	}

	// usually -4:/16 darker .. +4:16x brighter
	UPROPERTY()
	int32 LogOffset;
	// true: fixed exposure using the LogOffset value, false: automatic eye adaptation
	UPROPERTY()
	bool bFixed;
};


UCLASS(abstract, config=Engine)
class UEngineBaseTypes : public UObject
{
	GENERATED_UCLASS_BODY()

};

