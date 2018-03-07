// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "RenderCommandFence.h"
#include "Templates/ScopedCallback.h"
#include "Misc/WorldCompositionUtility.h"
#include "Engine/MaterialMerging.h"
#include "Level.generated.h"

class AActor;
class ABrush;
class AInstancedFoliageActor;
class ALevelBounds;
class APlayerController;
class AWorldSettings;
class FSceneInterface;
class ITargetPlatform;
class UAssetUserData;
class UMapBuildDataRegistry;
class UNavigationDataChunk;
class UTexture2D;
struct FLevelCollection;
class ULevelActorContainer;

/**
 * Structure containing all information needed for determining the screen space
 * size of an object/ texture instance.
 */
USTRUCT()
struct ENGINE_API FStreamableTextureInstance
{
	GENERATED_USTRUCT_BODY()

	/** Bounding sphere/ box of object */
	FBoxSphereBounds  Bounds;

	/** Min distance from view where this instance is usable */
	float MinDistance;
	/** Max distance from view where this instance is usable */
	float MaxDistance;

	/** Object (and bounding sphere) specific texel scale factor  */
	float	TexelFactor;

	/**
	 * FStreamableTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance );
};

/**
 * Serialized ULevel information about dynamic texture instances
 */
USTRUCT()
struct ENGINE_API FDynamicTextureInstance : public FStreamableTextureInstance
{
	GENERATED_USTRUCT_BODY()

	/** Texture that is used by a dynamic UPrimitiveComponent. */
	UPROPERTY()
	UTexture2D*					Texture;

	/** Whether the primitive that uses this texture is attached to the scene or not. */
	UPROPERTY()
	bool						bAttached;
	
	/** Original bounding sphere radius, at the time the TexelFactor was calculated originally. */
	UPROPERTY()
	float						OriginalRadius;

	/**
	 * FDynamicTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FDynamicTextureInstance& TextureInstance );
};

/** Manually implement TPointerIsConvertibleFromTo for AActor so that TWeakObjectPtr can use it below when AActor is forward declared. */
template<> struct TPointerIsConvertibleFromTo<AActor, const volatile UObject>
{
	enum { Value = 1 };
};

/** Struct that holds on to information about Actors that wish to be auto enabled for input before the player controller has been created */
struct FPendingAutoReceiveInputActor
{
	TWeakObjectPtr<AActor> Actor;
	int32 PlayerIndex;

	FPendingAutoReceiveInputActor(AActor* InActor, const int32 InPlayerIndex)
		: Actor(InActor)
		, PlayerIndex(InPlayerIndex)
	{
	}
};

/** A precomputed visibility cell, whose data is stored in FCompressedVisibilityChunk. */
class FPrecomputedVisibilityCell
{
public:

	/** World space min of the cell. */
	FVector Min;

	/** Index into FPrecomputedVisibilityBucket::CellDataChunks of this cell's data. */
	uint16 ChunkIndex;

	/** Index into the decompressed chunk data of this cell's visibility data. */
	uint16 DataOffset;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityCell& D )
	{
		Ar << D.Min << D.ChunkIndex << D.DataOffset;
		return Ar;
	}
};

/** A chunk of compressed visibility data from multiple FPrecomputedVisibilityCell's. */
class FCompressedVisibilityChunk
{
public:
	/** Whether the chunk is compressed. */
	bool bCompressed;

	/** Size of the uncompressed chunk. */
	int32 UncompressedSize;

	/** Compressed visibility data if bCompressed is true. */
	TArray<uint8> Data;

	friend FArchive& operator<<( FArchive& Ar, FCompressedVisibilityChunk& D )
	{
		Ar << D.bCompressed << D.UncompressedSize << D.Data;
		return Ar;
	}
};

/** A bucket of visibility cells that have the same spatial hash. */
class FPrecomputedVisibilityBucket
{
public:
	/** Size in bytes of the data of each cell. */
	int32 CellDataSize;

	/** Cells in this bucket. */
	TArray<FPrecomputedVisibilityCell> Cells;

	/** Data chunks corresponding to Cells. */
	TArray<FCompressedVisibilityChunk> CellDataChunks;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityBucket& D )
	{
		Ar << D.CellDataSize << D.Cells << D.CellDataChunks;
		return Ar;
	}
};

