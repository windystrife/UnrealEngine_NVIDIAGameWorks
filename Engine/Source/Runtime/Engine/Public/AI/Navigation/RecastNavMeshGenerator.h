// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EngineDefines.h"
#include "AI/NavigationModifier.h"
#include "AI/NavigationOctree.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "Async/AsyncWork.h"
#include "UObject/GCObject.h"
#include "AI/NavDataGenerator.h"

#if WITH_RECAST

#include "Recast/Recast.h"
#include "Detour/DetourNavMesh.h"

class UBodySetup;
class ARecastNavMesh;
class FNavigationOctree;
class FNavMeshBuildContext;
class FRecastNavMeshGenerator;
struct BuildContext;
struct FNavigationRelevantData;
struct dtTileCacheLayer;

#define MAX_VERTS_PER_POLY	6

struct FRecastBuildConfig : public rcConfig
{
	/** controls whether voxel filterring will be applied (via FRecastTileGenerator::ApplyVoxelFilter) */
	uint32 bPerformVoxelFiltering:1;
	/** generate detailed mesh (additional tessellation to match heights of geometry) */
	uint32 bGenerateDetailedMesh:1;
	/** generate BV tree (space partitioning for queries) */
	uint32 bGenerateBVTree:1;
	/** if set, mark areas with insufficient free height instead of cutting them out  */
	uint32 bMarkLowHeightAreas : 1;

	/** region partitioning method used by tile cache */
	int32 TileCachePartitionType;
	/** chunk size for ChunkyMonotone partitioning */
	int32 TileCacheChunkSize;

	int32 PolyMaxHeight;
	/** indicates what's the limit of navmesh polygons per tile. This value is calculated from other
	 *	factors - DO NOT SET IT TO ARBITRARY VALUE */
	int32 MaxPolysPerTile;

	/** Actual agent height (in uu)*/
	float AgentHeight;
	/** Actual agent climb (in uu)*/
	float AgentMaxClimb;
	/** Actual agent radius (in uu)*/
	float AgentRadius;
	/** Agent index for filtering links */
	int32 AgentIndex;

	FRecastBuildConfig()
	{
		Reset();
	}

	void Reset()
	{
		FMemory::Memzero(*this);
		bPerformVoxelFiltering = true;
		bGenerateDetailedMesh = true;
		bGenerateBVTree = true;
		bMarkLowHeightAreas = false;
		PolyMaxHeight = 10;
		MaxPolysPerTile = -1;
		AgentIndex = 0;
	}
};

struct FRecastVoxelCache
{
	struct FTileInfo
	{
		int16 TileX;
		int16 TileY;
		int32 NumSpans;
		FTileInfo* NextTile;
		rcSpanCache* SpanData;
	};

	int32 NumTiles;

	/** tile info */
	FTileInfo* Tiles;

	FRecastVoxelCache() {}
	FRecastVoxelCache(const uint8* Memory);
};

struct FRecastGeometryCache
{
	struct FHeader
	{
		int32 NumVerts;
		int32 NumFaces;
		struct FWalkableSlopeOverride SlopeOverride;
	};

	FHeader Header;

	/** recast coords of vertices (size: NumVerts * 3) */
	float* Verts;

	/** vert indices for triangles (size: NumFaces * 3) */
	int32* Indices;

	FRecastGeometryCache() {}
	FRecastGeometryCache(const uint8* Memory);
};

struct FRecastRawGeometryElement
{
	// Instance geometry
	TArray<float>		GeomCoords;
	TArray<int32>		GeomIndices;
	
	// Per instance transformations in unreal coords
	// When empty geometry is in world space
	TArray<FTransform>	PerInstanceTransform;
};

struct FRecastAreaNavModifierElement
{
	TArray<FAreaNavModifier> Areas;
	
	// Per instance transformations in unreal coords
	// When empty areas are in world space
	TArray<FTransform>	PerInstanceTransform;
};

