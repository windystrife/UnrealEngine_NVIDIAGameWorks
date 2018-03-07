// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Engine.h: Unreal engine public header file.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HitProxies.h"
#include "Engine/World.h"
#include "UObject/UObjectHash.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "GameFramework/WorldSettings.h"

class FCanvas;
class FViewport;
class UCanvas;
class UConsole;
class UPrimitiveComponent;

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit an actor.
struct HActor : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API )
	AActor* Actor;
	const UPrimitiveComponent* PrimComponent;
	int32 SectionIndex;
	int32 MaterialIndex;

	HActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent )
		: Actor( InActor ) 
		, PrimComponent( InPrimComponent )
		, SectionIndex( -1 )
		, MaterialIndex( -1 )
		{}

	HActor(AActor* InActor, const UPrimitiveComponent* InPrimComponent, int32 InSectionIndex, int32 InMaterialIndex)
		: Actor( InActor )
		, PrimComponent( InPrimComponent )
		, SectionIndex(InSectionIndex)
		, MaterialIndex( InMaterialIndex )
		{}

	HActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority) 
		: HHitProxy( InPriority )
		, Actor( InActor )
		, PrimComponent( InPrimComponent )
		, SectionIndex( -1 )
		, MaterialIndex( -1 )
		{}

	HActor(AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority, int32 InSectionIndex, int32 InMaterialIndex)
		: HHitProxy(InPriority)
		, Actor(InActor)
		, PrimComponent(InPrimComponent)
		, SectionIndex(InSectionIndex)
		, MaterialIndex(InMaterialIndex)
		{}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );
		Collector.AddReferencedObject( PrimComponent );
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

//
//	HBSPBrushVert
//

struct HBSPBrushVert : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );
	TWeakObjectPtr<ABrush>	Brush;
	FVector* Vertex;
	HBSPBrushVert(ABrush* InBrush,FVector* InVertex):
		HHitProxy(HPP_UI),
		Brush(InBrush),
		Vertex(InVertex)
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		//@todo: Brush Hit proxies are currently referencing between UWorld's (undesired), 
		// once this issue is resolved remove the TWeakObjectPtr and replace with Standard ABrush*.
		// Also uncomment this line:
		//Collector.AddReferencedObject( Brush );
	}
};

//
//	HStaticMeshVert
//

struct HStaticMeshVert : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );
	AActor*	Actor;
	FVector Vertex;
	HStaticMeshVert(AActor* InActor,FVector InVertex):
		HHitProxy(HPP_UI),
		Actor(InActor),
		Vertex(InVertex)
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );
	}
};

// Hit an actor even with translucency
struct HTranslucentActor : public HActor
{
	DECLARE_HIT_PROXY( ENGINE_API )
	HTranslucentActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent ) 
		: HActor( InActor, InPrimComponent ) 
		{}

	HTranslucentActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority) 
		: HActor(InActor, InPrimComponent, InPriority)
		{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return true;
	}
};


/*-----------------------------------------------------------------------------
	Iterator for the editor that loops through all selected actors.
-----------------------------------------------------------------------------*/

/**
 * Abstract base class for actor iteration. Implements all operators and relies on IsActorSuitable
 * to be overridden by derived class.
 * Note that when Playing In Editor, this will find actors only in CurrentWorld.
 */
class FActorIteratorState
{
public:
	/** Current world we are iterating upon						*/
	UWorld* CurrentWorld;
	/** Results from the GetObjectsOfClass query				*/
	TArray<UObject*> ObjectArray;
	/** index of the current element in the object array		*/
	int32 Index;
	/** Whether we already reached the end						*/
	bool	ReachedEnd;
	/** Number of actors that have been considered thus far		*/
	int32		ConsideredCount;
	/** Current actor pointed to by actor iterator				*/
	AActor*	CurrentActor;
	/** Contains any actors spawned during iteration			*/
	TArray<AActor*> SpawnedActorArray;
	/** The class type we are iterating, kept for filtering		*/
	UClass* DesiredClass;
	/** Handle to the registered OnActorSpawned delegate		*/
	FDelegateHandle ActorSpawnedDelegateHandle;

