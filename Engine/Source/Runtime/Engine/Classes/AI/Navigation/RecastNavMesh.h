// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationData.h"
#include "RecastNavMesh.generated.h"

/** Initial checkin. */
#define NAVMESHVER_INITIAL				1
#define NAVMESHVER_TILED_GENERATION		2
#define NAVMESHVER_SEAMLESS_REBUILDING_1 3
#define NAVMESHVER_AREA_CLASSES			4
#define NAVMESHVER_CLUSTER_PATH			5
#define NAVMESHVER_SEGMENT_LINKS		6
#define NAVMESHVER_DYNAMIC_LINKS		7
#define NAVMESHVER_64BIT				9
#define NAVMESHVER_CLUSTER_SIMPLIFIED	10
#define NAVMESHVER_OFFMESH_HEIGHT_BUG	11
#define NAVMESHVER_LANDSCAPE_HEIGHT		13


#define NAVMESHVER_LATEST				NAVMESHVER_LANDSCAPE_HEIGHT
#define NAVMESHVER_MIN_COMPATIBLE		NAVMESHVER_LANDSCAPE_HEIGHT

#define RECAST_MAX_SEARCH_NODES		2048

#define RECAST_MIN_TILE_SIZE		300.f

#define RECAST_MAX_AREAS			64
#define RECAST_DEFAULT_AREA			(RECAST_MAX_AREAS - 1)
#define RECAST_LOW_AREA				(RECAST_MAX_AREAS - 2)
#define RECAST_NULL_AREA			0
#define RECAST_STRAIGHTPATH_OFFMESH_CONNECTION 0x04
#define RECAST_UNWALKABLE_POLY_COST	FLT_MAX

class FPImplRecastNavMesh;
class FRecastQueryFilter;
class INavLinkCustomInterface;
class UCanvas;
class UNavArea;
class UNavigationSystem;
class UPrimitiveComponent;
class URecastNavMeshDataChunk;
struct FRecastAreaNavModifierElement;
class dtNavMesh;
class dtQueryFilter;

UENUM()
namespace ERecastPartitioning
{
	// keep in sync with rcRegionPartitioning enum!

	enum Type
	{
		Monotone,
		Watershed,
		ChunkyMonotone,
	};
}

namespace ERecastPathFlags
{
	/** If set, path won't be post processed. */
	const int32 SkipStringPulling = (1 << 0);

	/** If set, path will contain navigation corridor. */
	const int32 GenerateCorridor = (1 << 1);
}

/** Helper to translate FNavPathPoint.Flags. */
struct ENGINE_API FNavMeshNodeFlags
{
	/** Extra node information (like "path start", "off-mesh connection"). */
	uint8 PathFlags;
	/** Area type after this node. */
	uint8 Area;
	/** Area flags for this node. */
	uint16 AreaFlags;

	FNavMeshNodeFlags() : PathFlags(0), Area(0), AreaFlags(0) {}
	FNavMeshNodeFlags(uint32 Flags) : PathFlags(Flags), Area(Flags >> 8), AreaFlags(Flags >> 16) {}
	uint32 Pack() const { return PathFlags | ((uint32)Area << 8) | ((uint32)AreaFlags << 16); }
	bool IsNavLink() const { return (PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;  }

	FNavMeshNodeFlags& AddAreaFlags(const uint16 InAreaFlags) 
	{
		AreaFlags = (AreaFlags | InAreaFlags);
		return *this;
	}
};

struct ENGINE_API FNavMeshPath : public FNavigationPath
{
	typedef FNavigationPath Super;

	FNavMeshPath();

	FORCEINLINE void SetWantsStringPulling(const bool bNewWantsStringPulling) { bWantsStringPulling = bNewWantsStringPulling; }
	FORCEINLINE bool WantsStringPulling() const { return bWantsStringPulling; }
	FORCEINLINE bool IsStringPulled() const { return bStringPulled; }
	
	/** find string pulled path from PathCorridor */
	void PerformStringPulling(const FVector& StartLoc, const FVector& EndLoc);

	FORCEINLINE void SetWantsPathCorridor(const bool bNewWantsPathCorridor) { bWantsPathCorridor = bNewWantsPathCorridor; }
	FORCEINLINE bool WantsPathCorridor() const { return bWantsPathCorridor; }
	
	FORCEINLINE const TArray<FNavigationPortalEdge>& GetPathCorridorEdges() const { return bCorridorEdgesGenerated ? PathCorridorEdges : GeneratePathCorridorEdges(); }
	FORCEINLINE void SetPathCorridorEdges(const TArray<FNavigationPortalEdge>& InPathCorridorEdges) { PathCorridorEdges = InPathCorridorEdges; bCorridorEdgesGenerated = true; }

	FORCEINLINE void OnPathCorridorUpdated() { bCorridorEdgesGenerated = false; }

	virtual void DebugDraw(const ANavigationData* NavData, FColor PathColor, UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex = 0) const override;
	
	bool ContainsWithSameEnd(const FNavMeshPath* Other) const;
	
	void OffsetFromCorners(float Distance);

	void ApplyFlags(int32 NavDataFlags);

	virtual void ResetForRepath() override;

	/** get flags of path point or corridor poly (depends on bStringPulled flag) */
	bool GetNodeFlags(int32 NodeIdx, FNavMeshNodeFlags& Flags) const;

	/** get cost of path, starting from next poly in corridor */
	virtual float GetCostFromNode(NavNodeRef PathNode) const override { return GetCostFromIndex(PathCorridor.Find(PathNode) + 1); }