/**
 * Class handling generation of a single tile, caching data that can speed up subsequent tile generations
 */
class ENGINE_API FRecastTileGenerator : public FNoncopyable, public FGCObject
{
	friend FRecastNavMeshGenerator;

public:
	FRecastTileGenerator(FRecastNavMeshGenerator& ParentGenerator, const FIntPoint& Location);
	virtual ~FRecastTileGenerator();
		
	void DoWork();

	FORCEINLINE int32 GetTileX() const { return TileX; }
	FORCEINLINE int32 GetTileY() const { return TileY; }
	/** Whether specified layer was updated */
	FORCEINLINE bool IsLayerChanged(int32 LayerIdx) const { return DirtyLayers[LayerIdx]; }
	/** Whether tile data was fully regenerated */
	FORCEINLINE bool IsFullyRegenerated() const { return bRegenerateCompressedLayers; }
	/** Whether tile task has anything to build */
	bool HasDataToBuild() const;

	const TArray<FNavMeshTileData>& GetCompressedLayers() const { return CompressedLayers; }
protected:
	// to be used solely by FRecastNavMeshGenerator
	TArray<FNavMeshTileData>& GetNavigationData() { return NavigationData; }
	
public:
	uint32 GetUsedMemCount() const;

	// Memory amount used to construct generator 
	uint32 UsedMemoryOnStartup;

	// FGCObject begin
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	// FGCObject end
		
protected:
	/** Does the actual tile generations. 
	 *	@note always trigger tile generation only via TriggerAsyncBuild. This is a worker function
	 *	@return true if new tile navigation data has been generated and is ready to be added to navmesh instance, 
	 *	false if failed or no need to generate (still valid)
	 */
	bool GenerateTile();

	void Setup(const FRecastNavMeshGenerator& ParentGenerator, const TArray<FBox>& DirtyAreas);
	
	void GatherGeometry(const FRecastNavMeshGenerator& ParentGenerator, bool bGeometryChanged);
	void PrepareGeometrySources(const FRecastNavMeshGenerator& ParentGenerator, bool bGeometryChanged);
	void DoAsyncGeometryGathering();

	/** builds CompressedLayers array (geometry + modifiers) */
	virtual bool GenerateCompressedLayers(FNavMeshBuildContext& BuildContext);

	/** builds NavigationData array (layers + obstacles) */
	bool GenerateNavigationData(FNavMeshBuildContext& BuildContext);

	void ApplyVoxelFilter(struct rcHeightfield* SolidHF, float WalkableRadius);

	/** apply areas from DynamicAreas to layer */
	void MarkDynamicAreas(dtTileCacheLayer& Layer);
	void MarkDynamicArea(const FAreaNavModifier& Modifier, const FTransform& LocalToWorld, dtTileCacheLayer& Layer);
	void MarkDynamicArea(const FAreaNavModifier& Modifier, const FTransform& LocalToWorld, dtTileCacheLayer& Layer, const int32 AreaId, const int32* ReplaceAreaIdPtr);

	void AppendModifier(const FCompositeNavModifier& Modifier, const FNavDataPerInstanceTransformDelegate& InTransformsDelegate);
	/** Appends specified geometry to tile's geometry */
	void AppendGeometry(const TNavStatArray<uint8>& RawCollisionCache, const FNavDataPerInstanceTransformDelegate& InTransformsDelegate);
	void AppendVoxels(rcSpanCache* SpanData, int32 NumSpans);
	
	/** prepare voxel cache from collision data */
	void PrepareVoxelCache(const TNavStatArray<uint8>& RawCollisionCache, TNavStatArray<rcSpanCache>& SpanData);
	bool HasVoxelCache(const TNavStatArray<uint8>& RawVoxelCache, rcSpanCache*& CachedVoxels, int32& NumCachedVoxels) const;
	void AddVoxelCache(TNavStatArray<uint8>& RawVoxelCache, const rcSpanCache* CachedVoxels, const int32 NumCachedVoxels) const;

