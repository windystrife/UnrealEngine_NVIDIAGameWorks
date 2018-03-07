// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/CoreMisc.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "GenericOctreePublic.h"
#include "AI/Navigation/NavigationData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NavigationSystem.generated.h"

#define NAVSYS_DEBUG (0 && UE_BUILD_DEBUG)

#define NAV_USE_MAIN_NAVIGATION_DATA NULL

class AController;
class ANavMeshBoundsVolume;
class AWorldSettings;
class FEdMode;
class FNavDataGenerator;
class FNavigationOctree;
class INavLinkCustomInterface;
class INavRelevantInterface;
class UCrowdManagerBase;
class UNavArea;
class UNavigationPath;
struct FNavigationRelevantData;

#if WITH_EDITOR
class FEdMode;
#endif // WITH_EDITOR

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavigation, Warning, All);

/** delegate to let interested parties know that new nav area class has been registered */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavAreaChanged, const UClass* /*AreaClass*/);

/** Delegate to let interested parties know that Nav Data has been registered */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavDataGenerigEvent, ANavigationData*, NavData);

DECLARE_MULTICAST_DELEGATE(FOnNavigationInitDone);

namespace NavigationDebugDrawing
{
	extern const ENGINE_API float PathLineThickness;
	extern const ENGINE_API FVector PathOffset;
	extern const ENGINE_API FVector PathNodeBoxExtent;
}

UENUM()
enum class FNavigationSystemRunMode : uint8
{
	InvalidMode,
	GameMode,
	EditorMode,
	SimulationMode,
	PIEMode,
};

namespace FNavigationSystem
{	
	/** 
	 * Used to construct an ANavigationData instance for specified navigation data agent 
	 */
	typedef ANavigationData* (*FNavigationDataInstanceCreator)(UWorld*, const FNavDataConfig&);

	struct ENGINE_API FCustomLinkOwnerInfo
	{
		FWeakObjectPtr LinkOwner;
		INavLinkCustomInterface* LinkInterface;

		FCustomLinkOwnerInfo() : LinkInterface(nullptr) {}
		FCustomLinkOwnerInfo(INavLinkCustomInterface* Link);

		bool IsValid() const { return LinkOwner.IsValid(); }
	};
}

struct FNavigationSystemExec: public FSelfRegisteringExec
{
	//~ Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End FExec Interface
};

namespace ENavigationBuildLock
{
	enum Type
	{
		NoUpdateInEditor = 1 << 1,		// editor doesn't allow automatic updates
		InitialLock = 1 << 2,			// initial lock, release manually after levels are ready for rebuild (e.g. streaming)
		Custom = 1 << 3,
	};
}

namespace ENavigationLockReason
{
	enum Type
	{
		Unknown					= 1 << 0,
		AllowUnregister			= 1 << 1,

		MaterialUpdate			= 1 << 2,
		LightingUpdate			= 1 << 3,
		ContinuousEditorMove	= 1 << 4,
		SpawnOnDragEnter		= 1 << 5,
	};
}

class ENGINE_API FNavigationLockContext
{
public:
	FNavigationLockContext(ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true) 
		: MyWorld(NULL), LockReason(Reason), bSingleWorld(false), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	FNavigationLockContext(UWorld* InWorld, ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(InWorld), LockReason(Reason), bSingleWorld(true), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	~FNavigationLockContext()
	{
		UnlockUpdates(); 
	}

private:
	UWorld* MyWorld;
	uint8 LockReason;
	uint8 bSingleWorld : 1;
	uint8 bIsLocked : 1;

	void LockUpdates();
	void UnlockUpdates();
};

UCLASS(Within=World, config=Engine, defaultconfig)
class ENGINE_API UNavigationSystem : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	virtual ~UNavigationSystem();

	UPROPERTY()
	ANavigationData* MainNavData;

	/** special navigation data for managing direct paths, not part of NavDataSet! */
	UPROPERTY(Transient)
	ANavigationData* AbstractNavData;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Navigation)
	TSubclassOf<UCrowdManagerBase> CrowdManagerClass;

	/** Should navigation system spawn default Navigation Data when there's none and there are navigation bounds present? */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAutoCreateNavigationData:1;

	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAllowClientSideNavigation:1;

	/** gets set to true if gathering navigation data (like in navoctree) is required due to the need of navigation generation 
	 *	Is always true in Editor Mode. In other modes it depends on bRebuildAtRuntime of every required NavigationData class' CDO
	 */
	UPROPERTY()
	uint32 bSupportRebuilding : 1; 