	/** get cost of path, starting from given point */
	virtual float GetCostFromIndex(int32 PathPointIndex) const override
	{
		float TotalCost = 0.f;
		const float* Cost = PathCorridorCost.GetData();
		for (int32 PolyIndex = PathPointIndex; PolyIndex < PathCorridorCost.Num(); ++PolyIndex, ++Cost)
		{
			TotalCost += *Cost;
		}

		return TotalCost;
	}

	FORCEINLINE_DEBUGGABLE float GetTotalPathLength() const
	{
		return bStringPulled ? GetStringPulledLength(0) : GetPathCorridorLength(0);
	}

	FORCEINLINE int32 GetNodeRefIndex(const NavNodeRef NodeRef) const { return PathCorridor.Find(NodeRef); }

	/** check if path (all polys in corridor) contains given node */
	virtual bool ContainsNode(NavNodeRef NodeRef) const override { return PathCorridor.Contains(NodeRef); }

	virtual bool ContainsCustomLink(uint32 UniqueLinkId) const override { return CustomLinkIds.Contains(UniqueLinkId); }
	virtual bool ContainsAnyCustomLink() const override { return CustomLinkIds.Num() > 0; }

	bool IsPathSegmentANavLink(const int32 PathSegmentStartIndex) const;

	virtual bool DoesIntersectBox(const FBox& Box, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const override;
	virtual bool DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const override;
	/** retrieves normalized direction vector to given path segment. If path is not string pulled navigation corridor is being used */
	virtual FVector GetSegmentDirection(uint32 SegmentEndIndex) const override;

private:
	bool DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const;
	void InternalResetNavMeshPath();

public:

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const override;
	virtual FString GetDescription() const override;
#endif // ENABLE_VISUAL_LOG

protected:
	/** calculates total length of string pulled path. Does not generate string pulled 
	 *	path if it's not already generated (see bWantsStringPulling and bStrigPulled)
	 *	Internal use only */
	float GetStringPulledLength(const int32 StartingPoint) const;

	/** calculates estimated length of path expressed as sequence of navmesh edges.
	 *	It basically sums up distances between every subsequent nav edge pair edge middles.
	 *	Internal use only */
	float GetPathCorridorLength(const int32 StartingEdge) const;

	/** it's only const to be callable in const environment. It's not supposed to be called directly externally anyway,
	 *	just as part of retrieving corridor on demand or generating it in internal processes. It fills a mutable
	 *	array. */
	const TArray<FNavigationPortalEdge>& GeneratePathCorridorEdges() const;

public:
	
	/** sequence of navigation mesh poly ids representing an obstacle-free navigation corridor */
	TArray<NavNodeRef> PathCorridor;

	/** for every poly in PathCorridor stores traversal cost from previous navpoly */
	TArray<float> PathCorridorCost;

	/** set of unique link Ids */
	TArray<uint32> CustomLinkIds;

private:
	/** sequence of FVector pairs where each pair represents navmesh portal edge between two polygons navigation corridor.
	 *	Note, that it should always be accessed via GetPathCorridorEdges() since PathCorridorEdges content is generated
	 *	on first access */
	mutable TArray<FNavigationPortalEdge> PathCorridorEdges;

	/** transient variable indicating whether PathCorridorEdges contains up to date information */
	mutable uint32 bCorridorEdgesGenerated : 1;

public:
	/** is this path generated on dynamic navmesh (i.e. one attached to moving surface) */
	uint32 bDynamic : 1;

protected:
	/** does this path contain string pulled path? If true then NumPathVerts > 0 and
	 *	OutPathVerts contains valid data. If false there's only navigation corridor data
	 *	available.*/
	uint32 bStringPulled : 1;	

	/** If set to true path instance will contain a string pulled version. Otherwise only
	 *	navigation corridor will be available. Defaults to true */
	uint32 bWantsStringPulling : 1;

	/** If set to true path instance will contain path corridor generated as a part 
	 *	pathfinding call (i.e. without the need to generate it with GeneratePathCorridorEdges */
	uint32 bWantsPathCorridor : 1;

public:
	static const FNavPathType Type;
};

#if WITH_RECAST
struct FRecastDebugPathfindingNode
{
	NavNodeRef PolyRef;
	NavNodeRef ParentRef;
	float Cost;
	float TotalCost;
	float Length;
	uint32 bOpenSet : 1;
	uint32 bOffMeshLink : 1;
	uint32 bModified : 1;

	FVector NodePos;
	TArray<FVector> Verts;

	FRecastDebugPathfindingNode() : PolyRef(0), ParentRef(0) {}
	FRecastDebugPathfindingNode(NavNodeRef InPolyRef) : PolyRef(InPolyRef), ParentRef(0) {}

	FORCEINLINE bool operator==(const NavNodeRef& OtherPolyRef) const { return PolyRef == OtherPolyRef; }
	FORCEINLINE bool operator==(const FRecastDebugPathfindingNode& Other) const { return PolyRef == Other.PolyRef; }
	FORCEINLINE friend uint32 GetTypeHash(const FRecastDebugPathfindingNode& Other) { return GetTypeHash(Other.PolyRef); }

	FORCEINLINE float GetHeuristicCost() const { return TotalCost - Cost; }
};

namespace ERecastDebugPathfindingFlags
{
	enum Type
	{
		Basic = 0x0,
		BestNode = 0x1,
		Vertices = 0x2,
		PathLength = 0x4
	};
}

struct FRecastDebugPathfindingData
{
	TSet<FRecastDebugPathfindingNode> Nodes;
	FSetElementId BestNode;
	uint8 Flags;

