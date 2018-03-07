// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "GameFramework/Actor.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EngineDefines.h"
#include "NavigationData.generated.h"

class ANavigationData;
class Error;
class FNavDataGenerator;
class INavAgentInterface;
class INavLinkCustomInterface;
class UNavArea;
class UPrimitiveComponent;

USTRUCT()
struct ENGINE_API FSupportedAreaData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString AreaClassName;

	UPROPERTY()
	int32 AreaID;

	UPROPERTY(transient)
	const UClass* AreaClass;

	FSupportedAreaData(TSubclassOf<UNavArea> NavAreaClass = NULL, int32 InAreaID = INDEX_NONE);
};

struct FNavPathRecalculationRequest
{
	FNavPathWeakPtr Path;
	ENavPathUpdateType::Type Reason;

	FNavPathRecalculationRequest(const FNavPathSharedPtr& InPath, ENavPathUpdateType::Type InReason)
		: Path(InPath.ToSharedRef()), Reason(InReason)
	{}

	bool operator==(const FNavPathRecalculationRequest& Other) const { return Path == Other.Path;  }
};

struct FPathFindingResult
{
	FNavPathSharedPtr Path;
	ENavigationQueryResult::Type Result;

	FPathFindingResult(ENavigationQueryResult::Type InResult = ENavigationQueryResult::Invalid) : Result(InResult)
	{ }

	FORCEINLINE bool IsSuccessful() const
	{
		return Result == ENavigationQueryResult::Success;
	}
	FORCEINLINE bool IsPartial() const;
};

struct ENGINE_API FNavigationPath : public TSharedFromThis<FNavigationPath, ESPMode::ThreadSafe>
{
	//DECLARE_DELEGATE_OneParam(FPathObserverDelegate, FNavigationPath*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPathObserverDelegate, FNavigationPath*, ENavPathEvent::Type);

	FNavigationPath();
	FNavigationPath(const TArray<FVector>& Points, AActor* Base = NULL);
	virtual ~FNavigationPath()
	{ }

	FORCEINLINE bool IsValid() const
	{
		return bIsReady && PathPoints.Num() > 1 && bUpToDate;
	}
	FORCEINLINE bool IsUpToDate() const
	{
		return bUpToDate;
	}
	FORCEINLINE bool IsReady() const
	{
		return bIsReady;
	}
	FORCEINLINE bool IsPartial() const
	{
		return bIsPartial;
	}
	FORCEINLINE bool DidSearchReachedLimit() const
	{
		return bReachedSearchLimit;
	}
	FORCEINLINE bool IsWaitingForRepath() const
	{
		return bWaitingForRepath;
	}
	FORCEINLINE void SetManualRepathWaiting(const bool bInWaitingForRepath)
	{
		bWaitingForRepath = bInWaitingForRepath;
	}
	FORCEINLINE bool ShouldUpdateStartPointOnRepath() const
	{
		return bUpdateStartPointOnRepath;
	}
	FORCEINLINE bool ShouldUpdateEndPointOnRepath() const
	{
		return bUpdateEndPointOnRepath;
	}
	FORCEINLINE FVector GetDestinationLocation() const
	{
		return IsValid() ? PathPoints.Last().Location : INVALID_NAVEXTENT;
	}
	FORCEINLINE FPathObserverDelegate& GetObserver()
	{
		return ObserverDelegate;
	}
	FORCEINLINE FDelegateHandle AddObserver(FPathObserverDelegate::FDelegate NewObserver)
	{
		return ObserverDelegate.Add(NewObserver);
	}
	FORCEINLINE void RemoveObserver(FDelegateHandle HandleOfObserverToRemove)
	{
		ObserverDelegate.Remove(HandleOfObserverToRemove);
	}

	FORCEINLINE void MarkReady()
	{
		bIsReady = true;
	}

	FORCEINLINE void SetNavigationDataUsed(const ANavigationData* const NewData);

	FORCEINLINE ANavigationData* GetNavigationDataUsed() const
	{
		return NavigationDataUsed.Get();
	}
	FORCEINLINE void SetQuerier(const UObject* InQuerier)
	{
		PathFindingQueryData.Owner = InQuerier;
	}
	FORCEINLINE const UObject* GetQuerier() const
	{
		return PathFindingQueryData.Owner.Get();
	}
	FORCEINLINE void SetQueryData(const FPathFindingQueryData& QueryData)
	{
		PathFindingQueryData = QueryData;
	}
	FORCEINLINE FPathFindingQueryData GetQueryData() const
	{
		// return copy of query data
		return PathFindingQueryData;
	}