public:
	/** if set to true will result navigation system not rebuild navigation until 
	 *	a call to ReleaseInitialBuildingLock() is called. Does not influence 
	 *	editor-time generation (i.e. does influence PIE and Game).
	 *	Defaults to false.*/
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bInitialBuildingLocked:1;

	/** If set to true (default) navigation will be generated only within special navigation 
	 *	bounds volumes (like ANavMeshBoundsVolume). Set to false means navigation should be generated
	 *	everywhere.
	 */
	// @todo removing it from edition since it's currently broken and I'm not sure we want that at all
	// since I'm not sure we can make it efficient in a generic case
	//UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bWholeWorldNavigable:1;

	/** false by default, if set to true will result in not caring about nav agent height 
	 *	when trying to match navigation data to passed in nav agent */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bSkipAgentHeightCheckWhenPickingNavData:1;

protected:
	
	UPROPERTY(EditDefaultsOnly, Category = "NavigationSystem", config)
	ENavDataGatheringModeConfig DataGatheringMode;

	/** If set to true navigation will be generated only around registered "navigation enforcers"
	*	This has a range of consequences (including how navigation octree operates) so it needs to
	*	be a conscious decision.
	*	Once enabled results in whole world being navigable.
	*	@see RegisterNavigationInvoker
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Navigation Enforcing", config)
	uint32 bGenerateNavigationOnlyAroundNavigationInvokers : 1;
	
	/** Minimal time, in seconds, between active tiles set update */
	UPROPERTY(EditAnywhere, Category = "Navigation Enforcing", meta = (ClampMin = "0.1", UIMin = "0.1", EditCondition = "bGenerateNavigationOnlyAroundNavigationInvokers"), config)
	float ActiveTilesUpdateInterval;

	UPROPERTY(config, EditAnywhere, Category = Agents)
	TArray<FNavDataConfig> SupportedAgents;

public:
	/** update frequency for dirty areas on navmesh */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	float DirtyAreasUpdateFreq;

	UPROPERTY()
	TArray<ANavigationData*> NavDataSet;

	UPROPERTY(transient)
	TArray<ANavigationData*> NavDataRegistrationQueue;

	TSet<FNavigationDirtyElement> PendingOctreeUpdates;

	// List of pending navigation bounds update requests (add, remove, update size)
	TArray<FNavigationBoundsUpdateRequest> PendingNavBoundsUpdates;

 	UPROPERTY(/*BlueprintAssignable, */Transient)
	FOnNavDataGenerigEvent OnNavDataRegisteredEvent;

	UPROPERTY(BlueprintAssignable, Transient, meta = (displayname = OnNavigationGenerationFinished))
	FOnNavDataGenerigEvent OnNavigationGenerationFinishedDelegate;

	FOnNavigationInitDone OnNavigationInitDone;
	
private:
	TWeakObjectPtr<UCrowdManagerBase> CrowdManager;
	
	/** set to true when navigation processing was blocked due to missing nav bounds */
	uint32 bNavDataRemovedDueToMissingNavBounds : 1;
	
	/** All areas where we build/have navigation */
	TSet<FNavigationBounds> RegisteredNavBounds;

	TMap<AActor*, FNavigationInvoker> Invokers;

	float NextInvokersUpdateTime;
	void UpdateInvokers();