	FRecastDebugPathfindingData() : Flags(ERecastDebugPathfindingFlags::Basic) {}
	FRecastDebugPathfindingData(ERecastDebugPathfindingFlags::Type InFlags) : Flags(InFlags) {}
};

struct FRecastDebugGeometry
{
	enum EOffMeshLinkEnd
	{
		OMLE_None = 0x0,
		OMLE_Left = 0x1,
		OMLE_Right = 0x2,
		OMLE_Both = OMLE_Left | OMLE_Right
	};

	struct FOffMeshLink
	{
		FVector Left;
		FVector Right;
		uint8	AreaID;
		uint8	Direction;
		uint8	ValidEnds;
		float	Radius;
		float	Height;
		FColor	Color;
	};

	struct FCluster
	{
		TArray<int32> MeshIndices;
	};

	struct FClusterLink
	{
		FVector FromCluster;
		FVector ToCluster;
	};

	struct FOffMeshSegment
	{
		FVector LeftStart, LeftEnd;
		FVector RightStart, RightEnd;
		uint8	AreaID;
		uint8	Direction;
		uint8	ValidEnds;
	};

	TArray<FVector> MeshVerts;
	TArray<int32> AreaIndices[RECAST_MAX_AREAS];
	TArray<int32> BuiltMeshIndices;
	TArray<FVector> PolyEdges;
	TArray<FVector> NavMeshEdges;
	TArray<FOffMeshLink> OffMeshLinks;
	TArray<FCluster> Clusters;
	TArray<FClusterLink> ClusterLinks;
	TArray<FOffMeshSegment> OffMeshSegments;
	TArray<int32> OffMeshSegmentAreas[RECAST_MAX_AREAS];

	int32 bGatherPolyEdges : 1;
	int32 bGatherNavMeshEdges : 1;

	FRecastDebugGeometry() : bGatherPolyEdges(false), bGatherNavMeshEdges(false)
	{}

	uint32 ENGINE_API GetAllocatedSize() const;
};

struct FNavPoly
{
	NavNodeRef Ref;
	FVector Center;
};

namespace ERecastNamedFilter
{
	enum Type 
	{
		FilterOutNavLinks = 0,		// filters out all off-mesh connections
		FilterOutAreas,				// filters out all navigation areas except the default one (RECAST_DEFAULT_AREA)
		FilterOutNavLinksAndAreas,	// combines FilterOutNavLinks and FilterOutAreas

		NamedFiltersCount,
	};
}
#endif //WITH_RECAST


/**
 *	Structure to handle nav mesh tile's raw data persistence and releasing
 */
struct FNavMeshTileData
{
	// helper function so that we release NavData via dtFree not regular delete (for navigation mem stats)
	struct FNavData
	{
		uint8* RawNavData;

		FNavData(uint8* InNavData) : RawNavData(InNavData) {}
		~FNavData();
	};
	
	// layer index
	int32	LayerIndex;
	FBox	LayerBBox;
	// size of allocated data
	int32	DataSize;
	// actual tile data
	TSharedPtr<FNavData, ESPMode::ThreadSafe> NavData;
	
	FNavMeshTileData() : LayerIndex(0), DataSize(0) { }
	~FNavMeshTileData();
	
	explicit FNavMeshTileData(uint8* RawData, int32 RawDataSize, int32 LayerIdx = 0, FBox LayerBounds = FBox(ForceInit));
		
	FORCEINLINE uint8* GetData()
	{
		check(NavData.IsValid());
		return NavData->RawNavData;
	}

	FORCEINLINE const uint8* GetData() const
	{
		check(NavData.IsValid());
		return NavData->RawNavData;
	}

	FORCEINLINE uint8* GetDataSafe()
	{
		return NavData.IsValid() ? NavData->RawNavData : NULL;
	}

	FORCEINLINE bool operator==(const uint8* RawData) const
	{
		return GetData() == RawData;
	}

	FORCEINLINE bool IsValid() const { return NavData.IsValid() && GetData() != nullptr && DataSize > 0; }

	uint8* Release();

	// Duplicate shared state so we will have own copy of the data
	void MakeUnique();
};

DECLARE_MULTICAST_DELEGATE(FOnNavMeshUpdate);

namespace FNavMeshConfig
{
	struct FRecastNamedFiltersCreator
	{
		FRecastNamedFiltersCreator(bool bVirtualFilters);
	};
}


UCLASS(config=Engine, defaultconfig, hidecategories=(Input,Rendering,Tags,Transform,"Utilities|Transformation",Actor,Layers,Replication), notplaceable)
class ENGINE_API ARecastNavMesh : public ANavigationData
{
	GENERATED_UCLASS_BODY()

	typedef uint16 FNavPolyFlags;

	virtual void Serialize( FArchive& Ar ) override;

	/** should we draw edges of every navmesh's triangle */
	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawTriangleEdges:1;

	/** should we draw edges of every poly (i.e. not only border-edges)  */
	UPROPERTY(EditAnywhere, Category=Display, config)
	uint32 bDrawPolyEdges:1;

	/** if disabled skips filling drawn navmesh polygons */
	UPROPERTY(EditAnywhere, Category = Display)
	uint32 bDrawFilledPolys:1;

	/** should we draw border-edges */
	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawNavMeshEdges:1;

	/** should we draw the tile boundaries */
	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawTileBounds:1;
	
	/** Draw input geometry passed to the navmesh generator.  Recommend disabling other geometry rendering via viewport showflags in editor. */
	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawPathCollidingGeometry:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawTileLabels:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawPolygonLabels:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawDefaultPolygonCost:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawLabelsOnPathNodes:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawNavLinks:1;

	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawFailedNavLinks:1;
	