	//FORCEINLINE void SetObserver(const FPathObserverDelegate& Observer) { ObserverDelegate = Observer; }
	FORCEINLINE void SetIsPartial(const bool bPartial)
	{
		bIsPartial = bPartial;
	}
	FORCEINLINE void SetSearchReachedLimit(const bool bLimited)
	{
		bReachedSearchLimit = bLimited;
	}

	FORCEINLINE void SetFilter(FSharedConstNavQueryFilter InFilter)
	{
		PathFindingQueryData.QueryFilter = InFilter;
		Filter = InFilter;
	}
	FORCEINLINE FSharedConstNavQueryFilter GetFilter() const
	{
		return PathFindingQueryData.QueryFilter;
	}
	FORCEINLINE AActor* GetBaseActor() const
	{
		return Base.Get();
	}

	FVector GetStartLocation() const
	{
		return PathPoints.Num() > 0 ? PathPoints[0].Location : FNavigationSystem::InvalidLocation;
	}
	FVector GetEndLocation() const
	{
		return PathPoints.Num() > 0 ? PathPoints.Last().Location : FNavigationSystem::InvalidLocation;
	}

	FORCEINLINE void DoneUpdating(ENavPathUpdateType::Type UpdateType)
	{
		static const ENavPathEvent::Type PathUpdateTypeToPathEvent[] = {
			ENavPathEvent::UpdatedDueToGoalMoved // GoalMoved,
			, ENavPathEvent::UpdatedDueToNavigationChanged // NavigationChanged,
			, ENavPathEvent::MetaPathUpdate // MetaPathUpdate,
			, ENavPathEvent::Custom // Custom,
		};

		bUpToDate = true;
		bWaitingForRepath = false;

		if (bUseOnPathUpdatedNotify)
		{
			// notify path before observers
			OnPathUpdated(UpdateType);
		}
		
		ObserverDelegate.Broadcast(this, PathUpdateTypeToPathEvent[uint8(UpdateType)]);
	}

	FORCEINLINE float GetTimeStamp() const { return LastUpdateTimeStamp; }
	FORCEINLINE void SetTimeStamp(float TimeStamp) { LastUpdateTimeStamp = TimeStamp; }

	void Invalidate();
	void RePathFailed();

	/** Resets all variables describing generated path before attempting new pathfinding call. 
	  * This function will NOT reset setup variables like goal actor, filter, observer, etc */
	virtual void ResetForRepath();

	virtual void DebugDraw(const ANavigationData* NavData, FColor PathColor, class UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex = 0) const;
	
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
	virtual FString GetDescription() const;
#endif // ENABLE_VISUAL_LOG

	/** check if path contains specific custom nav link */
	virtual bool ContainsCustomLink(uint32 UniqueLinkId) const;

	/** check if path contains any custom nav link */
	virtual bool ContainsAnyCustomLink() const;

	/** check if path contains given node */
	virtual bool ContainsNode(NavNodeRef NodeRef) const;

	/** get cost of path, starting from given point */
	virtual float GetCostFromIndex(int32 PathPointIndex) const
	{
		return 0.0f;
	}

	/** get cost of path, starting from given node */
	virtual float GetCostFromNode(NavNodeRef PathNode) const
	{
		return 0.0f;
	}

	FORCEINLINE float GetCost() const
	{
		return GetCostFromIndex(0);
	}

	/** calculates total length of segments from NextPathPoint to the end of path, plus distance from CurrentPosition to NextPathPoint */
	virtual float GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const;

	FORCEINLINE float GetLength() const
	{
		return PathPoints.Num() ? GetLengthFromPosition(PathPoints[0].Location, 1) : 0.0f;
	}

	static bool GetPathPoint(const FNavigationPath* Path, uint32 PathVertIdx, FNavPathPoint& PathPoint)
	{
		if (Path && Path->GetPathPoints().IsValidIndex((int32)PathVertIdx))
		{
			PathPoint = Path->PathPoints[PathVertIdx];
			return true;
		}

		return false;
	}

	FORCEINLINE const TArray<FNavPathPoint>& GetPathPoints() const
	{
		return PathPoints;
	}
	FORCEINLINE TArray<FNavPathPoint>& GetPathPoints()
	{
		return PathPoints;
	}

	/** get based position of path point */
	FBasedPosition GetPathPointLocation(uint32 Index) const;