public:
	//----------------------------------------------------------------------//
	// Blueprint functions
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static UNavigationSystem* GetNavigationSystem(UObject* WorldContextObject);
	
	/** Project a point onto the NavigationData */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "ProjectPointToNavigation"))
	static bool K2_ProjectPointToNavigation(UObject* WorldContextObject, const FVector& Point, FVector& ProjectedLocation, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass, const FVector QueryExtent = FVector::ZeroVector);

	/** Generates a random location reachable from given Origin location.
	 *	@return Return Value represents if the call was successful */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomReachablePointInRadius"))
	static bool K2_GetRandomReachablePointInRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Generates a random location in navigable space within given radius of Origin.
	 *	@return Return Value represents if the call was successful */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomPointInNavigableRadius"))
	static bool K2_GetRandomPointInNavigableRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	
	/** Potentially expensive. Use with caution. Consider using UPathFollowingComponent::GetRemainingPathCost instead */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static ENavigationQueryResult::Type GetPathCost(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& PathCost, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Potentially expensive. Use with caution */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static ENavigationQueryResult::Type GetPathLength(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& PathLength, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static bool IsNavigationBeingBuilt(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool IsNavigationBeingBuiltOrLocked(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	static void SimpleMoveToActor(AController* Controller, const AActor* Goal);

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	static void SimpleMoveToLocation(AController* Controller, const FVector& Goal);

	/** Finds path instantly, in a FindPath Synchronously. 
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext="WorldContextObject"))
	static UNavigationPath* FindPathToLocationSynchronously(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Finds path instantly, in a FindPath Synchronously. Main advantage over FindPathToLocationSynchronously is that 
	 *	the resulting path will automatically get updated if goal actor moves more than TetherDistance away from last path node
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext="WorldContextObject"))
	static UNavigationPath* FindPathToActorSynchronously(UObject* WorldContextObject, const FVector& PathStart, AActor* GoalActor, float TetherDistance = 50.f, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Performs navigation raycast on NavigationData appropriate for given Querier.
	 *	@param Querier if not passed default navigation data will be used
	 *	@param HitLocation if line was obstructed this will be set to hit location. Otherwise it contains SegmentEnd
	 *	@return true if line from RayStart to RayEnd was obstructed. Also, true when no navigation data present */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ))
	static bool NavigationRaycast(UObject* WorldContextObject, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, AController* Querier = NULL);

	/** will limit the number of simultaneously running navmesh tile generation jobs to specified number.
	 *	@param MaxNumberOfJobs gets trimmed to be at least 1. You cannot use this function to pause navmesh generation */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetMaxSimultaneousTileGenerationJobsCount(int32 MaxNumberOfJobs);
	
	/** Brings limit of simultaneous navmesh tile generation jobs back to Project Setting's default value */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void ResetMaxSimultaneousTileGenerationJobsCount();

	/** Registers given actor as a "navigation enforcer" which means navigation system will
	 *	make sure navigation is being generated in specified radius around it.
	 *	@note: you need NavigationSystem's GenerateNavigationOnlyAroundNavigationInvokers to be set to true
	 *		to take advantage of this feature
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void RegisterNavigationInvoker(AActor* Invoker, float TileGenerationRadius = 3000, float TileRemovalRadius = 5000);

	/** Removes given actor from the list of active navigation enforcers.
	 *	@see RegisterNavigationInvoker for more details */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void UnregisterNavigationInvoker(AActor* Invoker);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation|Generation")
	void SetGeometryGatheringMode(ENavDataGatheringModeConfig NewMode);

	FORCEINLINE bool IsActiveTilesGenerationEnabled() const{ return bGenerateNavigationOnlyAroundNavigationInvokers; }
	
	/** delegate type for events that dirty the navigation data ( Params: const FBox& DirtyBounds ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationDirty, const FBox&);
	/** called after navigation influencing event takes place*/
	static FOnNavigationDirty NavigationDirtyEvent;

	enum ERegistrationResult
	{
		RegistrationError,
		RegistrationFailed_DataPendingKill,			// means navigation data being registered is marked as pending kill
		RegistrationFailed_AgentAlreadySupported,	// this means that navigation agent supported by given nav data is already handled by some other, previously registered instance
		RegistrationFailed_AgentNotValid,			// given instance contains navmesh that doesn't support any of expected agent types, or instance doesn't specify any agent
		RegistrationSuccessful,
	};

	enum EOctreeUpdateMode
	{
		OctreeUpdate_Default = 0,						// regular update, mark dirty areas depending on exported content
		OctreeUpdate_Geometry = 1,						// full update, mark dirty areas for geometry rebuild
		OctreeUpdate_Modifiers = 2,						// quick update, mark dirty areas for modifier rebuild
		OctreeUpdate_Refresh = 4,						// update is used for refresh, don't invalidate pending queue
		OctreeUpdate_ParentChain = 8,					// update child nodes, don't remove anything
	};

	enum ECleanupMode
	{
		CleanupWithWorld,
		CleanupUnsafe,
	};

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	virtual void Tick(float DeltaSeconds);	

	UWorld* GetWorld() const override { return GetOuterUWorld(); }

	UCrowdManagerBase* GetCrowdManager() const { return CrowdManager.Get(); }

protected:
	/** spawn new crowd manager */
	virtual void CreateCrowdManager();

	/** Used to properly set navigation class for indicated agent and propagate information to other places
	 *	(like project settings) that may need this information 
	 */
	void SetSupportedAgentsNavigationClass(int32 AgentIndex, TSubclassOf<ANavigationData> NavigationDataClass);

public:
	//----------------------------------------------------------------------//
	//~ Begin Public querying Interface                                                                
	//----------------------------------------------------------------------//
	/** 
	 *	Synchronously looks for a path from @fLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Does a simple path finding from @StartLocation to @EndLocation on specified NavData. If none passed MainNavData will be used
	 *	Result gets placed in ResultPath
	 *	@param NavData optional navigation data that will be used instead main navigation data
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Asynchronously looks for a path from @StartLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultDelegate delegate that will be called once query has been processed and finished. Will be called even if query fails - in such case see comments for delegate's params
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *	@param PathToFill if points to an actual navigation path instance than this instance will be filled with resulting path. Otherwise a new instance will be created and 
	 *		used in call to ResultDelegate
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 *	@return request ID
	 */
	uint32 FindPathAsync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, const FNavPathQueryDelegate& ResultDelegate, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** Removes query indicated by given ID from queue of path finding requests to process. */
	void AbortAsyncFindPathRequest(uint32 AsynPathQueryID);
	
	/** 
	 *	Synchronously check if path between two points exists
	 *  Does not return path object, but will run faster (especially in hierarchical mode)
	 *  @param Mode switch between normal and hierarchical path finding algorithms. @note Hierarchical mode ignores QueryFilter
	 *	@return true if path exists
	 */
	bool TestPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular, int32* NumVisitedNodes = NULL) const;

	/** Finds random point in navigable space
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPoint(FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL);

	/** Finds random, reachable point in navigable space restricted to Radius around Origin
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Finds random, point in navigable space restricted to Radius around Origin. Resulting location is not tested for reachability from the Origin
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;
	
	/** Calculates a path from PathStart to PathEnd and retrieves its cost. 
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathCost(const FVector& PathStart, const FVector& PathEnd, float& PathCost, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLength(const FVector& PathStart, const FVector& PathEnd, float& PathLength, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length and cost.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& PathLength, float& PathCost, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const FNavAgentProperties* AgentProperties = NULL, FSharedConstNavQueryFilter QueryFilter = NULL)
	{
		return ProjectPointToNavigation(Point, OutLocation, Extent, AgentProperties != NULL ? GetNavDataForProps(*AgentProperties) : GetMainNavData(FNavigationSystem::DontCreate), QueryFilter);
	}

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found;
	 */
	ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties);

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found; Const version.
	 */
	const ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties) const;

	/** Returns the world nav mesh object.  Creates one if it doesn't exist. */
	ANavigationData* GetMainNavData(FNavigationSystem::ECreateIfEmpty CreateNewIfNoneFound);
	/** Returns the world nav mesh object.  Creates one if it doesn't exist. */
	const ANavigationData* GetMainNavData() const { return MainNavData; }
	const ANavigationData& GetMainNavDataChecked() const { check(MainNavData); return *MainNavData; }

	ANavigationData* GetAbstractNavData() const { return AbstractNavData; }

	/** constructs a navigation data instance of specified NavDataClass, in passed World
	*	for supplied NavConfig */
	virtual ANavigationData* CreateNavigationDataInstance(const FNavDataConfig& NavConfig);

	FSharedNavQueryFilter CreateDefaultQueryFilterCopy() const;

	/** Super-hacky safety feature for threaded navmesh building. Will be gone once figure out why keeping TSharedPointer to Navigation Generator doesn't 
	 *	guarantee its existence */
	bool ShouldGeneratorRun(const FNavDataGenerator* Generator) const;

	bool IsNavigationBuilt(const AWorldSettings* Settings) const;

	bool IsThereAnywhereToBuildNavigation() const;

	bool ShouldGenerateNavigationEverywhere() const { return bWholeWorldNavigable; }

	bool ShouldAllowClientSideNavigation() const { return bAllowClientSideNavigation; }
	virtual bool ShouldLoadNavigationOnClient(ANavigationData* NavData = nullptr) const { return bAllowClientSideNavigation; }

	FBox GetWorldBounds() const;
	
	FBox GetLevelBounds(ULevel* InLevel) const;

	bool IsNavigationRelevant(const AActor* TestActor) const;

	const TSet<FNavigationBounds>& GetNavigationBounds() const;

	/** @return default walkable area class */
	FORCEINLINE static TSubclassOf<UNavArea> GetDefaultWalkableArea() { return DefaultWalkableArea; }

	/** @return default obstacle area class */
	FORCEINLINE static TSubclassOf<UNavArea> GetDefaultObstacleArea() { return DefaultObstacleArea; }

	FORCEINLINE const FNavDataConfig& GetDefaultSupportedAgentConfig() const { check(SupportedAgents.Num() > 0);  return SupportedAgents[0]; }
	FORCEINLINE const TArray<FNavDataConfig>& GetSupportedAgents() const { return SupportedAgents; }

	void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** checks if navigation/navmesh is dirty and needs to be rebuilt */
	bool IsNavigationDirty() const;

	/** checks if dirty navigation data can rebuild itself */
	bool CanRebuildDirtyNavigation() const;

	FORCEINLINE bool SupportsNavigationGeneration() const { return bSupportRebuilding; }

	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, uint32 StartingIndex = 0, FVector* AgentExtent = NULL);
	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0, FVector* AgentExtent = NULL);

	//----------------------------------------------------------------------//
	// Active tiles
	//----------------------------------------------------------------------//
	void RegisterInvoker(AActor& Invoker, float TileGenerationRadius, float TileRemovalRadius);
	void UnregisterInvoker(AActor& Invoker);

	static void RegisterNavigationInvoker(AActor& Invoker, float TileGenerationRadius, float TileRemovalRadius);
	static void UnregisterNavigationInvoker(AActor& Invoker);

	//----------------------------------------------------------------------//
	// Bookkeeping 
	//----------------------------------------------------------------------//
	
	// @todo document
	void UnregisterNavData(ANavigationData* NavData);

	/** adds NavData to registration candidates queue - NavDataRegistrationQueue */
	void RequestRegistration(ANavigationData* NavData, bool bTriggerRegistrationProcessing = true);