	UPROPERTY(EditAnywhere, Category=Display)
	uint32 bDrawClusters:1;

	/** should we draw edges of every navmesh's triangle */
	UPROPERTY(EditAnywhere, Category = Display)
	uint32 bDrawOctree : 1;

	UPROPERTY(config)
	uint32 bDistinctlyDrawTilesBeingBuilt:1;

	UPROPERTY(EditAnywhere, Category = Display)
	uint32 bDrawNavMesh : 1;

	/** vertical offset added to navmesh's debug representation for better readability */
	UPROPERTY(EditAnywhere, Category=Display, config)
	float DrawOffset;
	
	//----------------------------------------------------------------------//
	// NavMesh generation parameters
	//----------------------------------------------------------------------//

	/** if true, the NavMesh will allocate fixed size pool for tiles, should be enabled to support streaming */
	UPROPERTY(EditAnywhere, Category=Generation, config)
	uint32 bFixedTilePoolSize:1;

	/** maximum number of tiles NavMesh can hold */
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(editcondition = "bFixedTilePoolSize"))
	int32 TilePoolSize;

	/** size of single tile, expressed in uu */
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(ClampMin = "300.0"))
	float TileSizeUU;

	/** horizontal size of voxelization cell */
	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "1.0", ClampMax = "1024.0"))
	float CellSize;

	/** vertical size of voxelization cell */
	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "1.0", ClampMax = "1024.0"))
	float CellHeight;

	/** Radius of smallest agent to traverse this navmesh */
	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "0.0"))
	float AgentRadius;

	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "0.0"))
	float AgentHeight;

	/** Size of the tallest agent that will path with this navmesh. */
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(ClampMin = "0.0"))
	float AgentMaxHeight;

	/* The maximum slope (angle) that the agent can move on. */ 
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0" ))
	float AgentMaxSlope;

	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "0.0"))
	float AgentMaxStepHeight;

	/* The minimum dimension of area. Areas smaller than this will be discarded */
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(ClampMin = "0.0"))
	float MinRegionArea;

	/* The size limit of regions to be merged with bigger regions (watershed partitioning only) */
	UPROPERTY(EditAnywhere, Category=Generation, config, meta=(ClampMin = "0.0"))
	float MergeRegionSize;

	/** How much navigable shapes can get simplified - the higher the value the more freedom */
	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "0.0"))
	float MaxSimplificationError;

	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "0", UIMin = "0"), AdvancedDisplay)
	int32 MaxSimultaneousTileGenerationJobsCount;

	/** Absolute hard limit to number of navmesh tiles. Be very, very careful while modifying it while
	 *	having big maps with navmesh. A single, empty tile takes 176 bytes and empty tiles are
	 *	allocated up front (subject to change, but that's where it's at now)
	 *	@note TileNumberHardLimit is always rounded up to the closest power of 2 */
	UPROPERTY(EditAnywhere, Category = Generation, config, meta = (ClampMin = "1", UIMin = "1"), AdvancedDisplay)
	int32 TileNumberHardLimit;

	UPROPERTY(VisibleAnywhere, Category = Generation, AdvancedDisplay)
	int32 PolyRefTileBits;

	UPROPERTY(VisibleAnywhere, Category = Generation, AdvancedDisplay)
	int32 PolyRefNavPolyBits;

	UPROPERTY(VisibleAnywhere, Category = Generation, AdvancedDisplay)
	int32 PolyRefSaltBits;

	/** navmesh draw distance in game (always visible in editor) */
	UPROPERTY(config)
	float DefaultDrawDistance;

	/** specifes default limit to A* nodes used when performing navigation queries. 
	 *	Can be overridden by passing custom FNavigationQueryFilter */
	UPROPERTY(config)
	float DefaultMaxSearchNodes;

	/** specifes default limit to A* nodes used when performing hierarchical navigation queries. */
	UPROPERTY(config)
	float DefaultMaxHierarchicalSearchNodes;

	/** partitioning method for creating navmesh polys */
	UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
	TEnumAsByte<ERecastPartitioning::Type> RegionPartitioning;

	/** partitioning method for creating tile layers */
	UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
	TEnumAsByte<ERecastPartitioning::Type> LayerPartitioning;

	/** number of chunk splits (along single axis) used for region's partitioning: ChunkyMonotone */
	UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
	int32 RegionChunkSplits;

	/** number of chunk splits (along single axis) used for layer's partitioning: ChunkyMonotone */
	UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
	int32 LayerChunkSplits;

	/** Controls whether Navigation Areas will be sorted by cost before application 
	 *	to navmesh during navmesh generation. This is relevant then there are
	 *	areas overlapping and we want to have area cost express area relevancy
	 *	as well. Setting it to true will result in having area sorted by cost,
	 *	but it will also increase navmesh generation cost a bit */
	UPROPERTY(EditAnywhere, Category=Generation, config)
	uint32 bSortNavigationAreasByCost:1;

	/** controls whether voxel filterring will be applied (via FRecastTileGenerator::ApplyVoxelFilter). 
	 *	Results in generated navemesh better fitting navigation bounds, but hits (a bit) generation performance */
	UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
	uint32 bPerformVoxelFiltering:1;

	/** mark areas with insufficient free height above instead of cutting them out  */
	UPROPERTY(EditAnywhere, Category = Generation, config, AdvancedDisplay)
	uint32 bMarkLowHeightAreas : 1;

	UPROPERTY(EditAnywhere, Category = Generation, config, AdvancedDisplay)
	uint32 bDoFullyAsyncNavDataGathering : 1;
	
	/** TODO: switch to disable new code from OffsetFromCorners if necessary - remove it later */
	UPROPERTY(config)
	uint32 bUseBetterOffsetsFromCorners : 1;

	/** If set, tiles generated without any navmesh data will be marked to distinguish them from not generated / streamed out ones. Defaults to false. */
	UPROPERTY(config)
	uint32 bStoreEmptyTileLayers : 1;

	/** Indicates whether default navigation filters will use virtual functions. Defaults to true. */
	UPROPERTY(config)
	uint32 bUseVirtualFilters : 1;

	/** If set, paths can end at navlink poly (not the ground one!) */
	UPROPERTY(config)
	uint32 bAllowNavLinkAsPathEnd : 1;