	/** checks if given path, starting from StartingIndex, intersects with given AABB box */
	virtual bool DoesIntersectBox(const FBox& Box, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const;
	/** checks if given path, starting from StartingIndex, intersects with given AABB box. This version uses AgentLocation as beginning of the path
	 *	with segment between AgentLocation and path's StartingIndex-th node treated as first path segment to check */
	virtual bool DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const;
	/** retrieves normalized direction vector to given path segment
	 *	for '0'-th segment returns same as for 1st segment 
	 */
	virtual FVector GetSegmentDirection(uint32 SegmentEndIndex) const;

private:
	bool DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const;

	/** reset variables describing built path, leaves setup variables required for rebuilding */
	void InternalResetNavigationPath();

public:

	/** type safe casts */
	template<typename PathClass>
	FORCEINLINE const PathClass* CastPath() const
	{
		return PathType.IsA(PathClass::Type) ? static_cast<PathClass*>(this) : NULL;
	}

	template<typename PathClass>
	FORCEINLINE PathClass* CastPath()
	{
		return PathType.IsA(PathClass::Type) ? static_cast<PathClass*>(this) : NULL;
	}

	/** enables path observing specified AActor's location and update itself if actor changes location */
	void SetGoalActorObservation(const AActor& ActorToObserve, float TetherDistance);
	/** turns goal actor location's observation */
	void DisableGoalActorObservation();
	/** set's up the path to use SourceActor's location in case of recalculation */
	void SetSourceActor(const AActor& InSourceActor);

	const AActor* GetSourceActor() const { return SourceActor.Get(); }
	const INavAgentInterface* GetSourceActorAsNavAgent() const { return SourceActorAsNavAgent; }

	FVector GetLastRepathGoalLocation() const { return GoalActorLastLocation; }
	void UpdateLastRepathGoalLocation();
	
	float GetLastUpdateTime() const { return LastUpdateTimeStamp; }
	float GetGoalActorTetherDistance() const { return FMath::Sqrt(GoalActorLocationTetherDistanceSq); }

	/** if enabled path will request recalculation if it gets invalidated due to a change to underlying navigation */
	void EnableRecalculationOnInvalidation(bool bShouldAutoUpdate)
	{
		bDoAutoUpdateOnInvalidation = bShouldAutoUpdate;
	}
	bool WillRecalculateOnInvalidation() const
	{
		return bDoAutoUpdateOnInvalidation;
	}

	/** if ignoring, path will stay bUpToDate after being invalidated due to a change to underlying navigation (observer and auto repath will NOT be triggered!) */
	void SetIgnoreInvalidation(bool bShouldIgnore) { bIgnoreInvalidation = bShouldIgnore; }
	bool GetIgnoreInvalidation() const { return bIgnoreInvalidation; }

	EPathObservationResult::Type TickPathObservation();

	/** If GoalActor is set it retrieved its navigation location, if not retrieved last path point location */
	FVector GetGoalLocation() const;

	/** retrieved location to start path finding from (in case of path recalculation) */
	FVector GetPathFindingStartLocation() const;

	const AActor* GetGoalActor() const
	{
		return GoalActor.Get();
	}
	const INavAgentInterface* GetGoalActorAsNavAgent() const
	{
		return GoalActor.IsValid() ? GoalActorAsNavAgent : NULL;
	}

	// @todo this is navigation-type specific and should not be implemented here.
	/** additional node refs used during path following shortcuts */
	TArray<NavNodeRef> ShortcutNodeRefs;

protected:
	
	/** optional notify called when path finishes update, before broadcasting to observes - requires bUseOnPathUpdatedNotify flag set */
	virtual void OnPathUpdated(ENavPathUpdateType::Type UpdateType) {};
	
	/**
	* IMPORTANT: path is assumed to be valid if it contains _MORE_ than _ONE_ point
	*	point 0 is path's starting point - if it's the only point on the path then there's no path per se
	*/
	TArray<FNavPathPoint> PathPoints;

	/** base actor, if exist path points locations will be relative to it */
	TWeakObjectPtr<AActor> Base;

private:
	/** if set path will observe GoalActor's location and update itself if goal moves more then
	*	@note only actual navigation paths can use this feature, meaning the ones associated with
	*	a NavigationData instance (meaning NavigationDataUsed != NULL) */
	TWeakObjectPtr<const AActor> GoalActor;

	/** cached result of GoalActor casting to INavAgentInterface */
	const INavAgentInterface* GoalActorAsNavAgent;

	/** if set will be queried for location in case of path's recalculation */
	TWeakObjectPtr<const AActor> SourceActor;

	/** cached result of PathSource casting to INavAgentInterface */
	const INavAgentInterface* SourceActorAsNavAgent;

protected:
	// DEPRECATED: filter used to build this path
	FSharedConstNavQueryFilter Filter;

	/** type of path */
	static const FNavPathType Type;

	FNavPathType PathType;

	/** A delegate that will be called when path becomes invalid */
	FPathObserverDelegate ObserverDelegate;

	/** "true" until navigation data used to generate this path has been changed/invalidated */
	uint32 bUpToDate : 1;

	/** when false it means path instance has been created, but not filled with data yet */
	uint32 bIsReady : 1;

	/** "true" when path is only partially generated, when goal is unreachable and path represent best guess */
	uint32 bIsPartial : 1;

	/** set to true when path finding algorithm reached a technical limit (like limit of A* nodes).
	*	This generally means path cannot be trusted to lead to requested destination
	*	although it might lead closer to destination. */
	uint32 bReachedSearchLimit : 1;

	/** if true path will request re-pathing if it gets invalidated due to underlying navigation changed */
	uint32 bDoAutoUpdateOnInvalidation : 1;

	/** if true path will keep bUpToDate value after getting invalidated due to underlying navigation changed
	 *  (observer and auto repath will NOT be triggered!)
	 *  it's NOT safe to use if path relies on navigation data references (e.g. poly corridor)
	 */
	uint32 bIgnoreInvalidation : 1;

	/** if true path will use GetPathFindingStartLocation() for updating QueryData before repath */
	uint32 bUpdateStartPointOnRepath : 1;

	/** if true path will use GetGoalLocation() for updating QueryData before repath */
	uint32 bUpdateEndPointOnRepath : 1;

	/** set when path is waiting for recalc from navigation data */
	uint32 bWaitingForRepath : 1;

	/** if true path will call OnPathUpdated notify */
	uint32 bUseOnPathUpdatedNotify : 1;

	/** navigation data used to generate this path */
	TWeakObjectPtr<ANavigationData> NavigationDataUsed;

	/** essential part of query used to generate this path */
	FPathFindingQueryData PathFindingQueryData;

	/** gets set during path creation and on subsequent path's updates */
	float LastUpdateTimeStamp;

private:
	/* if GoalActor is set this is the distance we'll try to keep GoalActor from end of path. If GoalActor
	* moves more then this from the end of the path we'll recalculate the path */
	float GoalActorLocationTetherDistanceSq;

	/** last location of goal actor that was used for repaths to prevent spamming when path is partial */
	FVector GoalActorLastLocation;
};

/** 
 *  Supported options for runtime navigation data generation
 */
UENUM()
enum class ERuntimeGenerationType : uint8
{
	// No runtime generation, fully static navigation data
	Static,				
	// Supports only navigation modifiers updates
	DynamicModifiersOnly,	
	// Fully dynamic, supports geometry changes along with navigation modifiers
	Dynamic,
	// Only for legacy loading don't use it!
	LegacyGeneration UMETA(Hidden)
};

/** 
 *	Represents abstract Navigation Data (sub-classed as NavMesh, NavGraph, etc)
 *	Used as a common interface for all navigation types handled by NavigationSystem
 */
UCLASS(config=Engine, defaultconfig, NotBlueprintable, abstract)
class ENGINE_API ANavigationData : public AActor
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(transient, duplicatetransient)
	UPrimitiveComponent* RenderingComp;

protected:
	UPROPERTY()
	FNavDataConfig NavDataConfig;