/** Handles operations on precomputed visibility data for a level. */
class FPrecomputedVisibilityHandler
{
public:

	FPrecomputedVisibilityHandler() :
		Id(NextId)
	{
		NextId++;
	}
	
	~FPrecomputedVisibilityHandler() 
	{ 
		UpdateVisibilityStats(false);
	}

	/** Updates visibility stats. */
	ENGINE_API void UpdateVisibilityStats(bool bAllocating) const;

	/** Sets this visibility handler to be actively used by the rendering scene. */
	ENGINE_API void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
	ENGINE_API void Invalidate(FSceneInterface* Scene);

	/** Shifts origin of precomputed visibility volume by specified offset */
	ENGINE_API void ApplyWorldOffset(const FVector& InOffset);

	/** @return the Id */
	int32 GetId() const { return Id; }

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D );
	
private:

	/** World space origin of the cell grid. */
	FVector2D PrecomputedVisibilityCellBucketOriginXY;

	/** World space size of every cell in x and y. */
	float PrecomputedVisibilityCellSizeXY;

	/** World space height of every cell. */
	float PrecomputedVisibilityCellSizeZ;

	/** Number of cells in each bucket in x and y. */
	int32	PrecomputedVisibilityCellBucketSizeXY;

	/** Number of buckets in x and y. */
	int32	PrecomputedVisibilityNumCellBuckets;

	static int32 NextId;

	/** Id used by the renderer to know when cached visibility data is valid. */
	int32 Id;

	/** Visibility bucket data. */
	TArray<FPrecomputedVisibilityBucket> PrecomputedVisibilityCellBuckets;

	friend class FLightmassProcessor;
	friend class FSceneViewState;
};

/** Volume distance field generated by Lightmass, used by image based reflections for shadowing. */
class FPrecomputedVolumeDistanceField
{
public:

	/** Sets this volume distance field to be actively used by the rendering scene. */
	ENGINE_API void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's volume distance field and frees any memory used by it. */
	ENGINE_API void Invalidate(FSceneInterface* Scene);

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D );

private:
	/** Largest world space distance stored in the volume. */
	float VolumeMaxDistance;
	/** World space bounding box of the volume. */
	FBox VolumeBox;
	/** Volume dimension X. */
	int32 VolumeSizeX;
	/** Volume dimension Y. */
	int32 VolumeSizeY;
	/** Volume dimension Z. */
	int32 VolumeSizeZ;
	/** Distance field data. */
	TArray<FColor> Data;

	friend class FScene;
	friend class FLightmassProcessor;
};

USTRUCT()
struct ENGINE_API FLevelSimplificationDetails
{
	GENERATED_USTRUCT_BODY()

	/** Whether to create separate packages for each generated asset. All in map package otherwise */
	UPROPERTY(Category=General, EditAnywhere)
	bool bCreatePackagePerAsset;

	/** Percentage of details for static mesh proxy */
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Details Percentage", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))	
	float DetailsPercentage;

	/** Landscape material simplification */
	UPROPERTY(Category = Landscape, EditAnywhere)
	FMaterialProxySettings StaticMeshMaterialSettings;

	UPROPERTY(Category = Landscape, EditAnywhere, meta=(InlineEditConditionToggle))
	bool bOverrideLandscapeExportLOD;

	/** Landscape LOD to use for static mesh generation, when not specified 'Max LODLevel' from landscape actor will be used */
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", editcondition = "bOverrideLandscapeExportLOD"))
	int32 LandscapeExportLOD;
	
	/** Landscape material simplification */
	UPROPERTY(Category = Landscape, EditAnywhere)
	FMaterialProxySettings LandscapeMaterialSettings;

	/** Whether to bake foliage into landscape static mesh texture */
	UPROPERTY(Category=Landscape, EditAnywhere)
	bool bBakeFoliageToLandscape;

	/** Whether to bake grass into landscape static mesh texture */
	UPROPERTY(Category=Landscape, EditAnywhere)
	bool bBakeGrassToLandscape;

	UPROPERTY()
	bool bGenerateMeshNormalMap_DEPRECATED;
	
	UPROPERTY()
	bool bGenerateMeshMetallicMap_DEPRECATED;

	UPROPERTY()
	bool bGenerateMeshRoughnessMap_DEPRECATED;
	
