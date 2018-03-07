// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// Private implementation for communication with Recast library
// 
// All functions should be called through RecastNavMesh actor to make them thread safe!
//

#pragma once 

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/RecastQueryFilter.h"

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"
#endif

class FRecastNavMeshGenerator;
class UNavigationSystem;

#if WITH_RECAST

#define RECAST_VERY_SMALL_AGENT_RADIUS 0.0f

/** Engine Private! - Private Implementation details of ARecastNavMesh */
class ENGINE_API FPImplRecastNavMesh
{
public:

	/** Constructor */
	FPImplRecastNavMesh(ARecastNavMesh* Owner);

	/** Dtor */
	~FPImplRecastNavMesh();

	/**
	 * Serialization.
	 * @param Ar - The archive with which to serialize.
	 * @returns true if serialization was successful.
	 */
	void Serialize(FArchive& Ar, int32 NavMeshVersion);

	/** Debug rendering. */
	void GetDebugGeometry(FRecastDebugGeometry& OutGeometry, int32 TileIndex = INDEX_NONE) const;
	
	/** Returns bounding box for the whole navmesh. */
	FBox GetNavMeshBounds() const;

	/** Returns bounding box for a given navmesh tile. */
	FBox GetNavMeshTileBounds(int32 TileIndex) const;

	/** Retrieves XY and layer coordinates of tile specified by index */
	bool GetNavMeshTileXY(int32 TileIndex, int32& OutX, int32& OutY, int32& OutLayer) const;

	/** Retrieves XY coordinates of tile specified by position */
	bool GetNavMeshTileXY(const FVector& Point, int32& OutX, int32& OutY) const;

	/** Retrieves all tile indices at matching XY coordinates */
	void GetNavMeshTilesAt(int32 TileX, int32 TileY, TArray<int32>& Indices) const;

	/** Retrieves list of tiles that intersect specified bounds */
	void GetNavMeshTilesIn(const TArray<FBox>& InclusionBounds, TArray<int32>& Indices) const;

	/** Retrieves number of tiles in this navmesh */
	FORCEINLINE int32 GetNavMeshTilesCount() const { return DetourNavMesh ? DetourNavMesh->getMaxTiles() : 0; }

	/** Supported queries */