protected:

	/** Processes all NavigationData instances in UWorld owning navigation system instance, and registers
	 *	all previously unregistered */
	void RegisterNavigationDataInstances();

	/** called in places where we need to spawn the NavOctree, but is checking additional conditions if we really want to do that
	 *	depending on navigation data setup among others 
	 *	@return true if NavOctree instance has been created, or if one is already present */
	bool ConditionalPopulateNavOctree();

	/** Processes registration of candidates queues via RequestRegistration and stored in NavDataRegistrationQueue */
	void ProcessRegistrationCandidates();

	/** registers CustomLinks awaiting registration in PendingCustomLinkRegistration */
	void ProcessCustomLinkPendingRegistration();

	/** used to apply updates of nav volumes in navigation system's tick */
	void PerformNavigationBoundsUpdate(const TArray<FNavigationBoundsUpdateRequest>& UpdateRequests);
	
	/** adds data to RegisteredNavBounds */
	void AddNavigationBounds(const FNavigationBounds& NewBounds);

	/** Searches for all valid navigation bounds in the world and stores them */
	virtual void GatherNavigationBounds();

	/** @return pointer to ANavigationData instance of given ID, or NULL if it was not found. Note it looks only through registered navigation data */
	ANavigationData* GetNavDataWithID(const uint16 NavDataID) const;