	UPROPERTY()
	bool bGenerateMeshSpecularMap_DEPRECATED;
	
	UPROPERTY()
	bool bGenerateLandscapeNormalMap_DEPRECATED;

	UPROPERTY()
	bool bGenerateLandscapeMetallicMap_DEPRECATED;

	UPROPERTY()
	bool bGenerateLandscapeRoughnessMap_DEPRECATED;
	
	UPROPERTY()
	bool bGenerateLandscapeSpecularMap_DEPRECATED;

	/** Handles deprecated properties */
	void PostLoadDeprecated();
	
	FLevelSimplificationDetails();
	bool operator == (const FLevelSimplificationDetails& Other) const;
};

//
// The level object.  Contains the level's actor list, BSP information, and brush list.
// Every Level has a World as its Outer and can be used as the PersistentLevel, however,
// when a Level has been streamed in the OwningWorld represents the World that it is a part of.
//


/**
 * A Level is a collection of Actors (lights, volumes, mesh instances etc.).
 * Multiple Levels can be loaded and unloaded into the World to create a streaming experience.
 * 
 * @see https://docs.unrealengine.com/latest/INT/Engine/Levels
 * @see UActor
 */
UCLASS(MinimalAPI)
class ULevel : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()

public:

	/** URL associated with this level. */
	FURL					URL;

	/** Array of all actors in this level, used by FActorIteratorBase and derived classes */
	TArray<AActor*> Actors;

	/** Array of actors to be exposed to GC in this level. All other actors will be referenced through ULevelActorContainer */
	TArray<AActor*> ActorsForGC;

	/** Set before calling LoadPackage for a streaming level to ensure that OwningWorld is correct on the Level */
	ENGINE_API static TMap<FName, TWeakObjectPtr<UWorld> > StreamedLevelsOwningWorld;
		
	/** 
	 * The World that has this level in its Levels array. 
	 * This is not the same as GetOuter(), because GetOuter() for a streaming level is a vestigial world that is not used. 
	 * It should not be accessed during BeginDestroy(), just like any other UObject references, since GC may occur in any order.
	 */
	UPROPERTY(transient)
	UWorld* OwningWorld;

	/** BSP UModel. */
	UPROPERTY()
	class UModel* Model;

	/** BSP Model components used for rendering. */
	UPROPERTY()
	TArray<class UModelComponent*> ModelComponents;

	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	ULevelActorContainer* ActorCluster;

#if WITH_EDITORONLY_DATA
	/** Reference to the blueprint for level scripting */
	UPROPERTY(NonTransactional)
	class ULevelScriptBlueprint* LevelScriptBlueprint;

	/** The Guid list of all materials and meshes Guid used in the last texture streaming build. Used to know if the streaming data needs rebuild. Only used for the persistent level. */
	UPROPERTY(NonTransactional)
	TArray<FGuid> TextureStreamingResourceGuids;