	void DumpAsyncData();

protected:
	uint32 bSucceeded : 1;
	uint32 bRegenerateCompressedLayers : 1;
	uint32 bFullyEncapsulatedByInclusionBounds : 1;
	uint32 bUpdateGeometry : 1;
	
	int32 TileX;
	int32 TileY;
	uint32 Version;
	/** Tile's bounding box, Unreal coords */
	FBox TileBB;
	
	/** Layers dirty flags */
	TBitArray<> DirtyLayers;
	
	/** Parameters defining navmesh tiles */
	FRecastBuildConfig TileConfig;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Additional config */
	FRecastNavMeshCachedData AdditionalCachedData;

	// generated tile data
	TArray<FNavMeshTileData> CompressedLayers;
	TArray<FNavMeshTileData> NavigationData;
	
	// tile's geometry: without voxel cache
	TArray<FRecastRawGeometryElement> RawGeometry;
	// areas used for creating navigation data: obstacles
	TArray<FRecastAreaNavModifierElement> Modifiers;
	// navigation links
	TArray<FSimpleLinkNavModifier> OffmeshLinks;

	TWeakPtr<FNavDataGenerator, ESPMode::ThreadSafe> ParentGeneratorWeakPtr;

	TNavStatArray<TSharedRef<FNavigationRelevantData, ESPMode::ThreadSafe> > NavigationRelevantData;
	TSharedPtr<FNavigationOctree, ESPMode::ThreadSafe> NavOctree; 
	FNavDataConfig NavDataConfig;
};

struct ENGINE_API FRecastTileGeneratorWrapper : public FNonAbandonableTask
{
	TSharedRef<FRecastTileGenerator> TileGenerator;

	FRecastTileGeneratorWrapper(TSharedRef<FRecastTileGenerator> InTileGenerator)
		: TileGenerator(InTileGenerator)
	{
	}
	
	void DoWork()
	{
		TileGenerator->DoWork();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRecastTileGenerator, STATGROUP_ThreadPoolAsyncTasks);
	}
};

typedef FAsyncTask<FRecastTileGeneratorWrapper> FRecastTileGeneratorTask;
//typedef FAsyncTask<FRecastTileGenerator> FRecastTileGeneratorTask;

struct FPendingTileElement
{
	/** tile coordinates on a grid in recast space */
	FIntPoint	Coord;
	/** distance to seed, used for sorting pending tiles */
	float		SeedDistance; 
	/** Whether we need a full rebuild for this tile grid cell */
	bool		bRebuildGeometry;
	/** We need to store dirty area bounds to check which cached layers needs to be regenerated
	 *  In case geometry is changed cached layers data will be fully regenerated without using dirty areas list
	 */
	TArray<FBox> DirtyAreas;

	FPendingTileElement()
		: Coord(FIntPoint::NoneValue)
		, SeedDistance(MAX_flt)
		, bRebuildGeometry(false)
	{
	}

	bool operator == (const FIntPoint& Location) const
	{
		return Coord == Location;
	}

	bool operator == (const FPendingTileElement& Other) const
	{
		return Coord == Other.Coord;
	}

	bool operator < (const FPendingTileElement& Other) const
	{
		return Other.SeedDistance < SeedDistance;
	}

	friend uint32 GetTypeHash(const FPendingTileElement& Element)
	{
		return GetTypeHash(Element.Coord);
	}
};

struct FRunningTileElement
{
	FRunningTileElement()
		: Coord(FIntPoint::NoneValue)
		, bShouldDiscard(false)
		, AsyncTask(nullptr)
	{
	}
	
	FRunningTileElement(FIntPoint InCoord)
		: Coord(InCoord)
		, bShouldDiscard(false)
		, AsyncTask(nullptr)
	{
	}