private:
	/** Cache rasterized voxels instead of just collision vertices/indices in navigation octree */
	UPROPERTY(config)
	uint32 bUseVoxelCache : 1;

	/** indicates how often we will sort navigation tiles to mach players position */
	UPROPERTY(config)
	float TileSetUpdateInterval;
	
	/** contains last available dtPoly's flag bit set (8th bit at the moment of writing) */
	static FNavPolyFlags NavLinkFlag;

	/** Squared draw distance */
	static float DrawDistanceSq;

public:

	struct FRaycastResult
	{
		enum 
		{
			MAX_PATH_CORRIDOR_POLYS = 128
		};

		NavNodeRef CorridorPolys[MAX_PATH_CORRIDOR_POLYS];
		float CorridorCost[MAX_PATH_CORRIDOR_POLYS];
		int32 CorridorPolysCount;
		float HitTime;
		FVector HitNormal;
		uint32 bIsRaycastEndInCorridor : 1;

		FRaycastResult()
			: CorridorPolysCount(0)
			, HitTime(FLT_MAX)
			, HitNormal(0.f)
			, bIsRaycastEndInCorridor(false)
		{
			FMemory::Memzero(CorridorPolys);
			FMemory::Memzero(CorridorCost);
		}

		FORCEINLINE int32 GetMaxCorridorSize() const { return MAX_PATH_CORRIDOR_POLYS; }
		FORCEINLINE bool HasHit() const { return HitTime != FLT_MAX; }
		FORCEINLINE NavNodeRef GetLastNodeRef() const { return CorridorPolysCount > 0 ? CorridorPolys[CorridorPolysCount - 1] : INVALID_NAVNODEREF; }
	};

	//----------------------------------------------------------------------//
	// Recast runtime params
	//----------------------------------------------------------------------//
	/** Euclidean distance heuristic scale used while pathfinding */
	UPROPERTY(EditAnywhere, Category = Query, config, meta = (ClampMin = "0.1"))
	float HeuristicScale;

	/** Value added to each search height to compensate for error between navmesh polys and walkable geometry  */
	UPROPERTY(EditAnywhere, Category = Query, config, meta = (ClampMin = "0.0"))
	float VerticalDeviationFromGroundCompensation;

	/** broadcast for navmesh updates */
	FOnNavMeshUpdate OnNavMeshUpdate;

	FORCEINLINE static void SetDrawDistance(float NewDistance) { DrawDistanceSq = NewDistance * NewDistance; }
	FORCEINLINE static float GetDrawDistanceSq() { return DrawDistanceSq; }

	//////////////////////////////////////////////////////////////////////////

	bool HasValidNavmesh() const;

#if WITH_RECAST
private:
	//----------------------------------------------------------------------//
	// Life cycle & serialization
	//----------------------------------------------------------------------//
	/** scans the world and creates appropriate RecastNavMesh instances */
	static ANavigationData* CreateNavigationInstances(UNavigationSystem* NavSys);
public:
	/** Dtor */
	virtual ~ARecastNavMesh();

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

#if WITH_EDITOR
	/** RecastNavMesh instances are dynamically spawned and should not be coppied */
	virtual bool ShouldExport() override { return false; }