#endif //WITH_EDITORONLY_DATA

	/** Num of components missing valid texture streaming data. Updated in map check. */
	UPROPERTY(NonTransactional)
	int32 NumTextureStreamingUnbuiltComponents;

	/** Num of resources that have changed since the last texture streaming build. Updated in map check. */
	UPROPERTY(NonTransactional)
	int32 NumTextureStreamingDirtyResources;

	/** The level scripting actor, created by instantiating the class from LevelScriptBlueprint.  This handles all level scripting */
	UPROPERTY(NonTransactional)
	class ALevelScriptActor* LevelScriptActor;

	/**
	 * Start and end of the navigation list for this level, used for quickly fixing up
	 * when streaming this level in/out. @TODO DEPRECATED - DELETE
	 */
	UPROPERTY()
	class ANavigationObjectBase *NavListStart;
	UPROPERTY()
	class ANavigationObjectBase	*NavListEnd;
	
	/** Navigation related data that can be stored per level */
	UPROPERTY()
	TArray<UNavigationDataChunk*> NavDataChunks;
	
	/** Total number of KB used for lightmap textures in the level. */
	UPROPERTY(VisibleAnywhere, Category=Level)
	float LightmapTotalSize;
	/** Total number of KB used for shadowmap textures in the level. */
	UPROPERTY(VisibleAnywhere, Category=Level)
	float ShadowmapTotalSize;

	/** threes of triangle vertices - AABB filtering friendly. Stored if there's a runtime need to rebuild navigation that accepts BSPs 
	 *	as well - it's a lot easier this way than retrieve this data at runtime */
	UPROPERTY()
	TArray<FVector> StaticNavigableGeometry;

	/** The Guid of each texture refered by FStreamingTextureBuildInfo::TextureLevelIndex	*/
	UPROPERTY()
	TArray<FGuid> StreamingTextureGuids;

	/** Data structures for holding the tick functions **/
	class FTickTaskLevel*						TickTaskLevel;

	/** 
	* The precomputed light information for this level.  
	* The extra level of indirection is to allow forward declaring FPrecomputedLightVolume.
	*/
	class FPrecomputedLightVolume*				PrecomputedLightVolume;

	/** The volumetric lightmap data for this level. */
	class FPrecomputedVolumetricLightmap*			PrecomputedVolumetricLightmap;

	/** Contains precomputed visibility data for this level. */
	FPrecomputedVisibilityHandler				PrecomputedVisibilityHandler;

	/** Precomputed volume distance field for this level. */
	FPrecomputedVolumeDistanceField				PrecomputedVolumeDistanceField;

	/** Fence used to track when the rendering thread has finished referencing this ULevel's resources. */
	FRenderCommandFence							RemoveFromSceneFence;

	/** 
	 * Whether the level is a lighting scenario.  Lighting is built separately for each lighting scenario level with all other scenario levels hidden. 
	 * Only one lighting scenario level should be visible at a time for correct rendering, and lightmaps from that level will be used on the rest of the world.
	 * Note: When a lighting scenario level is present, lightmaps for all streaming levels are placed in the scenario's _BuildData package.  
	 *		This means that lightmaps for those streaming levels will not be streamed with them.
	 */
	UPROPERTY()
	bool bIsLightingScenario;

	/** Identifies map build data specific to this level, eg lighting volume samples. */
	UPROPERTY()
	FGuid LevelBuildDataId;

	/** 
	 * Registry for data from the map build.  This is stored in a separate package from the level to speed up saving / autosaving. 
	 * ReleaseRenderingResources must be called before changing what is referenced, to update the rendering thread state.
	 */
	UPROPERTY()
	UMapBuildDataRegistry* MapBuildData;

	/** Level offset at time when lighting was built */
	UPROPERTY()
	FIntVector LightBuildLevelOffset;

	/** Whether components are currently registered or not. */
	uint8										bAreComponentsCurrentlyRegistered:1;

	/** Whether the geometry needs to be rebuilt for correct lighting */
	uint8										bGeometryDirtyForLighting:1;

	/** Whether a level transform rotation was applied since the texture streaming builds. Invalidates the precomputed streaming bounds. */
	UPROPERTY()
	uint8 										bTextureStreamingRotationChanged : 1;

	/** Whether the level is currently visible/ associated with the world */
	UPROPERTY(transient)
	uint8										bIsVisible:1;

	/** Whether this level is locked; that is, its actors are read-only 
	 *	Used by WorldBrowser to lock a level when corresponding ULevelStreaming does not exist
	 */
	UPROPERTY()
	uint8 										bLocked:1;
	
	/** The below variables are used temporarily while making a level visible.				*/

	/** Whether we already moved actors.													*/
	uint8										bAlreadyMovedActors:1;
	/** Whether we already shift actors positions according to world composition.			*/
	uint8										bAlreadyShiftedActors:1;
	/** Whether we already updated components.												*/
	uint8										bAlreadyUpdatedComponents:1;
	/** Whether we already associated streamable resources.									*/
	uint8										bAlreadyAssociatedStreamableResources:1;
	/** Whether we already initialized network actors.											*/
	uint8										bAlreadyInitializedNetworkActors:1;
	/** Whether we already routed initialize on actors.										*/
	uint8										bAlreadyRoutedActorInitialize:1;
	/** Whether we already sorted the actor list.											*/
	uint8										bAlreadySortedActorList:1;
	/** Whether this level is in the process of being associated with its world				*/
	uint8										bIsAssociatingLevel:1;
	/** Whether this level should be fully added to the world before rendering his components	*/
	uint8										bRequireFullVisibilityToRender:1;
	/** Whether this level is specific to client, visibility state will not be replicated to server	*/
	uint8										bClientOnlyVisible:1;
	/** Whether this level was duplicated for PIE	*/
	uint8										bWasDuplicatedForPIE:1;
	/** Whether the level is currently being removed from the world */
	uint8										bIsBeingRemoved:1;
	/** Whether this level has gone through a complete rerun construction script pass. */
	uint8										bHasRerunConstructionScripts:1;
	/** Whether the level had its actor cluster created. This doesn't mean that the creation was successful. */
	uint8										bActorClusterCreated : 1;

	/** Current index into actors array for updating components.							*/
	int32										CurrentActorIndexForUpdateComponents;
	/** Current index into actors array for updating components.							*/
	int32										CurrentActorIndexForUnregisterComponents;


	/** Whether the level is currently pending being made visible.							*/
	DEPRECATED(4.15, "Use HasVisibilityChangeRequestPending")
	bool HasVisibilityRequestPending() const;

	/** Whether the level is currently pending being made invisible or visible.				*/
	bool HasVisibilityChangeRequestPending() const;

	// Event on level transform changes
	DECLARE_MULTICAST_DELEGATE_OneParam(FLevelTransformEvent, const FTransform&);
	FLevelTransformEvent OnApplyLevelTransform;