	/**
	 * Default ctor, inits everything
	 */
	FActorIteratorState( UWorld* InWorld, TSubclassOf<AActor> InClass ) :
		CurrentWorld( InWorld ),
		Index( -1 ),
		ReachedEnd( false ),
		ConsideredCount( 0 ),
		CurrentActor( NULL ),
		DesiredClass(InClass)
	{
		check(IsInGameThread());
		check(CurrentWorld);
		EObjectFlags ExcludeFlags = RF_ClassDefaultObject;
		GetObjectsOfClass(InClass, ObjectArray, true, ExcludeFlags, EInternalObjectFlags::PendingKill);

		auto ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FActorIteratorState::OnActorSpawned);
		ActorSpawnedDelegateHandle = CurrentWorld->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
	}

	~FActorIteratorState()
	{
		CurrentWorld->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* GetActorChecked() const
	{
		check(CurrentActor);
		checkf(!CurrentActor->IsUnreachable(), TEXT("%s"), *CurrentActor->GetFullName());
		return CurrentActor;
	}

private:
	void OnActorSpawned(AActor* InActor)
	{
		if (InActor->IsA(DesiredClass))
		{
			SpawnedActorArray.AddUnique(InActor);
		}
	}
};

/** Type enum, used to represent the special End iterator */
enum class EActorIteratorType
{
	End
};

/** Iteration flags, specifies which types of levels and actors should be iterated */
enum class EActorIteratorFlags
{
	AllActors			= 0x00000000, // No flags, iterate all actors
	SkipPendingKill		= 0x00000001, // Skip pending kill actors
	OnlySelectedActors	= 0x00000002, // Only iterate actors that are selected
	OnlyActiveLevels	= 0x00000004, // Only iterate active levels
};
ENUM_CLASS_FLAGS(EActorIteratorFlags);

/**
 * Template class used to filter actors by certain characteristics
 */