#endif
	
	virtual void CleanUp() override;

	//~ Begin ANavigationData Interface
	virtual FNavLocation GetRandomPoint(FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;
	virtual bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;
	virtual bool GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;

	virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;
	
	/** Project batch of points using shared search extent and filter */
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;

	/** Project batch of points using shared search filter. This version is not requiring user to pass in Extent, 
	 *	and is instead relying on FNavigationProjectionWork.ProjectionLimit.
	 *	@note function will assert if item's FNavigationProjectionWork.ProjectionLimit is invalid */
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;
	
	virtual ENavigationQueryResult::Type CalcPathCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override;
	virtual ENavigationQueryResult::Type CalcPathLength(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const override;
	virtual ENavigationQueryResult::Type CalcPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, float& OutPathCost, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const override;
	virtual bool DoesNodeContainLocation(NavNodeRef NodeRef, const FVector& WorldSpaceLocation) const override;

	virtual UPrimitiveComponent* ConstructRenderingComponent() override;
	/** Returns bounding box for the navmesh. */
	virtual FBox GetBounds() const override { return GetNavMeshBounds(); }
	/** Called on world origin changes **/
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;

	virtual void OnStreamingLevelAdded(ULevel* InLevel, UWorld* InWorld) override;
	virtual void OnStreamingLevelRemoved(ULevel* InLevel, UWorld* InWorld) override;
	//~ End ANavigationData Interface

protected:
	/** Serialization helper. */
	void SerializeRecastNavMesh(FArchive& Ar, FPImplRecastNavMesh*& NavMesh, int32 NavMeshVersion);

	TArray<FIntPoint>& GetActiveTiles(); 
	virtual void RestrictBuildingToActiveTiles(bool InRestrictBuildingToActiveTiles) override;

public:
	/** Whether NavMesh should adjust his tile pool size when NavBounds are changed */
	bool IsResizable() const;

	/** Returns bounding box for the whole navmesh. */
	FBox GetNavMeshBounds() const;

	/** Returns bounding box for a given navmesh tile. */
	FBox GetNavMeshTileBounds(int32 TileIndex) const;

	/** Retrieves XY coordinates of tile specified by index */
	bool GetNavMeshTileXY(int32 TileIndex, int32& OutX, int32& OutY, int32& Layer) const;

	/** Retrieves XY coordinates of tile specified by position */
	bool GetNavMeshTileXY(const FVector& Point, int32& OutX, int32& OutY) const;

	/** Retrieves all tile indices at matching XY coordinates */
	void GetNavMeshTilesAt(int32 TileX, int32 TileY, TArray<int32>& Indices) const;

	/** Retrieves number of tiles in this navmesh */
	int32 GetNavMeshTilesCount() const;

	/** Removes compressed tile data at given tile coord */
	void RemoveTileCacheLayers(int32 TileX, int32 TileY);
	
	/** Stores compressed tile data for given tile coord */
	void AddTileCacheLayers(int32 TileX, int32 TileY, const TArray<FNavMeshTileData>& InLayers);

	/** Marks tile coord as rebuild and empty */
	void MarkEmptyTileCacheLayers(int32 TileX, int32 TileY);
	
	/** Returns compressed tile data at given tile coord */
	TArray<FNavMeshTileData> GetTileCacheLayers(int32 TileX, int32 TileY) const;
	
	void GetEdgesForPathCorridor(const TArray<NavNodeRef>* PathCorridor, TArray<struct FNavigationPortalEdge>* PathCorridorEdges) const;

	void UpdateDrawing();

	/** Creates a task to be executed on GameThread calling UpdateDrawing */
	void RequestDrawingUpdate(bool bForce = false);

	/** called after regenerating tiles */
	virtual void OnNavMeshTilesUpdated(const TArray<uint32>& ChangedTiles);

	/** Event from generator that navmesh build has finished */
	virtual void OnNavMeshGenerationFinished();

	virtual void EnsureBuildCompletion() override;

	virtual void SetConfig(const FNavDataConfig& Src) override;
protected:
	virtual void FillConfig(FNavDataConfig& Dest) override;

	FORCEINLINE const FNavigationQueryFilter& GetRightFilterRef(FSharedConstNavQueryFilter Filter) const 
	{
		return *(Filter.IsValid() ? Filter.Get() : GetDefaultQueryFilter().Get());
	}

public:

	static bool IsVoxelCacheEnabled();

	//----------------------------------------------------------------------//
	// Debug                                                                
	//----------------------------------------------------------------------//
	/** Debug rendering support. */
	void GetDebugGeometry(FRecastDebugGeometry& OutGeometry, int32 TileIndex = INDEX_NONE) const;

	// @todo docuement
	void DrawDebugPathCorridor(NavNodeRef const* PathPolys, int32 NumPathPolys, bool bPersistent=true) const;

#if !UE_BUILD_SHIPPING
	virtual uint32 LogMemUsed() const override;
#endif // !UE_BUILD_SHIPPING

	void UpdateNavMeshDrawing();

	//----------------------------------------------------------------------//
	// Utilities
	//----------------------------------------------------------------------//
	virtual void OnNavAreaChanged() override;
	virtual void OnNavAreaAdded(const UClass* NavAreaClass, int32 AgentIndex) override;
	virtual int32 GetNewAreaID(const UClass* AreaClass) const override;
	virtual int32 GetMaxSupportedAreas() const override { return RECAST_MAX_AREAS; }

	/** Get forbidden area flags from default query filter */
	uint16 GetDefaultForbiddenFlags() const;
	/** Change forbidden area flags in default query filter */
	void SetDefaultForbiddenFlags(uint16 ForbiddenAreaFlags);

	/** Area sort function */
	virtual void SortAreasForGenerator(TArray<FRecastAreaNavModifierElement>& Areas) const;

	void RecreateDefaultFilter();

	int32 GetMaxSimultaneousTileGenerationJobsCount() const { return MaxSimultaneousTileGenerationJobsCount; }
	void SetMaxSimultaneousTileGenerationJobsCount(int32 NewJobsCountLimit);

	/** Returns query extent including adjustments for voxelization error compensation */
	FVector GetModifiedQueryExtent(const FVector& QueryExtent) const
	{
		return FVector(QueryExtent.X, QueryExtent.Y, QueryExtent.Z + FMath::Max(0.0f, VerticalDeviationFromGroundCompensation));
	}

	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//

	virtual void UpdateCustomLink(const INavLinkCustomInterface* CustomLink) override;

	/** update area class and poly flags for all offmesh links with given UserId */
	void UpdateNavigationLinkArea(int32 UserId, TSubclassOf<UNavArea> AreaClass) const;

	/** update area class and poly flags for all offmesh segment links with given UserId */
	void UpdateSegmentLinkArea(int32 UserId, TSubclassOf<UNavArea> AreaClass) const;

	//----------------------------------------------------------------------//
	// Batch processing (important with async rebuilding)
	//----------------------------------------------------------------------//

	/** Starts batch processing and locks access to navmesh from other threads */
	virtual void BeginBatchQuery() const override;

	/** Finishes batch processing and release locks */
	virtual void FinishBatchQuery() const override;

	//----------------------------------------------------------------------//
	// Querying                                                                
	//----------------------------------------------------------------------//
	
	/** dtNavMesh getter */
	dtNavMesh* GetRecastMesh();

	/** dtNavMesh getter */
	const dtNavMesh* GetRecastMesh() const;

	/** Retrieves LinkUserID associated with indicated PolyID */
	uint32 GetLinkUserId(NavNodeRef LinkPolyID) const;

	FColor GetAreaIDColor(uint8 AreaID) const;

	/** Returns nearest navmesh polygon to Loc, or INVALID_NAVMESHREF if Loc is not on the navmesh. */
	NavNodeRef FindNearestPoly(FVector const& Loc, FVector const& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const;

	/** Finds the distance to the closest wall, limited to MaxDistance
	 *	[out] OutClosestPointOnWall, if supplied, will be set to closest point on closest wall. Will not be set if no wall in the area (return value 0.f) */
	float FindDistanceToWall(const FVector& StartLoc, FSharedConstNavQueryFilter Filter = nullptr, float MaxDistance = FLT_MAX, FVector* OutClosestPointOnWall = nullptr) const;

	/** Retrieves center of the specified polygon. Returns false on error. */
	bool GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const;

	/** Retrieves the vertices for the specified polygon. Returns false on error. */
	bool GetPolyVerts(NavNodeRef PolyID, TArray<FVector>& OutVerts) const;

	/** Retrieves area ID for the specified polygon. */
	uint32 GetPolyAreaID(NavNodeRef PolyID) const;

	/** Sets area ID for the specified polygon. */
	bool SetPolyArea(NavNodeRef PolyID, TSubclassOf<UNavArea> AreaClass);

	/** Sets area ID for the specified polygons */
	void SetPolyArrayArea(const TArray<FNavPoly>& Polys, TSubclassOf<UNavArea> AreaClass);

	/** Retrieves poly and area flags for specified polygon */
	bool GetPolyFlags(NavNodeRef PolyID, uint16& PolyFlags, uint16& AreaFlags) const;
	bool GetPolyFlags(NavNodeRef PolyID, FNavMeshNodeFlags& Flags) const;

	/** Finds all polys connected with specified one */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const;

	/** Finds all polys connected with specified one, results expressed as array of NavNodeRefs */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<NavNodeRef>& Neighbors) const;

	/** Finds edges of specified poly */
	bool GetPolyEdges(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const;

	/** Finds closest point constrained to given poly */
	bool GetClosestPointOnPoly(NavNodeRef PolyID, const FVector& TestPt, FVector& PointOnPoly) const;

	/** Decode poly ID into tile index and poly index */
	bool GetPolyTileIndex(NavNodeRef PolyID, uint32& PolyIndex, uint32& TileIndex) const;

	/** Retrieves start and end point of offmesh link */
	bool GetLinkEndPoints(NavNodeRef LinkPolyID, FVector& PointA, FVector& PointB) const;

	/** Retrieves bounds of cluster. Returns false on error. */
	bool GetClusterBounds(NavNodeRef ClusterRef, FBox& OutBounds) const;

	/** Get random point in given cluster */
	bool GetRandomPointInCluster(NavNodeRef ClusterRef, FNavLocation& OutLocation) const;

	/** Get cluster ref containing given poly ref */
	NavNodeRef GetClusterRef(NavNodeRef PolyRef) const;

	/** Retrieves all polys within given pathing distance from StartLocation.
	 *	@NOTE query is not using string-pulled path distance (for performance reasons),
	 *		it measured distance between middles of portal edges, do you might want to 
	 *		add an extra margin to PathingDistance */
	bool GetPolysWithinPathingDistance(FVector const& StartLoc, const float PathingDistance, TArray<NavNodeRef>& FoundPolys,
		FSharedConstNavQueryFilter Filter = nullptr, const UObject* Querier = nullptr, FRecastDebugPathfindingData* DebugData = nullptr) const;

	/** Filters nav polys in PolyRefs with Filter */
	bool FilterPolys(TArray<NavNodeRef>& PolyRefs, const FRecastQueryFilter* Filter, const UObject* Querier = NULL) const;

	/** Get all polys from tile */
	bool GetPolysInTile(int32 TileIndex, TArray<FNavPoly>& Polys) const;

	/** Get all polys that overlap the specified box */
	bool GetPolysInBox(const FBox& Box, TArray<FNavPoly>& Polys, FSharedConstNavQueryFilter Filter = nullptr, const UObject* Owner = nullptr) const;

	/** Get all polys from tile */
	bool GetNavLinksInTile(const int32 TileIndex, TArray<FNavPoly>& Polys, const bool bIncludeLinksFromNeighborTiles) const;

	/** Projects point on navmesh, returning all hits along vertical line defined by min-max Z params */
	bool ProjectPointMulti(const FVector& Point, TArray<FNavLocation>& OutLocations, const FVector& Extent,
		float MinZ, float MaxZ, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const;
	
	// @todo docuement
	static FPathFindingResult FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query);
	static bool TestPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes);
	static bool TestHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes);
	static bool NavMeshRaycast(const ANavigationData* Self, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier, FRaycastResult& Result);
	static bool NavMeshRaycast(const ANavigationData* Self, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL);
	static bool NavMeshRaycast(const ANavigationData* Self, NavNodeRef RayStartNode, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL);

	virtual void BatchRaycast(TArray<FNavigationRaycastWork>& Workload, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL) const override;

	/** finds a Filter-passing navmesh location closest to specified StartLoc
	 *	@return true if adjusting was required, false otherwise */
	bool AdjustLocationWithFilter(const FVector& StartLoc, FVector& OutAdjustedLocation, const FNavigationQueryFilter& Filter, const UObject* Querier = NULL) const;
	
	/** Check if navmesh is defined (either built/streamed or recognized as empty tile by generator) in given radius.
	  * @returns true if ALL tiles inside are ready
	  */
	bool HasCompleteDataInRadius(const FVector& TestLocation, float TestRadius) const;

	/** @return true is specified segment is fully on navmesh (respecting the optional filter) */
	bool IsSegmentOnNavmesh(const FVector& SegmentStart, const FVector& SegmentEnd, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const;

	/** Check if poly is a custom link */
	bool IsCustomLink(NavNodeRef PolyRef) const;

	/** finds stringpulled path from given corridor */
	bool FindStraightPath(const FVector& StartLoc, const FVector& EndLoc, const TArray<NavNodeRef>& PathCorridor, TArray<FNavPathPoint>& PathPoints, TArray<uint32>* CustomLinks = NULL) const;

	/** Runs A* pathfinding on navmesh and collect data for every step */
	int32 DebugPathfinding(const FPathFindingQuery& Query, TArray<FRecastDebugPathfindingData>& Steps);

	static const FRecastQueryFilter* GetNamedFilter(ERecastNamedFilter::Type FilterType);
	FORCEINLINE static FNavPolyFlags GetNavLinkFlag() { return NavLinkFlag; }
	
	virtual bool NeedsRebuild() const override;
	virtual bool SupportsRuntimeGeneration() const override;
	virtual bool SupportsStreaming() const override;
	virtual void ConditionalConstructGenerator() override;
	bool ShouldGatherDataOnGameThread() const { return bDoFullyAsyncNavDataGathering == false; }
	int32 GetTileNumberHardLimit() const { return TileNumberHardLimit; }

	void UpdateActiveTiles(const TArray<FNavigationInvokerRaw>& InvokerLocations);
	void RemoveTiles(const TArray<FIntPoint>& Tiles);
	void RebuildTile(const TArray<FIntPoint>& Tiles);

protected:

	void UpdatePolyRefBitsPreview();
	
	/** Invalidates active paths that go through changed tiles  */
	void InvalidateAffectedPaths(const TArray<uint32>& ChangedTiles);

	/** Spawns an ARecastNavMesh instance, and configures it if AgentProps != NULL */
	static ARecastNavMesh* SpawnInstance(UNavigationSystem* NavSys, const FNavDataConfig* AgentProps = NULL);

private:
	friend class FRecastNavMeshGenerator;
	friend class FPImplRecastNavMesh;
	// destroys FPImplRecastNavMesh instance if it has been created 
	void DestroyRecastPImpl();
	// @todo docuement
	void UpdateNavVersion();
	void UpdateNavObject();

	/** @return Navmesh data chunk that belongs to this actor */
	URecastNavMeshDataChunk* GetNavigationDataChunk(ULevel* InLevel) const;

protected:
	// retrieves RecastNavMeshImpl
	FPImplRecastNavMesh* GetRecastNavMeshImpl() { return RecastNavMeshImpl; }
	const FPImplRecastNavMesh* GetRecastNavMeshImpl() const { return RecastNavMeshImpl; }

private:
	/** NavMesh versioning. */
	uint32 NavMeshVersion;
	
	/** 
	 * This is a pimpl-style arrangement used to tightly hide the Recast internals from the rest of the engine.
	 * Using this class should *not* require the inclusion of the private RecastNavMesh.h
	 *	@NOTE: if we switch over to C++11 this should be unique_ptr
	 *	@TODO since it's no secret we're using recast there's no point in having separate implementation class. FPImplRecastNavMesh should be merged into ARecastNavMesh
	 */
	FPImplRecastNavMesh* RecastNavMeshImpl;
	
#if RECAST_ASYNC_REBUILDING
	/** batch query counter */
	mutable int32 BatchQueryCounter;

#endif // RECAST_ASYNC_REBUILDING

private:
	static const FRecastQueryFilter* NamedFilters[ERecastNamedFilter::NamedFiltersCount];

	// DEPRECATED FUNCTIONS
public:
	DEPRECATED(4.11, "Function was changed to SetPolyArea, using AreaClass pointer instead of Id")
	void SetPolyAreaID(NavNodeRef PolyID, uint8 AreaID);
	DEPRECATED(4.11, "Function was changed to SetPolyArrayArea, using AreaClass pointer instead of Id")
	void SetPolyArrayAreaID(const TArray<FNavPoly>& Polys, uint8 AreaID);

#endif // WITH_RECAST
};

#if WITH_RECAST
FORCEINLINE
bool ARecastNavMesh::NavMeshRaycast(const ANavigationData* Self, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier)
{
	FRaycastResult Result;
	return NavMeshRaycast(Self, RayStart, RayEnd, HitLocation, QueryFilter, Querier, Result);
}


/** structure to cache owning RecastNavMesh data so that it doesn't have to be polled
 *	directly from RecastNavMesh while asyncronously generating navmesh */
struct FRecastNavMeshCachedData
{
	ARecastNavMesh::FNavPolyFlags FlagsPerArea[RECAST_MAX_AREAS];
	ARecastNavMesh::FNavPolyFlags FlagsPerOffMeshLinkArea[RECAST_MAX_AREAS];
	TMap<const UClass*, int32> AreaClassToIdMap;
	const ARecastNavMesh* ActorOwner;
	uint32 bUseSortFunction : 1;

	static FRecastNavMeshCachedData Construct(const ARecastNavMesh* RecastNavMeshActor);
	void OnAreaAdded(const UClass* AreaClass, int32 AreaID);
};

#endif // WITH_RECAST