public:
	void ReleaseInitialBuildingLock();

	//----------------------------------------------------------------------//
	// navigation octree related functions
	//----------------------------------------------------------------------//
	static void OnComponentRegistered(UActorComponent* Comp);
	static void OnComponentUnregistered(UActorComponent* Comp);
	static void OnActorRegistered(AActor* Actor);
	static void OnActorUnregistered(AActor* Actor);

	/** update navoctree entry for specified actor/component */
	static void UpdateActorInNavOctree(AActor& Actor);
	static void UpdateComponentInNavOctree(UActorComponent& Comp);
	/** update all navoctree entries for actor and its components */
	static void UpdateActorAndComponentsInNavOctree(AActor& Actor, bool bUpdateAttachedActors = true);
	/** update all navoctree entries for actor and its non scene components after root movement */
	static void UpdateNavOctreeAfterMove(USceneComponent* Comp);

protected:
	/** updates navoctree information on actors attached to RootActor */
	static void UpdateAttachedActorsInNavOctree(AActor& RootActor);

public:
	/** removes all navoctree entries for actor and its components */
	static void ClearNavOctreeAll(AActor* Actor);
	/** updates bounds of all components implementing INavRelevantInterface */
	static void UpdateNavOctreeBounds(AActor* Actor);

	void AddDirtyArea(const FBox& NewArea, int32 Flags);
	void AddDirtyAreas(const TArray<FBox>& NewAreas, int32 Flags);
	bool HasDirtyAreasQueued() const;

	const FNavigationOctree* GetNavOctree() const { return NavOctree.Get(); }
	FNavigationOctree* GetMutableNavOctree() { return NavOctree.Get(); }

	FORCEINLINE void SetObjectsNavOctreeId(UObject* Object, FOctreeElementId Id) { ObjectToOctreeId.Add(Object, Id); }
	FORCEINLINE const FOctreeElementId* GetObjectsNavOctreeId(const UObject* Object) const { return ObjectToOctreeId.Find(Object); }
	FORCEINLINE bool HasPendingObjectNavOctreeId(UObject* Object) const { return PendingOctreeUpdates.Contains(FNavigationDirtyElement(Object)); }
	FORCEINLINE	void RemoveObjectsNavOctreeId(UObject* Object) { ObjectToOctreeId.Remove(Object); }
	void RemoveNavOctreeElementId(const FOctreeElementId& ElementId, int32 UpdateFlags);

	const FNavigationRelevantData* GetDataForObject(const UObject& Object) const;

	/** find all elements in navigation octree within given box (intersection) */
	void FindElementsInNavOctree(const FBox& QueryBox, const struct FNavigationOctreeFilter& Filter, TArray<struct FNavigationOctreeElement>& Elements);

	/** update single element in navoctree */
	void UpdateNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);

	/** force updating parent node and all its children */
	void UpdateNavOctreeParentChain(UObject* ElementOwner, bool bSkipElementOwnerUpdate = false);

	/** update component bounds in navigation octree and mark only specified area as dirty, doesn't re-export component geometry */
	bool UpdateNavOctreeElementBounds(UActorComponent* Comp, const FBox& NewBounds, const FBox& DirtyArea);

	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//
	void RegisterCustomLink(INavLinkCustomInterface& CustomLink);
	void UnregisterCustomLink(INavLinkCustomInterface& CustomLink);
	
	static void RequestCustomLinkRegistering(INavLinkCustomInterface& CustomLink, UObject* OwnerOb);
	static void RequestCustomLinkUnregistering(INavLinkCustomInterface& CustomLink, UObject* ObjectOb);

	/** find custom link by unique ID */
	INavLinkCustomInterface* GetCustomLink(uint32 UniqueLinkId) const;

	/** updates custom link for all active navigation data instances */
	void UpdateCustomLink(const INavLinkCustomInterface* CustomLink);

	//----------------------------------------------------------------------//
	// Areas
	//----------------------------------------------------------------------//
	static void RequestAreaRegistering(UClass* NavAreaClass);
	static void RequestAreaUnregistering(UClass* NavAreaClass);

	/** find index in SupportedAgents array for given navigation data */
	int32 GetSupportedAgentIndex(const ANavigationData* NavData) const;

	/** find index in SupportedAgents array for agent type */
	int32 GetSupportedAgentIndex(const FNavAgentProperties& NavAgent) const;

	//----------------------------------------------------------------------//
	// Filters
	//----------------------------------------------------------------------//
	
	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using enum */
	void DescribeFilterFlags(UEnum* FlagsEnum) const;

	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using array */
	void DescribeFilterFlags(const TArray<FString>& FlagsDesc) const;

	/** removes cached filters from currently registered navigation data */
	void ResetCachedFilter(TSubclassOf<UNavigationQueryFilter> FilterClass);

	//----------------------------------------------------------------------//
	// building
	//----------------------------------------------------------------------//
	
	/** Triggers navigation building on all eligible navigation data. */
	virtual void Build();

	// @todo document
	void OnPIEStart();
	// @todo document
	void OnPIEEnd();
	
	// @todo document
	FORCEINLINE bool IsNavigationBuildingLocked() const { return NavBuildingLockFlags != 0; }

	/** check if building is permanently locked to avoid showing navmesh building notify (due to queued dirty areas) */
	FORCEINLINE bool IsNavigationBuildingPermanentlyLocked() const
	{
		return (NavBuildingLockFlags & ~ENavigationBuildLock::InitialLock) != 0; 
	}

	/** check if navigation octree updates are currently ignored */
	FORCEINLINE bool IsNavigationOctreeLocked() const { return bNavOctreeLock; }

	// @todo document
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void OnNavigationBoundsUpdated(ANavMeshBoundsVolume* NavVolume);
	virtual void OnNavigationBoundsAdded(ANavMeshBoundsVolume* NavVolume);
	void OnNavigationBoundsRemoved(ANavMeshBoundsVolume* NavVolume);

	/** Used to display "navigation building in progress" notify */
	bool IsNavigationBuildInProgress(bool bCheckDirtyToo = true);

	virtual void OnNavigationGenerationFinished(ANavigationData& NavData);

	/** Used to display "navigation building in progress" counter */
	int32 GetNumRemainingBuildTasks() const;

	/** Number of currently running tasks */
	int32 GetNumRunningBuildTasks() const;