	bool operator == (const FRunningTileElement& Other) const
	{
		return Coord == Other.Coord;
	}
	
	/** tile coordinates on a grid in recast space */
	FIntPoint					Coord;
	/** whether generated results should be discarded */
	bool						bShouldDiscard; 
	FRecastTileGeneratorTask*	AsyncTask;
};

struct FTileTimestamp
{
	uint32 TileIdx;
	double Timestamp;
	
	bool operator == (const FTileTimestamp& Other) const
	{
		return TileIdx == Other.TileIdx;
	}
};

/**
 * Class that handles generation of the whole Recast-based navmesh.
 */
class ENGINE_API FRecastNavMeshGenerator : public FNavDataGenerator
{
public:
	FRecastNavMeshGenerator(ARecastNavMesh& InDestNavMesh);
	virtual ~FRecastNavMeshGenerator();

private:
	/** Prevent copying. */
	FRecastNavMeshGenerator(FRecastNavMeshGenerator const& NoCopy) { check(0); };
	FRecastNavMeshGenerator& operator=(FRecastNavMeshGenerator const& NoCopy) { check(0); return *this; }

public:
	virtual bool RebuildAll() override;
	virtual void EnsureBuildCompletion() override;
	virtual void CancelBuild() override;
	virtual void TickAsyncBuild(float DeltaSeconds) override;
	virtual void OnNavigationBoundsChanged() override;

	/** Asks generator to update navigation affected by DirtyAreas */
	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas) override;
	virtual bool IsBuildInProgress(bool bCheckDirtyToo = false) const override;
	virtual int32 GetNumRemaningBuildTasks() const override;
	virtual int32 GetNumRunningBuildTasks() const override;

	/** Checks if a given tile is being build or has just finished building */
	bool IsTileChanged(int32 TileIdx) const;
		
	FORCEINLINE uint32 GetVersion() const { return Version; }

	const ARecastNavMesh* GetOwner() const { return DestNavMesh; }

	/** update area data */
	void OnAreaAdded(const UClass* AreaClass, int32 AreaID);
		
	//--- accessors --- //
	FORCEINLINE class UWorld* GetWorld() const { return DestNavMesh->GetWorld(); }

	const FRecastBuildConfig& GetConfig() const { return Config; }

	const TNavStatArray<FBox>& GetInclusionBounds() const { return InclusionBounds; }
	
	/** checks if any on InclusionBounds encapsulates given box.
	 *	@return index to first item in InclusionBounds that meets expectations */
	int32 FindInclusionBoundEncapsulatingBox(const FBox& Box) const;

	/** Total navigable area box, sum of all navigation volumes bounding boxes */
	FBox GetTotalBounds() const { return TotalNavBounds; }

	const FRecastNavMeshCachedData& GetAdditionalCachedData() const { return AdditionalCachedData; }

	bool HasDirtyTiles() const;

	bool GatherGeometryOnGameThread() const;

	FBox GrowBoundingBox(const FBox& BBox, bool bIncludeAgentHeight) const;
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
	virtual void ExportNavigationData(const FString& FileName) const override;
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot, const FBox& BoundingBox, const struct FLogCategoryBase& Category, ELogVerbosity::Type Verbosity) const override;
#endif

	/** 
	 *	@param Actor is a reference to make callee responsible for assuring it's valid
	 */
	static void ExportComponentGeometry(UActorComponent* Component, FNavigationRelevantData& Data);
	static void ExportVertexSoupGeometry(const TArray<FVector>& Verts, FNavigationRelevantData& Data);

	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& LocalToWorld = FTransform::Identity);
	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutTriMeshVertexBuffer, TNavStatArray<int32>& OutTriMeshIndexBuffer, TNavStatArray<FVector>& OutConvexVertexBuffer, TNavStatArray<int32>& OutConvexIndexBuffer, TNavStatArray<int32>& OutShapeBuffer, const FTransform& LocalToWorld = FTransform::Identity);
	