	// @TODONAV
	/** Generates path from the given query. Synchronous. */
	ENavigationQueryResult::Type FindPath(const FVector& StartLoc, const FVector& EndLoc, FNavMeshPath& Path, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Check if path exists */
	ENavigationQueryResult::Type TestPath(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& Filter, const UObject* Owner, int32* NumVisitedNodes = 0) const;

	/** Check if path exists using cluster graph */
	ENavigationQueryResult::Type TestClusterPath(const FVector& StartLoc, const FVector& EndLoc, int32* NumVisitedNodes = 0) const;

	/** Checks if the whole segment is in navmesh */
	void Raycast(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner,
		ARecastNavMesh::FRaycastResult& RaycastResult, NavNodeRef StartNode = INVALID_NAVNODEREF) const;

	/** Generates path from given query and collect data for every step of A* algorithm */
	int32 DebugPathfinding(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& Filter, const UObject* Owner, TArray<FRecastDebugPathfindingData>& Steps);

	/** Returns a random location on the navmesh. */
	FNavLocation GetRandomPoint(const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Returns a random location on the navmesh within cluster */
	bool GetRandomPointInCluster(NavNodeRef ClusterRef, FNavLocation& OutLocation) const;

	bool ProjectPointToNavMesh(const FVector& Point, FNavLocation& Result, const FVector& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const;
	
	/** Project single point and grab all vertical intersections */
	bool ProjectPointMulti(const FVector& Point, TArray<FNavLocation>& OutLocations, const FVector& Extent,
		float MinZ, float MaxZ, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Returns nearest navmesh polygon to Loc, or INVALID_NAVMESHREF if Loc is not on the navmesh. */
	NavNodeRef FindNearestPoly(FVector const& Loc, FVector const& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const;

	/** Retrieves all polys within given pathing distance from StartLocation.
	 *	@NOTE query is not using string-pulled path distance (for performance reasons),
	 *		it measured distance between middles of portal edges, do you might want to 
	 *		add an extra margin to PathingDistance */
	bool GetPolysWithinPathingDistance(FVector const& StartLoc, const float PathingDistance,
		const FNavigationQueryFilter& Filter, const UObject* Owner,
		TArray<NavNodeRef>& FoundPolys, FRecastDebugPathfindingData* DebugData) const;

	//@todo document
	void GetEdgesForPathCorridor(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges) const;

	/** finds stringpulled path from given corridor */
	bool FindStraightPath(const FVector& StartLoc, const FVector& EndLoc, const TArray<NavNodeRef>& PathCorridor, TArray<FNavPathPoint>& PathPoints, TArray<uint32>* CustomLinks = NULL) const;

	/** Filters nav polys in PolyRefs with Filter */
	bool FilterPolys(TArray<NavNodeRef>& PolyRefs, const FRecastQueryFilter* Filter, const UObject* Owner) const;

	/** Get all polys from tile */
	bool GetPolysInTile(int32 TileIndex, TArray<FNavPoly>& Polys) const;

	/** Updates area on polygons creating point-to-point connection with given UserId */
	void UpdateNavigationLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const;
	/** Updates area on polygons creating segment-to-segment connection with given UserId */
	void UpdateSegmentLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const;

	/** Retrieves center of the specified polygon. Returns false on error. */
	bool GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const;
	/** Retrieves the vertices for the specified polygon. Returns false on error. */
	bool GetPolyVerts(NavNodeRef PolyID, TArray<FVector>& OutVerts) const;
	/** Retrieves the flags for the specified polygon. Returns false on error. */
	bool GetPolyData(NavNodeRef PolyID, uint16& Flags, uint8& AreaType) const;
	/** Retrieves area ID for the specified polygon. */
	uint32 GetPolyAreaID(NavNodeRef PolyID) const;
	/** Sets area ID for the specified polygon. */
	void SetPolyAreaID(NavNodeRef PolyID, uint8 AreaID);
	/** Finds all polys connected with specified one */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const;
	/** Finds all polys connected with specified one, results expressed as array of NavNodeRefs */
	bool GetPolyNeighbors(NavNodeRef PolyID, TArray<NavNodeRef>& Neighbors) const;
	/** Finds all polys connected with specified one */
	bool GetPolyEdges(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Edges) const;
	/** Finds closest point constrained to given poly */
	bool GetClosestPointOnPoly(NavNodeRef PolyID, const FVector& TestPt, FVector& PointOnPoly) const;
	/** Decode poly ID into tile index and poly index */
	bool GetPolyTileIndex(NavNodeRef PolyID, uint32& PolyIndex, uint32& TileIndex) const;
	/** Retrieves user ID for given offmesh link poly */
	uint32 GetLinkUserId(NavNodeRef LinkPolyID) const;
	/** Retrieves start and end point of offmesh link */
	bool GetLinkEndPoints(NavNodeRef LinkPolyID, FVector& PointA, FVector& PointB) const;
	/** Check if poly is a custom link */
	bool IsCustomLink(NavNodeRef PolyRef) const;

	/** Retrieves bounds of cluster. Returns false on error. */
	bool GetClusterBounds(NavNodeRef ClusterRef, FBox& OutBounds) const;

	uint32 GetTileIndexFromPolyRef(const NavNodeRef PolyRef) const { return DetourNavMesh != NULL ? DetourNavMesh->decodePolyIdTile(PolyRef) : uint32(INDEX_NONE); }
	NavNodeRef GetClusterRefFromPolyRef(const NavNodeRef PolyRef) const;

	static uint16 GetFilterForbiddenFlags(const FRecastQueryFilter* Filter);
	static void SetFilterForbiddenFlags(FRecastQueryFilter* Filter, uint16 ForbiddenFlags);

	void OnAreaCostChanged();

public:
	dtNavMesh const* GetRecastMesh() const { return DetourNavMesh; };
	dtNavMesh* GetRecastMesh() { return DetourNavMesh; };
	void ReleaseDetourNavMesh();

	void RemoveTileCacheLayers(int32 TileX, int32 TileY);
	void RemoveTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx);
	void AddTileCacheLayers(int32 TileX, int32 TileY, const TArray<FNavMeshTileData>& Layers);
	void AddTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx, const FNavMeshTileData& LayerData);
	void MarkEmptyTileCacheLayers(int32 TileX, int32 TileY);
	FNavMeshTileData GetTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx) const;
	TArray<FNavMeshTileData> GetTileCacheLayers(int32 TileX, int32 TileY) const;
	bool HasTileCacheLayers(int32 TileX, int32 TileY) const;

	/** Assigns recast generated navmesh to this instance.
	 *	@param bOwnData if true from now on this FPImplRecastNavMesh instance will be responsible for this piece 
	 *		of memory
	 */
	void SetRecastMesh(dtNavMesh* NavMesh);

	float GetTotalDataSize() const;

	/** Called on world origin changes */
	void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** calculated cost of given segment if traversed on specified poly. Function measures distance between specified points
	 *	and returns cost of traversing this distance on given poly.
	 *	@note no check if segment is on poly is performed. */
	float CalcSegmentCostOnPoly(NavNodeRef PolyID, const dtQueryFilter* Filter, const FVector& StartLoc, const FVector& EndLoc) const;

	ARecastNavMesh* NavMeshOwner;
	
	/** Recast's runtime navmesh data that we can query against */
	dtNavMesh* DetourNavMesh;

	/** Compressed layers data, can be reused for tiles generation */
	TMap<FIntPoint, TArray<FNavMeshTileData> > CompressedTileCacheLayers;

	/** query used for searching data on game thread */
	mutable dtNavMeshQuery SharedNavQuery;

	/** Helper function to serialize a single Recast tile. */
	static void SerializeRecastMeshTile(FArchive& Ar, int32 NavMeshVersion, unsigned char*& TileData, int32& TileDataSize);

	/** Helper function to serialize a Recast tile compressed data. */
	static void SerializeCompressedTileCacheData(FArchive& Ar, int32 NavMeshVersion, unsigned char*& CompressedData, int32& CompressedDataSize);

	/** Initialize data for pathfinding */
	bool InitPathfinding(const FVector& UnrealStart, const FVector& UnrealEnd, 
		const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
		FVector& RecastStart, dtPolyRef& StartPoly,
		FVector& RecastEnd, dtPolyRef& EndPoly) const;

	/** Marks path flags, perform string pulling if needed */
	void PostProcessPath(dtStatus PathfindResult, FNavMeshPath& Path,
		const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
		NavNodeRef StartNode, NavNodeRef EndNode,
		const FVector& UnrealStart, const FVector& UnrealEnd,
		const FVector& RecastStart, FVector& RecastEnd,
		dtQueryResult& PathResult) const;

	void GetDebugPolyEdges(const dtMeshTile& Tile, bool bInternalEdges, bool bNavMeshEdges, TArray<FVector>& InternalEdgeVerts, TArray<FVector>& NavMeshEdgeVerts) const;

	/** workhorse function finding portal edges between corridor polys */
	void GetEdgesForPathCorridorImpl(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges, const dtNavMeshQuery& NavQuery) const;

protected:
	int32 GetTilesDebugGeometry(const FRecastNavMeshGenerator* Generator, const dtMeshTile& Tile, int32 VertBase, FRecastDebugGeometry& OutGeometry, int32 TileIdx = INDEX_NONE) const;
};

#endif	// WITH_RECAST