protected:
	/** Sets up SuportedAgents and NavigationDataCreators. Override it to add additional setup, but make sure to call Super implementation */
	virtual void DoInitialSetup();

	/** spawn new crowd manager */
	virtual void UpdateAbstractNavData();
	
public:
	/** Called upon UWorld destruction to release what needs to be released */
	void CleanUp(ECleanupMode Mode = ECleanupMode::CleanupUnsafe);

	/** 
	 *	Called when owner-UWorld initializes actors
	 */
	virtual void OnInitializeActors();

	/** */
	virtual void OnWorldInitDone(FNavigationSystemRunMode Mode);

	FORCEINLINE bool IsInitialized() const { return bWorldInitDone; }

	/** adds BSP collisions of currently streamed in levels to octree */
	void InitializeLevelCollisions();

	FORCEINLINE void AddNavigationBuildLock(uint8 Flags) { NavBuildingLockFlags |= Flags; }
	void RemoveNavigationBuildLock(uint8 Flags, bool bSkipRebuildInEditor = false);

	void SetNavigationOctreeLock(bool bLock) { bNavOctreeLock = bLock; }

#if WITH_EDITOR
	/** allow editor to toggle whether seamless navigation building is enabled */
	static void SetNavigationAutoUpdateEnabled(bool bNewEnable, UNavigationSystem* InNavigationSystem);

	/** check whether seamless navigation building is enabled*/
	FORCEINLINE static bool GetIsNavigationAutoUpdateEnabled() { return bNavigationAutoUpdateEnabled; }

	FORCEINLINE bool IsNavigationRegisterLocked() const { return NavUpdateLockFlags != 0; }
	FORCEINLINE bool IsNavigationUnregisterLocked() const { return NavUpdateLockFlags && !(NavUpdateLockFlags & ENavigationLockReason::AllowUnregister); }
	FORCEINLINE bool IsNavigationUpdateLocked() const { return IsNavigationRegisterLocked(); }
	FORCEINLINE void AddNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags |= Flags; }
	FORCEINLINE void RemoveNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags &= ~Flags; }

	void UpdateLevelCollision(ULevel* InLevel);

	virtual void OnEditorModeChanged(FEdMode* Mode, bool IsEntering);