	/** if set to true then this navigation data will be drawing itself when requested as part of "show navigation" */
	UPROPERTY(Transient, EditAnywhere, Category=Display)
	uint32 bEnableDrawing:1;

	//----------------------------------------------------------------------//
	// game-time config
	//----------------------------------------------------------------------//
	
	/** By default navigation will skip the first update after being successfully loaded
	*  setting bForceRebuildOnLoad to false can override this behavior */
	UPROPERTY(config, EditAnywhere, Category = Runtime)
	uint32 bForceRebuildOnLoad : 1;

	/** If set, navigation data can act as default one in navigation system's queries */
	UPROPERTY(config, VisibleAnywhere, Category = Runtime, AdvancedDisplay)
	uint32 bCanBeMainNavData : 1;

	/** If set, navigation data will be spawned in persistent level during rebuild if actor doesn't exist */
	UPROPERTY(config, VisibleAnywhere, Category = Runtime, AdvancedDisplay)
	uint32 bCanSpawnOnRebuild : 1;

	/** If true, the NavMesh can be dynamically rebuilt at runtime. */
	UPROPERTY(config)
	uint32 bRebuildAtRuntime_DEPRECATED:1;

	/** Navigation data runtime generation options */
	UPROPERTY(EditAnywhere, Category = Runtime, config)
	ERuntimeGenerationType RuntimeGeneration;

	/** all observed paths will be processed every ObservedPathsTickInterval seconds */
	UPROPERTY(EditAnywhere, Category = Runtime, config)
	float ObservedPathsTickInterval;

public:
	//----------------------------------------------------------------------//
	// Life cycle                                                                
	//----------------------------------------------------------------------//

	//~ Begin UObject/AActor Interface
	virtual void PostInitProperties() override;
	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UObject Interface
		
	virtual void CleanUp();

private:
	/** Simply unregisters self from navigation system and calls CleanUp */
	void UnregisterAndCleanUp();

public:
	virtual void CleanUpAndMarkPendingKill();