#if WITH_EDITORONLY_DATA
	/** Level simplification settings for each LOD */
	UPROPERTY()
	FLevelSimplificationDetails LevelSimplification[WORLDTILE_LOD_MAX_INDEX];

	/** 
	 * The level color used for visualization. (Show -> Advanced -> Level Coloration)
	 * Used only in world composition mode
	 */
	UPROPERTY()
	FLinearColor LevelColor;

	float FixupOverrideVertexColorsTime;
	int32 FixupOverrideVertexColorsCount;
#endif //WITH_EDITORONLY_DATA

	/** Actor which defines level logical bounding box				*/
	TWeakObjectPtr<ALevelBounds>				LevelBoundsActor;

	/** Cached pointer to Foliage actor		*/
	TWeakObjectPtr<AInstancedFoliageActor>		InstancedFoliageActor;

	/** Called when Level bounds actor has been updated */
	DECLARE_EVENT( ULevel, FLevelBoundsActorUpdatedEvent );
	FLevelBoundsActorUpdatedEvent& LevelBoundsActorUpdated() { return LevelBoundsActorUpdatedEvent; }
	/**	Broadcasts that Level bounds actor has been updated */ 
	void BroadcastLevelBoundsActorUpdated() { LevelBoundsActorUpdatedEvent.Broadcast(); }

	/** Marks level bounds as dirty so they will be recalculated  */
	ENGINE_API void MarkLevelBoundsDirty();

private:
	FLevelBoundsActorUpdatedEvent LevelBoundsActorUpdatedEvent; 

	UPROPERTY()
	AWorldSettings* WorldSettings;

	/** Cached level collection that this level is contained in, for faster access than looping through the collections in the world. */
	FLevelCollection* CachedLevelCollection;

protected:

	/** Array of user data stored with the asset */
	UPROPERTY()
	TArray<UAssetUserData*> AssetUserData;

private:
	// Actors awaiting input to be enabled once the appropriate PlayerController has been created
	TArray<FPendingAutoReceiveInputActor> PendingAutoReceiveInputActors;

public:
	// Used internally to determine which actors should go on the world's NetworkActor list
	static bool IsNetActor(const AActor* Actor);

	/** Called when a level package has been dirtied. */
	ENGINE_API static FSimpleMulticastDelegate LevelDirtiedEvent;

	// Constructor.
	ENGINE_API void Initialize(const FURL& InURL);
	ULevel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	ULevel(FVTableHelper& Helper)
		: Super(Helper)
		, Actors()
	{}

	~ULevel();

	//~ Begin UObject Interface.
	virtual void PostInitProperties() override;	
	virtual void Serialize( FArchive& Ar ) override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	virtual UWorld* GetWorld() const override;