#endif // WITH_EDITOR

	FORCEINLINE bool IsSetUpForLazyGeometryExporting() const { return bGenerateNavigationOnlyAroundNavigationInvokers; }

	static UNavigationSystem* CreateNavigationSystem(UWorld* WorldOwner);

	static UNavigationSystem* GetCurrent(UWorld* World);
	static UNavigationSystem* GetCurrent(UObject* WorldContextObject);
	
	/** try to create and setup navigation system */
	static void InitializeForWorld(UWorld* World, FNavigationSystemRunMode Mode);

	// Fetch the array of all nav-agent properties.
	void GetNavAgentPropertiesArray(TArray<FNavAgentProperties>& OutNavAgentProperties) const;

	static FORCEINLINE bool ShouldUpdateNavOctreeOnComponentChange()
	{
		return (bUpdateNavOctreeOnComponentChange && !bStaticRuntimeNavigation)
#if WITH_EDITOR
			|| (GIsEditor && !GIsPlayInEditorWorld)
#endif
			;
	}

	static FORCEINLINE bool IsNavigationSystemStatic()
	{
		return bStaticRuntimeNavigation
#if WITH_EDITOR
			&& !(GIsEditor && !GIsPlayInEditorWorld)
#endif
			;
	}

	/** Use this function to signal the NavigationSystem it doesn't need to store
	 *	any navigation-generation-related data at game runtime, because 
	 *	nothing is going to use it anyway. This will short-circuit all code related 
	 *	to navmesh rebuilding, so use it only if you have fully static navigation in 
	 *	your game.
	 *	Note: this is not a runtime switch. Call it before any actual game starts. */
	static void ConfigureAsStatic();

	static void SetUpdateNavOctreeOnComponentChange(bool bNewUpdateOnComponentChange);

	/** 
	 * Exec command handlers
	 */
	bool HandleCycleNavDrawnCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCountNavMemCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	
	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	void CycleNavigationDataDrawn();

protected:

	UPROPERTY()
	FNavigationSystemRunMode OperationMode;

	TSharedPtr<FNavigationOctree, ESPMode::ThreadSafe> NavOctree;

	TArray<FAsyncPathFindingQuery> AsyncPathFindingQueries;

	FCriticalSection NavDataRegistration;

	TMap<FNavAgentProperties, TWeakObjectPtr<ANavigationData> > AgentToNavDataMap;
	
	TMap<const UObject*, FOctreeElementId> ObjectToOctreeId;

	/** Map of all objects that are tied to indexed navigation parent */
	TMultiMap<UObject*, FWeakObjectPtr> OctreeChildNodesMap;

	/** Map of all custom navigation links, that are relevant for path following */
	TMap<uint32, FNavigationSystem::FCustomLinkOwnerInfo> CustomLinksMap;

	/** stores areas marked as dirty throughout the frame, processes them 
	 *	once a frame in Tick function */
	TArray<FNavigationDirtyArea> DirtyAreas;

	// async queries
	FCriticalSection NavDataRegistrationSection;
	static FCriticalSection CustomLinkRegistrationSection;
	
#if WITH_EDITOR
	uint8 NavUpdateLockFlags;
#endif
	uint8 NavBuildingLockFlags;

	/** set of locking flags applied on startup of navigation system */
	uint8 InitialNavBuildingLockFlags;

	/** if set, navoctree updates are ignored, use with caution! */
	uint8 bNavOctreeLock : 1;

	uint8 bInitialSetupHasBeenPerformed : 1;
	uint8 bInitialLevelsAdded : 1;
	uint8 bWorldInitDone : 1;
	uint8 bAsyncBuildPaused : 1;
	uint8 bCanAccumulateDirtyAreas : 1;

	/** cached navigable world bounding box*/
	mutable FBox NavigableWorldBounds;

	/** indicates which of multiple navigation data instances to draw*/
	int32 CurrentlyDrawnNavDataIndex;

	/** temporary cumulative time to calculate when we need to update dirty areas */
	float DirtyAreasUpdateTime;
	
#if !UE_BUILD_SHIPPING
	/** self-registering exec command to handle nav sys console commands */
	static FNavigationSystemExec ExecHandler;
#endif // !UE_BUILD_SHIPPING
	
	/** whether seamless navigation building is enabled */
	static bool bNavigationAutoUpdateEnabled;
	static bool bUpdateNavOctreeOnComponentChange;
	static bool bStaticRuntimeNavigation;

	static TMap<INavLinkCustomInterface*, FWeakObjectPtr> PendingCustomLinkRegistration;
	TSet<const UClass*> NavAreaClasses;
	static TSubclassOf<UNavArea> DefaultWalkableArea;
	static TSubclassOf<UNavArea> DefaultObstacleArea;

	/** delegate handler for PostLoadMap event */
	void OnPostLoadMap(UWorld* LoadedWorld);
#if WITH_EDITOR
	/** delegate handler for ActorMoved events */
	void OnActorMoved(AActor* Actor);
#endif
	/** delegate handler called when navigation is dirtied*/
	void OnNavigationDirtied(const FBox& Bounds);