	FORCEINLINE bool IsRegistered() const { return bRegistered; }
	void OnRegistered();
	void OnUnregistered();
	
	FORCEINLINE uint16 GetNavDataUniqueID() const { return NavDataUniqueID; }

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void RerunConstructionScripts() override;

	virtual bool NeedsRebuild() const { return false; }
	virtual bool SupportsRuntimeGeneration() const;
	virtual bool SupportsStreaming() const;
	virtual void OnNavigationBoundsChanged();
	virtual void OnStreamingLevelAdded(ULevel* InLevel, UWorld* InWorld) {};
	virtual void OnStreamingLevelRemoved(ULevel* InLevel, UWorld* InWorld) {};
	
	//----------------------------------------------------------------------//
	// Generation & data access                                                      
	//----------------------------------------------------------------------//
public:
	FORCEINLINE const FNavDataConfig& GetConfig() const { return NavDataConfig; }
	FORCEINLINE ERuntimeGenerationType GetRuntimeGenerationMode() const { return RuntimeGeneration; }
	virtual void SetConfig(const FNavDataConfig& Src) { NavDataConfig = Src; }

	void SetSupportsDefaultAgent(bool bIsDefault) { bSupportsDefaultAgent = bIsDefault; SetNavRenderingEnabled(bIsDefault); }
	bool IsSupportingDefaultAgent() const { return bSupportsDefaultAgent; }

	virtual bool DoesSupportAgent(const FNavAgentProperties& AgentProps) const;

	virtual void RestrictBuildingToActiveTiles(bool InRestrictBuildingToActiveTiles) {}

	bool CanBeMainNavData() const { return bCanBeMainNavData; }
	bool CanSpawnOnRebuild() const { return bCanSpawnOnRebuild; }
	bool NeedsRebuildOnLoad() const { return bForceRebuildOnLoad; }

protected:
	virtual void FillConfig(FNavDataConfig& Dest) { Dest = NavDataConfig; }

public:
	/** Creates new generator in case navigation supports it */
	virtual void ConditionalConstructGenerator();
	
	/** Triggers rebuild in case navigation supports it */
	virtual void RebuildAll();

	/** Blocks until navigation build is complete  */
	virtual void EnsureBuildCompletion();

	/** Cancels current build  */
	virtual void CancelBuild();

	/** Ticks navigation build  */
	virtual void TickAsyncBuild(float DeltaSeconds);
	
	/** Retrieves navmesh's generator */
	FNavDataGenerator* GetGenerator() { return NavDataGenerator.Get(); }
	const FNavDataGenerator* GetGenerator() const { return NavDataGenerator.Get(); }

	/** Request navigation data update after changes in nav octree */
	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas);
	
	/** releases navigation generator if any has been created */
protected:
	/** register self with navigation system as new NavAreaDefinition(s) observer */
	void RegisterAsNavAreaClassObserver();

public:
	/** 
	 *	Created an instance of navigation path of specified type.
	 *	PathType needs to derive from FNavigationPath 
	 */
	template<typename PathType> 
	DEPRECATED(4.12, "This version of CreatePathInstance was deprecated, please use the one taking FPathFindingQueryData argument.")
	FNavPathSharedPtr CreatePathInstance(const UObject* Querier = NULL) const
	{
		FNavPathSharedPtr SharedPath = MakeShareable(new PathType());
		SharedPath->SetNavigationDataUsed(this);
		SharedPath->SetQueryData(FPathFindingQueryData(Querier, FNavigationSystem::InvalidLocation, FNavigationSystem::InvalidLocation));
		SharedPath->SetTimeStamp(GetWorldTimeStamp());

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Adding a path to ActivePaths"),
		STAT_FSimpleDelegateGraphTask_AddingPathToActivePaths,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &ANavigationData::RegisterActivePath, SharedPath),
			GET_STATID(STAT_FSimpleDelegateGraphTask_AddingPathToActivePaths), NULL, ENamedThreads::GameThread
			);

		return SharedPath;
	}

	template<typename PathType>
	FNavPathSharedPtr CreatePathInstance(const FPathFindingQueryData& QueryData) const
	{
		FNavPathSharedPtr SharedPath = MakeShareable(new PathType());
		SharedPath->SetNavigationDataUsed(this);
		SharedPath->SetQueryData(QueryData);
		SharedPath->SetTimeStamp( GetWorldTimeStamp() );

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Adding a path to ActivePaths"),
			STAT_FSimpleDelegateGraphTask_AddingPathToActivePaths,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &ANavigationData::RegisterActivePath, SharedPath),
			GET_STATID(STAT_FSimpleDelegateGraphTask_AddingPathToActivePaths), NULL, ENamedThreads::GameThread
		);

		return SharedPath;
	}
	
	void RegisterObservedPath(FNavPathSharedPtr SharedPath)
	{
		check(IsInGameThread());
		if (ObservedPaths.Num() == 0)
		{
			NextObservedPathsTickInSeconds = ObservedPathsTickInterval;
		}
		ObservedPaths.Add(SharedPath);
	}

	void RequestRePath(FNavPathSharedPtr Path, ENavPathUpdateType::Type Reason) { RepathRequests.AddUnique(FNavPathRecalculationRequest(Path, Reason)); }