#if	WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform) override;
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual bool CanBeClusterRoot() const override;
	virtual void CreateCluster() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	/**
	 * Clears all components of actors associated with this level (aka in Actors array) and 
	 * also the BSP model components.
	 */
	ENGINE_API void ClearLevelComponents();

	/**
	 * Updates all components of actors associated with this level (aka in Actors array) and 
	 * creates the BSP model components.
	 * @param bRerunConstructionScripts	If we want to rerun construction scripts on actors in level
	 */
	ENGINE_API void UpdateLevelComponents(bool bRerunConstructionScripts);

	/**
	 * Incrementally updates all components of actors associated with this level.
	 *
	 * @param NumComponentsToUpdate		Number of components to update in this run, 0 for all
	 * @param bRerunConstructionScripts	If we want to rerun construction scripts on actors in level
	 */
	void IncrementalUpdateComponents( int32 NumComponentsToUpdate, bool bRerunConstructionScripts );

	/**
	* Incrementally unregisters all components of actors associated with this level.
	* This is done at the granularity of actors (individual actors have all of their components unregistered)
    *
	* @param NumComponentsToUnregister		Minimum number of components to unregister in this run, 0 for all
	*/
	bool IncrementalUnregisterComponents(int32 NumComponentsToUnregister);


	/**
	 * Invalidates the cached data used to render the level's UModel.
	 */
	void InvalidateModelGeometry();
	
#if WITH_EDITOR
	/** Marks all level components render state as dirty */
	ENGINE_API void MarkLevelComponentsRenderStateDirty();

	/** Called to create ModelComponents for BSP rendering */
	void CreateModelComponents();
#endif // WITH_EDITOR

	/**
	 * Updates the model components associated with this level
	 */
	ENGINE_API void UpdateModelComponents();

	/**
	 * Commits changes made to the UModel's surfaces.
	 */
	ENGINE_API void CommitModelSurfaces();

	/**
	 * Discards the cached data used to render the level's UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 */
	void InvalidateModelSurface();

	/**
	 * Makes sure that all light components have valid GUIDs associated.
	 */
	void ValidateLightGUIDs();

	/**
	 * Sorts the actor list by net relevancy and static behaviour. First all not net relevant static
	 * actors, then all net relevant static actors and then the rest. This is done to allow the dynamic
	 * and net relevant actor iterators to skip large amounts of actors.
	 */
	ENGINE_API void SortActorList();

	virtual bool IsNameStableForNetworking() const override { return true; }		// For now, assume all levels have stable net names

	/** Handles network initialization for actors in this level */
	void InitializeNetworkActors();

	/** Initializes rendering resources for this level. */
	ENGINE_API void InitializeRenderingResources();

	/** Releases rendering resources for this level. */
	ENGINE_API void ReleaseRenderingResources();

	/**
	 * Routes pre and post initialize to actors and also sets volumes.
	 *
	 * @todo seamless worlds: this doesn't correctly handle volumes in the multi- level case
	 */
	void RouteActorInitialize();

	/**
	 * Rebuilds static streaming data for all levels in the specified UWorld.
	 *
	 * @param World				Which world to rebuild streaming data for. If NULL, all worlds will be processed.
	 * @param TargetLevel		[opt] Specifies a single level to process. If NULL, all levels will be processed.
	 * @param TargetTexture		[opt] Specifies a single texture to process. If NULL, all textures will be processed.
	 */
	ENGINE_API static void BuildStreamingData(UWorld* World, ULevel* TargetLevel=NULL, UTexture2D* TargetTexture=NULL);

	/**
	 * Returns the default brush for this level.
	 *
	 * @return		The default brush for this level.
	 */
	ENGINE_API ABrush* GetDefaultBrush() const;

	/**
	 * Returns the world info for this level.
	 *
	 * @return		The AWorldSettings for this level.
	 */
	ENGINE_API AWorldSettings* GetWorldSettings(bool bChecked = true) const;

	ENGINE_API void SetWorldSettings(AWorldSettings* NewWorldSettings);

	/**
	 * Returns the level scripting actor associated with this level
	 * @return	a pointer to the level scripting actor for this level (may be NULL)
	 */
	ENGINE_API class ALevelScriptActor* GetLevelScriptActor() const;

	/** Returns the cached collection that contains this level, if any. May be null. */
	FLevelCollection* GetCachedLevelCollection() const { return CachedLevelCollection; }

	/** Sets the cached level collection that contains this level. Should only be called by FLevelCollection. */
	void SetCachedLevelCollection(FLevelCollection* const InCachedLevelCollection) { CachedLevelCollection = InCachedLevelCollection; }

	/**
	 * Utility searches this level's actor list for any actors of the specified type.
	 */
	bool HasAnyActorsOfType(UClass *SearchType);

	/**
	 * Resets the level nav list.
	 */
	ENGINE_API void ResetNavList();

	ENGINE_API UPackage* CreateMapBuildDataPackage() const;

	ENGINE_API UMapBuildDataRegistry* GetOrCreateMapBuildData();

	/** Sets whether this level is a lighting scenario and handles propagating the change. */
	ENGINE_API void SetLightingScenario(bool bNewIsLightingScenario);

	/** Creates UMapBuildDataRegistry entries for legacy lightmaps from components loaded for this level. */
	ENGINE_API void HandleLegacyMapBuildData();