protected:
	// Performs initial setup of member variables so that generator is ready to do its thing from this point on
	void Init();

	// Updates cached list of navigation bounds
	void UpdateNavigationBounds();
		
	// Sorts pending build tiles by proximity to player, so tiles closer to player will get generated first
	void SortPendingBuildTiles();

	/** Instantiates dtNavMesh and configures it for tiles generation. Returns false if failed */
	bool ConstructTiledNavMesh();

	/** Determine bit masks for poly address */
	void CalcNavMeshProperties(int32& MaxTiles, int32& MaxPolys);
	
	/** Marks grid tiles affected by specified areas as dirty */
	void MarkDirtyTiles(const TArray<FNavigationDirtyArea>& DirtyAreas);
	
	/** Processes pending tile generation tasks */
	TArray<uint32> ProcessTileTasks(const int32 NumTasksToSubmit);

public:
	/** Adds generated tiles to NavMesh, replacing old ones */
	TArray<uint32> AddGeneratedTiles(FRecastTileGenerator& TileGenerator);

public:
	/** Removes all tiles at specified grid location */
	TArray<uint32> RemoveTileLayers(const int32 TileX, const int32 TileY, TMap<int32, dtPolyRef>* OldLayerTileIdMap = nullptr);

	void RemoveTiles(const TArray<FIntPoint>& Tiles);
	void ReAddTiles(const TArray<FIntPoint>& Tiles);

	bool IsBuildingRestrictedToActiveTiles() const { return bRestrictBuildingToActiveTiles; }

	/** sets a limit to number of asynchronous tile generators running at one time
	 *	@note if used at runtime will not result in killing tasks above limit count
	 *	@mote function does not validate the input parameter - it's on caller */
	void SetMaxTileGeneratorTasks(int32 NewLimit) { MaxTileGeneratorTasks = NewLimit; }

	static void CalcPolyRefBits(ARecastNavMesh* NavMeshOwner, int32& MaxTileBits, int32& MaxPolyBits);

protected:
	bool IsInActiveSet(const FIntPoint& Tile) const;
	void RestrictBuildingToActiveTiles(bool InRestrictBuildingToActiveTiles);
	
	/** Blocks until build for specified list of tiles is complete and discard results */
	void DiscardCurrentBuildingTasks();

	virtual TSharedRef<FRecastTileGenerator> CreateTileGenerator(const FIntPoint& Coord, const TArray<FBox>& DirtyAreas);

	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	virtual uint32 LogMemUsed() const override;

private:
	friend ARecastNavMesh;

	/** Parameters defining navmesh tiles */
	FRecastBuildConfig Config;
	
	int32 NumActiveTiles;
	/** the limit to number of asynchronous tile generators running at one time */
	int32 MaxTileGeneratorTasks;
	float AvgLayersPerTile;

	/** Total bounding box that includes all volumes, in unreal units. */
	FBox TotalNavBounds;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Navigation mesh that owns this generator */
	ARecastNavMesh*	DestNavMesh;
	
	/** List of dirty tiles that needs to be regenerated */
	TNavStatArray<FPendingTileElement> PendingDirtyTiles;			
	
	/** List of dirty tiles currently being regenerated */
	TNavStatArray<FRunningTileElement> RunningDirtyTiles;

#if WITH_EDITOR
	/** List of tiles that were recently regenerated */
	TNavStatArray<FTileTimestamp> RecentlyBuiltTiles;
#endif// WITH_EDITOR
	
	TArray<FIntPoint> ActiveTiles;

	/** */
	FRecastNavMeshCachedData AdditionalCachedData;

	uint32 bInitialized:1;

	uint32 bRestrictBuildingToActiveTiles:1;

	/** Runtime generator's version, increased every time all tile generators get invalidated
	 *	like when navmesh size changes */
	uint32 Version;
};

#endif // WITH_RECAST