protected:
	/** removes from ActivePaths all paths that no longer have shared references (and are invalid in fact) */
	void PurgeUnusedPaths();

	void RegisterActivePath(FNavPathSharedPtr SharedPath)
	{
		check(IsInGameThread());
		ActivePaths.Add(SharedPath);
	}

public:
	/** Returns bounding box for the navmesh. */
	virtual FBox GetBounds() const PURE_VIRTUAL(ANavigationData::GetBounds,return FBox(););
	
	/** Returns list of navigable bounds. */
	TArray<FBox> GetNavigableBounds() const;
	
	/** Returns list of navigable bounds that belongs to specific level */
	TArray<FBox> GetNavigableBoundsInLevel(ULevel* InLevel) const;
	
	//----------------------------------------------------------------------//
	// Debug                                                                
	//----------------------------------------------------------------------//
	void DrawDebugPath(FNavigationPath* Path, FColor PathColor = FColor::White, class UCanvas* Canvas = NULL, bool bPersistent = true, const uint32 NextPathPointIndex = 0) const;

	FORCEINLINE bool IsDrawingEnabled() const { return bEnableDrawing; }

	/** @return Total mem counted, including super calls. */
	virtual uint32 LogMemUsed() const;

	//----------------------------------------------------------------------//
	// Batch processing (important with async rebuilding)
	//----------------------------------------------------------------------//

	/** Starts batch processing and locks access to navigation data from other threads */
	virtual void BeginBatchQuery() const {}

	/** Finishes batch processing and release locks */
	virtual void FinishBatchQuery() const {};

	//----------------------------------------------------------------------//
	// Querying                                                                
	//----------------------------------------------------------------------//
	FORCEINLINE FSharedConstNavQueryFilter GetDefaultQueryFilter() const { return DefaultQueryFilter; }
	FORCEINLINE const class INavigationQueryFilterInterface* GetDefaultQueryFilterImpl() const { return DefaultQueryFilter->GetImplementation(); }	
	FORCEINLINE FVector GetDefaultQueryExtent() const { return NavDataConfig.DefaultQueryExtent; }

	/** 
	 *	Synchronously looks for a path from @StartLocation to @EndLocation for agent with properties @AgentProperties. NavMesh actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@return true if path has been found, false otherwise
	 *
	 *	@note don't make this function virtual! Look at implementation details and its comments for more info.
	 */
	FORCEINLINE FPathFindingResult FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query) const
	{
		check(FindPathImplementation);
		// this awkward implementation avoids virtual call overhead - it's possible this function will be called a lot
		return (*FindPathImplementation)(AgentProperties, Query);
	}

	/** 
	 *	Synchronously looks for a path from @StartLocation to @EndLocation for agent with properties @AgentProperties. NavMesh actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@return true if path has been found, false otherwise
	 *
	 *	@note don't make this function virtual! Look at implementation details and its comments for more info.
	 */
	FORCEINLINE FPathFindingResult FindHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query) const
	{
		check(FindHierarchicalPathImplementation);
		// this awkward implementation avoids virtual call overhead - it's possible this function will be called a lot
		return (*FindHierarchicalPathImplementation)(AgentProperties, Query);
	}

	/** 
	 *	Synchronously checks if path between two points exists
	 *	FNavAgentProperties will be found automatically
	 *	@return true if path has been found, false otherwise
	 *
	 *	@note don't make this function virtual! Look at implementation details and its comments for more info.
	 */
	FORCEINLINE bool TestPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes) const
	{
		check(TestPathImplementation);
		// this awkward implementation avoids virtual call overhead - it's possible this function will be called a lot
		return (*TestPathImplementation)(AgentProperties, Query, NumVisitedNodes);
	}

	/** 
	 *	Synchronously checks if path between two points exists using hierarchical graph
	 *	FNavAgentProperties will be found automatically
	 *	@return true if path has been found, false otherwise
	 *
	 *	@note don't make this function virtual! Look at implementation details and its comments for more info.
	 */
	FORCEINLINE bool TestHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes) const
	{
		check(TestHierarchicalPathImplementation);
		// this awkward implementation avoids virtual call overhead - it's possible this function will be called a lot
		return (*TestHierarchicalPathImplementation)(AgentProperties, Query, NumVisitedNodes);
	}

	/** 
	 *	Synchronously makes a raycast on navigation data using QueryFilter
	 *	@param HitLocation if line was obstructed this will be set to hit location. Otherwise it contains SegmentEnd
	 *	@return true if line from RayStart to RayEnd is obstructed
	 *
	 *	@note don't make this function virtual! Look at implementation details and its comments for more info.
	 */
	FORCEINLINE bool Raycast(const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL) const
	{
		check(RaycastImplementation);
		// this awkward implementation avoids virtual call overhead - it's possible this function will be called a lot
		return (*RaycastImplementation)(this, RayStart, RayEnd, HitLocation, QueryFilter, Querier);
	}

	/** Raycasts batched for efficiency */
	virtual void BatchRaycast(TArray<FNavigationRaycastWork>& Workload, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::BatchRaycast, );

	virtual FNavLocation GetRandomPoint(FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::GetRandomPoint, return FNavLocation(););

	/** finds a random location in Radius, reachable from Origin */
	virtual bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::GetRandomReachablePointInRadius, return false;);

	/** finds a random location in navigable space, in given Radius */
	virtual bool GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::GetRandomPointInNavigableRadius, return false;);
	
	/**	Tries to project given Point to this navigation type, within given Extent.
	 *	@param OutLocation if successful this variable will be filed with result
	 *	@return true if successful, false otherwise
	 */
	virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::ProjectPoint, return false;);

	/**	batches ProjectPoint's work for efficiency */
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::BatchProjectPoints, );

	/** Project batch of points using shared search filter. This version is not requiring user to pass in Extent, 
	 *	and is instead relying on FNavigationProjectionWork.ProjectionLimit.
	 *	@note function should assert if item's FNavigationProjectionWork.ProjectionLimit is invalid */
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::BatchProjectPoints, );

	/** Calculates path from PathStart to PathEnd and retrieves its cost. 
	 *	@NOTE this function does not generate string pulled path so the result is an (over-estimated) approximation
	 *	@NOTE potentially expensive, so use it with caution */
	virtual ENavigationQueryResult::Type CalcPathCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::CalcPathCost, return ENavigationQueryResult::Invalid;);

	/** Calculates path from PathStart to PathEnd and retrieves its length.
	 *	@NOTE this function does not generate string pulled path so the result is an (over-estimated) approximation
	 *	@NOTE potentially expensive, so use it with caution */
	virtual ENavigationQueryResult::Type CalcPathLength(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::CalcPathLength, return ENavigationQueryResult::Invalid;);

	/** Calculates path from PathStart to PathEnd and retrieves its length.
	 *	@NOTE this function does not generate string pulled path so the result is an (over-estimated) approximation
	 *	@NOTE potentially expensive, so use it with caution */
	virtual ENavigationQueryResult::Type CalcPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, float& OutPathCost, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::CalcPathLength, return ENavigationQueryResult::Invalid;);

	/** Checks if specified navigation node contains given location 
	 *	@param Location is expressed in WorldSpace, navigation data is responsible for tansforming if need be */
	virtual bool DoesNodeContainLocation(NavNodeRef NodeRef, const FVector& WorldSpaceLocation) const PURE_VIRTUAL(ANavigationData::DoesNodeContainLocation, return false;);

	float GetWorldTimeStamp() const;

	//----------------------------------------------------------------------//
	// Areas
	//----------------------------------------------------------------------//

	/** new area was registered in navigation system */
	virtual void OnNavAreaAdded(const UClass* NavAreaClass, int32 AgentIndex);
	
	/** area was removed from navigation system */
	virtual void OnNavAreaRemoved(const UClass* NavAreaClass);

	/** called after changes to registered area classes */
	virtual void OnNavAreaChanged();

	void OnNavAreaEvent(const UClass* NavAreaClass, ENavAreaEvent::Type Event);

	/** add all existing areas */
	void ProcessNavAreas(const TSet<const UClass*>& AreaClasses, int32 AgentIndex);

	/** get class associated with AreaID */
	const UClass* GetAreaClass(int32 AreaID) const;
	
	/** check if AreaID was assigned to class (class itself may not be loaded yet!) */
	bool IsAreaAssigned(int32 AreaID) const;

	/** get ID assigned to AreaClas or -1 when not assigned */
	int32 GetAreaID(const UClass* AreaClass) const;

	/** get max areas supported by this navigation data */
	virtual int32 GetMaxSupportedAreas() const { return MAX_int32; }

	/** read all supported areas */
	void GetSupportedAreas(TArray<FSupportedAreaData>& Areas) const { Areas = SupportedAreas; }

	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//

	virtual void UpdateCustomLink(const INavLinkCustomInterface* CustomLink);

	//----------------------------------------------------------------------//
	// Filters
	//----------------------------------------------------------------------//

	/** get cached query filter */
	FSharedConstNavQueryFilter GetQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass) const;

	/** store cached query filter */
	void StoreQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass, FSharedConstNavQueryFilter NavFilter);

	/** removes cached query filter */
	void RemoveQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass);

	//----------------------------------------------------------------------//
	// all the rest                                                                
	//----------------------------------------------------------------------//
	virtual UPrimitiveComponent* ConstructRenderingComponent() { return NULL; }

	/** updates state of rendering component */
	void SetNavRenderingEnabled(bool bEnable);