#if WITH_EDITOR
	/** 
	*  Called after lighting was built and data gets propagated to this level
	*  @param	bLightingSuccessful	 Whether lighting build was successful
	*/
	ENGINE_API void OnApplyNewLightingData(bool bLightingSuccessful);

	/**
	 *	Grabs a reference to the level scripting blueprint for this level.  If none exists, it creates a new blueprint
	 *
	 * @param	bDontCreate		If true, if no level scripting blueprint is found, none will be created
	 */
	ENGINE_API class ULevelScriptBlueprint* GetLevelScriptBlueprint(bool bDontCreate=false);

	/**
	 * Nulls certain references related to the LevelScriptBlueprint. Called by UWorld::CleanupWorld.
	 */
	ENGINE_API void CleanupLevelScriptBlueprint();

	/**
	 *  Returns a list of all blueprints contained within the level
	 */
	ENGINE_API TArray<class UBlueprint*> GetLevelBlueprints() const;

	/**
	 *  Called when the level script blueprint has been successfully changed and compiled.  Handles creating an instance of the blueprint class in LevelScriptActor
	 */
	ENGINE_API void OnLevelScriptBlueprintChanged(class ULevelScriptBlueprint* InBlueprint);

	/** 
	 * Call on a level that was loaded from disk instead of PIE-duplicating, to fixup actor references
	 */
	ENGINE_API void FixupForPIE(int32 PIEInstanceID);
#endif

	/** @todo document */
	ENGINE_API TArray<FVector> const* GetStaticNavigableGeometry() const { return &StaticNavigableGeometry;}

	/** 
	* Is this the persistent level 
	*/
	ENGINE_API bool IsPersistentLevel() const;

	/** 
	* Is this the current level in the world it is owned by
	*/
	ENGINE_API bool IsCurrentLevel() const;
	
	/** 
	 * Shift level actors by specified offset
	 * The offset vector will get subtracted from all actors positions and corresponding data structures
	 *
	 * @param InWorldOffset	 Vector to shift all actors by
	 * @param bWorldShift	 Whether this call is part of whole world shifting
	 */
	ENGINE_API void ApplyWorldOffset(const FVector& InWorldOffset, bool bWorldShift);

	/** Register an actor that should be added to a player's input stack when they are created */
	void RegisterActorForAutoReceiveInput(AActor* Actor, const int32 PlayerIndex);

	/** Push any pending auto receive input actor's input components on to the player controller's input stack */
	void PushPendingAutoReceiveInput(APlayerController* PC);
	
	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	//~ End IInterface_AssetUserData Interface

#if WITH_EDITOR
	/** meant to be called only from editor, calculating and storing static geometry to be used with off-line and/or on-line navigation building */
	ENGINE_API void RebuildStaticNavigableGeometry();

#endif

};


/**
 * Macro for wrapping Delegates in TScopedCallback
 */
 #define DECLARE_SCOPED_DELEGATE( CallbackName, TriggerFunc )						\
	class ENGINE_API FScoped##CallbackName##Impl										\
	{																				\
	public:																			\
		static void FireCallback() { TriggerFunc; }									\
	};																				\
																					\
	typedef TScopedCallback<FScoped##CallbackName##Impl> FScoped##CallbackName;

DECLARE_SCOPED_DELEGATE( LevelDirtied, ULevel::LevelDirtiedEvent.Broadcast() );

#undef DECLARE_SCOPED_DELEGATE