template <typename Derived>
class TActorIteratorBase
{
public:
	/**
	 * Iterates to next suitable actor.
	 */
	void operator++()
	{
		// Use local version to avoid LHSs as compiler is not required to write out member variables to memory.
		AActor*           LocalCurrentActor      = nullptr;
		int32             LocalIndex             = State->Index;
		TArray<UObject*>& LocalObjectArray       = State->ObjectArray;
		TArray<AActor*>&  LocalSpawnedActorArray = State->SpawnedActorArray;
		UWorld*           LocalCurrentWorld      = State->CurrentWorld;
		while(++LocalIndex < (LocalObjectArray.Num() + LocalSpawnedActorArray.Num()))
		{
			if (LocalIndex < LocalObjectArray.Num())
			{
				LocalCurrentActor = static_cast<AActor*>(LocalObjectArray[LocalIndex]);
			}
			else
			{
				LocalCurrentActor = LocalSpawnedActorArray[LocalIndex - LocalObjectArray.Num()];
			}
			State->ConsideredCount++;
			
			ULevel* ActorLevel = LocalCurrentActor ? LocalCurrentActor->GetLevel() : nullptr;
			if ( ActorLevel
				&& static_cast<const Derived*>(this)->IsActorSuitable(LocalCurrentActor)
				&& static_cast<const Derived*>(this)->CanIterateLevel(ActorLevel)
				&& ActorLevel->GetWorld() == LocalCurrentWorld)
			{
				// ignore non-persistent world settings
				if (LocalCurrentActor->GetLevel() == LocalCurrentWorld->PersistentLevel || !LocalCurrentActor->IsA(AWorldSettings::StaticClass()))
				{
					State->CurrentActor = LocalCurrentActor;
					State->Index = LocalIndex;
					return;
				}
			}
		}
		State->CurrentActor = NULL;
		State->ReachedEnd = true;
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator*() const
	{
		return State->GetActorChecked();
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator->() const
	{
		return State->GetActorChecked();
	}
	/**
	 * Returns whether the iterator has reached the end and no longer points
	 * to a suitable actor.
	 *
	 * @return true if iterator points to a suitable actor, false if it has reached the end
	 */
	FORCEINLINE explicit operator bool() const
	{
		return !State->ReachedEnd;
	}

	/**
	 * Clears the current Actor in the array (setting it to NULL).
	 */
	void ClearCurrent()
	{
		check(!State->ReachedEnd);
		State->CurrentWorld->RemoveActor(State->CurrentActor, true);
	}

	/**
	 * Returns the number of actors considered thus far. Can be used in combination
	 * with GetProgressDenominator to gauge progress iterating over all actors.
	 *
	 * @return number of actors considered thus far.
	 */
	int32 GetProgressNumerator() const
	{
		return State->ConsideredCount;
	}

protected:
	/**
	 * Hide the constructors as construction on this class should only be done by subclasses
	 */
	explicit TActorIteratorBase(EActorIteratorType)
		: Flags(EActorIteratorFlags::AllActors)
	{
	}

	TActorIteratorBase(UWorld* InWorld, TSubclassOf<AActor> InClass, EActorIteratorFlags InFlags)
		: Flags(InFlags)
	{
		State.Emplace(InWorld, InClass);
	}

	/**
	 * Determines whether this is a valid actor or not.
	 * This function should be redefined (thus hiding this one) in a derived class if it wants special actor filtering.
	 *
	 * @param	Actor	Actor to check
	 * @return	true
	 */
	FORCEINLINE bool IsActorSuitable(AActor* Actor) const
	{
		if (EnumHasAnyFlags(Flags, EActorIteratorFlags::SkipPendingKill) && Actor->IsPendingKill())
		{
			return false;
		}

		if (EnumHasAnyFlags(Flags, EActorIteratorFlags::OnlySelectedActors) && !Actor->IsSelected())
		{
			return false;
		}

		return true;
	}

	/**
	 * Used to examine whether this level is valid for iteration or not
	 * This function should be redefined (thus hiding this one) in a derived class if it wants special level filtering.
	 *
	 * @param Level the level to check for iteration
	 * @return true if the level can be iterated, false otherwise
	 */
	FORCEINLINE bool CanIterateLevel(ULevel* Level) const
	{
		if (EnumHasAnyFlags(Flags, EActorIteratorFlags::OnlyActiveLevels))
		{
			const bool bIsLevelVisibleOrAssociating = Level->bIsVisible || Level->bIsAssociatingLevel;

			// Only allow iteration of Level if it's in the currently active level collection of the world, or is a static level.
			const FLevelCollection* const ActorLevelCollection = Level->GetCachedLevelCollection();
			const FLevelCollection* const ActiveLevelCollection = Level->OwningWorld ? Level->OwningWorld->GetActiveLevelCollection() : nullptr;

			// If the world's active level collection is null, we can't apply any meaningful filter,
			// so just allow iteration in this case.
			const bool bIsCurrentLevelCollectionTicking = !ActiveLevelCollection || (ActorLevelCollection == ActiveLevelCollection);

			const bool bIsLevelCollectionNullOrStatic = !ActorLevelCollection || ActorLevelCollection->GetType() == ELevelCollectionType::StaticLevels;
			const bool bShouldIterateLevelCollection = bIsCurrentLevelCollectionTicking || bIsLevelCollectionNullOrStatic;

			return bIsLevelVisibleOrAssociating && bShouldIterateLevelCollection;
		}

		return true;
	}

private:
	EActorIteratorFlags Flags;
	TOptional<FActorIteratorState> State;

	friend bool operator==(const TActorIteratorBase& Lhs, const TActorIteratorBase& Rhs) { check(!Rhs.State); return  !Lhs; }
	friend bool operator!=(const TActorIteratorBase& Lhs, const TActorIteratorBase& Rhs) { check(!Rhs.State); return !!Lhs; }
};

/**
 * Actor iterator
 * Note that when Playing In Editor, this will find actors only in CurrentWorld
 */
class FActorIterator : public TActorIteratorBase<FActorIterator>
{
	friend class TActorIteratorBase<FActorIterator>;
	typedef TActorIteratorBase<FActorIterator> Super;

public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 */
	explicit FActorIterator(UWorld* InWorld, EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill)
		: Super(InWorld, AActor::StaticClass(), InFlags)
	{
		++(*this);
	}

	/**
	 * Constructor for creating an end iterator
	 */
	explicit FActorIterator(EActorIteratorType)
		: Super(EActorIteratorType::End)
	{
	}
};

/**
 * Actor range for ranged-for support.
 * Note that when Playing In Editor, this will find actors only in CurrentWorld
 */
class FActorRange
{
public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 */
	explicit FActorRange(UWorld* InWorld, EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill)
		: Flags(InFlags)
		, World(InWorld)
	{
	}

private:
	EActorIteratorFlags	Flags;
	UWorld*				World;

	friend FActorIterator begin(const FActorRange& Range) { return FActorIterator(Range.World, Range.Flags); }
	friend FActorIterator end  (const FActorRange& Range) { return FActorIterator(EActorIteratorType::End); }
};

/**
 * Template actor iterator.
 */
template <typename ActorType>
class TActorIterator : public TActorIteratorBase<TActorIterator<ActorType>>
{
	friend class TActorIteratorBase<TActorIterator>;
	typedef TActorIteratorBase<TActorIterator> Super;

public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 * @param  InClass  The subclass of actors to be iterated over.
	 */
	explicit TActorIterator(UWorld* InWorld, TSubclassOf<ActorType> InClass = ActorType::StaticClass(), EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill)
		: Super(InWorld, InClass, InFlags)
	{
		++(*this);
	}

	/**
	 * Constructor for creating an end iterator
	 */
	explicit TActorIterator(EActorIteratorType)
		: Super(EActorIteratorType::End)
	{
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE ActorType* operator*() const
	{
		return CastChecked<ActorType>(**static_cast<const Super*>(this));
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE ActorType* operator->() const
	{
		return **this;
	}
};

/**
 * Template actor range for ranged-for support.
 */
template <typename ActorType>
class TActorRange
{
public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 * @param  InClass  The subclass of actors to be iterated over.
	 */
	explicit TActorRange(UWorld* InWorld, TSubclassOf<ActorType> InClass = ActorType::StaticClass(), EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill)
		: Flags(InFlags)
		, World(InWorld)
		, Class(InClass)
	{
	}

private:
	EActorIteratorFlags		Flags;
	UWorld*					World;
	TSubclassOf<ActorType>	Class;

	friend TActorIterator<ActorType> begin(const TActorRange& Range) { return TActorIterator<ActorType>(Range.World, Range.Class, Range.Flags); }
	friend TActorIterator<ActorType> end(const TActorRange& Range) { return TActorIterator<ActorType>(EActorIteratorType::End); }
};

/**
 * Selected actor iterator, this is for ease of use but the same can be done by adding EActorIteratorFlags::OnlySelectedActors to 
 */
class FSelectedActorIterator : public TActorIteratorBase<FSelectedActorIterator>
{
	friend class TActorIteratorBase<FSelectedActorIterator>;
	typedef TActorIteratorBase<FSelectedActorIterator> Super;

public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 */
	explicit FSelectedActorIterator(UWorld* InWorld)
		: Super(InWorld, AActor::StaticClass(), EActorIteratorFlags::SkipPendingKill | EActorIteratorFlags::OnlySelectedActors)
	{
		++(*this);
	}

	/**
	 * Constructor for creating an end iterator
	 */
	explicit FSelectedActorIterator(EActorIteratorType)
		: Super(EActorIteratorType::End)
	{
	}
};

/**
 * Selected actor range for ranged-for support.
 */
class FSelectedActorRange
{
public:
	/**
	 * Constructor
	 *
	 * @param  InWorld  The world whose actors are to be iterated over.
	 */
	explicit FSelectedActorRange(UWorld* InWorld)
		: World(InWorld)
	{
	}

private:
	UWorld* World;

	friend FSelectedActorIterator begin(const FSelectedActorRange& Range) { return FSelectedActorIterator(Range.World); }
	friend FSelectedActorIterator end  (const FSelectedActorRange& Range) { return FSelectedActorIterator(EActorIteratorType::End); }
};

/**
 * An output device that forwards output to both the log and the console.
 */
class ENGINE_API FConsoleOutputDevice : public FStringOutputDevice
{
	typedef FStringOutputDevice Super;

public:

	/**
	 * Minimal initialization constructor.
	 */
	FConsoleOutputDevice(class UConsole* InConsole):
		Super(TEXT("")),
		Console(InConsole)
	{}

	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const class FName& Category) override;

private:

	/** The console which output is written to. */
	UConsole* Console;
};


/**
 *	Renders stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation	Location of camera
 *	@param ViewRotation	Rotation of camera
 */
ENGINE_API void DrawStatsHUD( UWorld* InWorld, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<struct FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation );

/** SubLevel status information */
struct FSubLevelStatus
{
	FName				PackageName;
	EStreamingStatus	StreamingStatus;
	int32				LODIndex;
	bool				bPlayerInside;
};

/**
 *	Gathers SubLevels status from a provided world
 *	@param InWorld		World to gather sublevels stats from
 *	@return				sublevels status (streaming state, LOD index, where player is)
 */
TArray<FSubLevelStatus> GetSubLevelsStatus( UWorld* InWorld );

#if !UE_BUILD_SHIPPING

// Helper class containing information about UObject assets referenced.
struct FContentComparisonAssetInfo
{
	/** Name of the asset */
	FString AssetName;
	/** The resource size of the asset */
	int32 ResourceSize;

	/** Constructor */
	FContentComparisonAssetInfo()
	{
		FMemory::Memzero(this, sizeof(FContentComparisonAssetInfo));
	}

	/** operator == */
	bool operator==(const FContentComparisonAssetInfo& Other)
	{
		return (
			(AssetName == Other.AssetName) &&
			(ResourceSize == Other.ResourceSize)
			);
	}

	/** operator = */
	FContentComparisonAssetInfo& operator=(const FContentComparisonAssetInfo& Other)
	{
		AssetName = Other.AssetName;
		ResourceSize = Other.ResourceSize;
		return *this;
	}
};

/** Helper class for performing the content comparison console command */
class FContentComparisonHelper
{
public:
	FContentComparisonHelper();
	virtual ~FContentComparisonHelper();

	/**
	 *	Compare the classes derived from the given base class.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	bool					true if successful, false if not
	 */
	virtual bool CompareClasses(const FString& InBaseClassName, int32 InRecursionDepth);

	/**
	 *	Compare the classes derived from the given base class, ignoring specified base classes.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InBaseClassesToIgnore	The base classes to ignore when processing objects.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	bool					true if successful, false if not
	 */
	virtual bool CompareClasses(const FString& InBaseClassName, const TArray<FString>& InBaseClassesToIgnore, int32 InRecursionDepth);

	/**
	 *	Recursive function for collecting objects referenced by the given object.
	 *
	 *	@param	InStartObject			The object to collect the referencees for
	 *	@param	InCurrDepth				The current depth being processed
	 *	@param	InMaxDepth				The maximum depth to traverse the reference chain
	 *	@param	OutCollectReferences	The resulting referenced object list
	 */
	void RecursiveObjectCollection(UObject* InStartObject, int32 InCurrDepth, int32 InMaxDepth, TMap<UObject*,bool>& OutCollectedReferences);

protected:
	TMap<FString,bool> ReferenceClassesOfInterest;
};
#endif

namespace EngineUtils
{
	enum EAssetToLoad
	{
		ATL_Regular,
		ATL_Class,
	};

	/** Loads all the assets found in the specified path and subpaths */
	ENGINE_API bool FindOrLoadAssetsByPath(const FString& Path, TArray<UObject*>& OutAssets, EAssetToLoad Type);
}

/** Helper class for serializing flags describing which data have been stripped (if any). */
class ENGINE_API FStripDataFlags
{
	/** Serialized engine strip flags (up to 8 flags). */
	uint8 GlobalStripFlags;
	/** Serialized per-class strip flags (user defined, up to 8 flags). */
	uint8 ClassStripFlags;

public:

	/** Engine strip flags */
	enum EStrippedData
	{
		None = 0,

		/* Editor data */
		Editor = 1,
		/* All data not required for dedicated server to work correctly (usually includes editor data). */
		Server = 2,

		// Add global flags here (up to 8 including the already defined ones).

		/** All flags */
		All = 0xff
	};

	/** 
	 * Constructor.
	 * Serializes strip data flags. Global (engine) flags are automatically generated from target platform
	 * when saving. Class flags need to be defined by the user.
	 *
	 * @param Ar - Archive to serialize with.
	 * @param InClassFlags - User defined per class flags .
	 * @param InVersion - Minimal strip version required to serialize strip flags
	 */
	FStripDataFlags( FArchive& Ar, uint8 InClassFlags = 0, int32 InVersion = VER_UE4_OLDEST_LOADABLE_PACKAGE );

	/** 
	 * Constructor.
	 * Serializes strip data flags. Global (engine) flags are user defined and will not be automatically generated
	 * when saving. Class flags also need to be defined by the user.
	 *
	 * @param Ar - Archive to serialize with.
	 * @param InClassFlags - User defined per class flags.
	 * @param InVersion - Minimal version required to serialize strip flags
	 */
	FStripDataFlags( FArchive& Ar, uint8 InGlobalFlags, uint8 InClassFlags, int32 InVersion = VER_UE4_OLDEST_LOADABLE_PACKAGE );

	/**
	 * Checks if FStripDataFlags::Editor flag is set or not
	 *
	 * @return true if FStripDataFlags::Editor is set, false otherwise.
	 */
	FORCEINLINE bool IsEditorDataStripped() const
	{
		return (GlobalStripFlags & FStripDataFlags::Editor) != 0;
	}

	/**
	 * Checks if FStripDataFlags::Server flag is set or not
	 *
	 * @return true if FStripDataFlags::Server is set, false otherwise.
	 */
	bool IsDataStrippedForServer() const
	{
		return (GlobalStripFlags & FStripDataFlags::Server) != 0;
	}

	/**
	 * Checks if user defined flags are set or not.
	 *
	 * @InFlags - User defined flags to check.
	 * @return true if the specified user defined flags are set, false otherwise.
	 */
	bool IsClassDataStripped(uint8 InFlags) const
	{
		return (ClassStripFlags & InFlags) != 0;
	}
};