protected:
	void InstantiateAndRegisterRenderingComponent();

	/** get ID to assign for newly added area */
	virtual int32 GetNewAreaID(const UClass* AreaClass) const;
	
protected:
	/** Navigation data versioning. */
	UPROPERTY()
	uint32 DataVersion;

	typedef FPathFindingResult (*FFindPathPtr)(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query);
	FFindPathPtr FindPathImplementation;
	FFindPathPtr FindHierarchicalPathImplementation; 
	
	typedef bool (*FTestPathPtr)(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes);
	FTestPathPtr TestPathImplementation;
	FTestPathPtr TestHierarchicalPathImplementation; 

	typedef bool(*FNavRaycastPtr)(const ANavigationData* NavDataInstance, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier);
	FNavRaycastPtr RaycastImplementation; 

protected:
	TSharedPtr<FNavDataGenerator, ESPMode::ThreadSafe> NavDataGenerator;
	/** 
	 *	Container for all path objects generated with this Navigation Data instance. 
	 *	Is meant to be added to only on GameThread, and in fact should user should never 
	 *	add items to it manually, @see CreatePathInstance
	 */
	TArray<FNavPathWeakPtr> ActivePaths;

	/**
	 *	Contains paths that requested observing its goal's location. These paths will be 
	 *	processed on a regular basis (@see ObservedPathsTickInterval) */
	TArray<FNavPathWeakPtr> ObservedPaths;

	/** paths that requested re-calculation */
	TArray<FNavPathRecalculationRequest> RepathRequests;

	/** contains how much time left to the next ObservedPaths processing */
	float NextObservedPathsTickInSeconds;

	/** Query filter used when no other has been passed to relevant functions */
	FSharedNavQueryFilter DefaultQueryFilter;

	/** Map of query filters by UNavigationQueryFilter class */
	TMap<UClass*,FSharedConstNavQueryFilter > QueryFilters;

	/** serialized area class - ID mapping */
	UPROPERTY()
	TArray<FSupportedAreaData> SupportedAreas;

	/** mapping for SupportedAreas */
	TMap<const UClass*, int32> AreaClassToIdMap;

	/** whether this instance is registered with Navigation System*/
	uint32 bRegistered : 1;

	/** was it generated for default agent (SupportedAgents[0]) */
	uint32 bSupportsDefaultAgent : 1;
	
	DEPRECATED(4.12, "This flag is now deprecated, initial rebuild ignore should be handled by discarding dirty areas in UNavigationSystem::ConditionalPopulateNavOctree.")
	uint32 bWantsUpdate:1;