#if WITH_HOT_RELOAD
	FDelegateHandle HotReloadDelegateHandle;

	/** called to notify NavigaitonSystem about finished hot reload */
	virtual void OnHotReload(bool bWasTriggeredAutomatically);
#endif // WITH_HOT_RELOAD

	/** Registers given navigation data with this Navigation System.
	 *	@return RegistrationSuccessful if registration was successful, other results mean it failed
	 *	@see ERegistrationResult
	 */
	ERegistrationResult RegisterNavData(ANavigationData* NavData);

	/** tries to register navigation area */
	void RegisterNavAreaClass(UClass* NavAreaClass);

	/** tries to unregister navigation area */
	void UnregisterNavAreaClass(UClass* NavAreaClass);

	void OnNavigationAreaEvent(UClass* AreaClass, ENavAreaEvent::Type Event);
	
 	FSetElementId RegisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	void UnregisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	
	/** read element data from navigation octree */
	bool GetNavOctreeElementData(UObject* NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);

	/** Adds given element to NavOctree. No check for owner's validity are performed, 
	 *	nor its presence in NavOctree - function assumes callee responsibility 
	 *	in this regard **/
	void AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement);

	void SetCrowdManager(UCrowdManagerBase* NewCrowdManager);

	/** Add BSP collision data to navigation octree */
	void AddLevelCollisionToOctree(ULevel* Level);
	
	/** Remove BSP collision data from navigation octree */
	void RemoveLevelCollisionFromOctree(ULevel* Level);

	virtual void SpawnMissingNavigationData();

private:
	// adds navigation bounds update request to a pending list
	void AddNavigationBoundsUpdateRequest(const FNavigationBoundsUpdateRequest& UpdateRequest);

	/** Triggers navigation building on all eligible navigation data. */
	void RebuildAll(bool bIsLoadTime = false);
		 
	/** Handler for FWorldDelegates::LevelAddedToWorld event */
	 void OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);
	 
	/** Handler for FWorldDelegates::LevelRemovedFromWorld event */
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	/** Adds given request to requests queue. Note it's to be called only on game thread only */
	void AddAsyncQuery(const FAsyncPathFindingQuery& Query);
		 
	/** spawns a non-game-thread task to process requests given in PathFindingQueries.
	 *	In the process PathFindingQueries gets copied. */
	void TriggerAsyncQueries(TArray<FAsyncPathFindingQuery>& PathFindingQueries);

	/** Processes pathfinding requests given in PathFindingQueries.*/
	void PerformAsyncQueries(TArray<FAsyncPathFindingQuery> PathFindingQueries);

	/** */
	void DestroyNavOctree();

	/** Whether Navigation system needs to populate nav octree. 
	 *  Depends on runtime generation settings of each navigation data, always true in the editor
	 */
	bool RequiresNavOctree() const;

	/** Return "Strongest" runtime generation type required by registered navigation data objects
	 *  Depends on runtime generation settings of each navigation data, always ERuntimeGenerationType::Dynamic in the editor world
	 */
	ERuntimeGenerationType GetRuntimeGenerationType() const;

	/** Discards all navigation data chunks in all sub-levels */
	static void DiscardNavigationDataChunks(UWorld* InWorld);

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
public:
	DEPRECATED(4.11, "UpdateNavOctree is deprecated. Use UpdateActorInNavOctree")
	static void UpdateNavOctree(AActor* Actor);
	DEPRECATED(4.11, "UpdateNavOctree is deprecated. Use UpdateComponentInNavOctree")
	static void UpdateNavOctree(UActorComponent* Comp);
	DEPRECATED(4.11, "UpdateNavOctreeAll is deprecated. Use UpdateActorAndComponentsInNavOctree")
	static void UpdateNavOctreeAll(AActor* Actor);
	DEPRECATED(4.16, "This version of ProjectPointToNavigation is deprecated. Please use the new version")
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "ProjectPointToNavigation_DEPRECATED", DeprecatedFunction, DeprecationMessage = "This version of ProjectPointToNavigation is deprecated. Please use the new version"))
	static FVector ProjectPointToNavigation(UObject* WorldContextObject, const FVector& Point, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, const FVector QueryExtent = FVector::ZeroVector);
	DEPRECATED(4.16, "This version of GetRandomReachablePointInRadius is deprecated. Please use the new version")
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomReachablePointInRadius_DEPRECATED", DeprecatedFunction, DeprecationMessage = "This version of GetRandomReachablePointInRadius is deprecated. Please use the new version"))
	static FVector GetRandomReachablePointInRadius(UObject* WorldContextObject, const FVector& Origin, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	DEPRECATED(4.16, "This version of GetRandomPointInNavigableRadius is deprecated. Please use the new version")
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomPointInNavigableRadius_DEPRECATED", DeprecatedFunction, DeprecationMessage = "This version of GetRandomPointInNavigableRadius is deprecated. Please use the new version"))
	static FVector GetRandomPointInNavigableRadius(UObject* WorldContextObject, const FVector& Origin, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
};