private:
	uint16 NavDataUniqueID;

	static uint16 GetNextUniqueID();

public:
	DEPRECATED(4.12, "This function is now deprecated, initial rebuild ignore should be handled by discarding dirty areas in UNavigationSystem::ConditionalPopulateNavOctree.")
	void MarkAsNeedingUpdate() { }
};

struct FAsyncPathFindingQuery : public FPathFindingQuery
{
	const uint32 QueryID;
	const FNavPathQueryDelegate OnDoneDelegate;
	const TEnumAsByte<EPathFindingMode::Type> Mode;
	FPathFindingResult Result;

	FAsyncPathFindingQuery()
		: QueryID(INVALID_NAVQUERYID)
	{ }

	FAsyncPathFindingQuery(const UObject* InOwner, const ANavigationData& InNavData, const FVector& Start, const FVector& End, const FNavPathQueryDelegate& Delegate, FSharedConstNavQueryFilter SourceQueryFilter);
	FAsyncPathFindingQuery(const FPathFindingQuery& Query, const FNavPathQueryDelegate& Delegate, const EPathFindingMode::Type QueryMode);

protected:
	FORCEINLINE static uint32 GetUniqueID()
	{
		return ++LastPathFindingUniqueID;
	}

	static uint32 LastPathFindingUniqueID;
};

FORCEINLINE bool FPathFindingResult::IsPartial() const
{
	return (Result != ENavigationQueryResult::Error) && Path.IsValid() && Path->IsPartial();
}

FORCEINLINE void FNavigationPath::SetNavigationDataUsed(const ANavigationData* const NavData)
{
	NavigationDataUsed = NavData;
}
