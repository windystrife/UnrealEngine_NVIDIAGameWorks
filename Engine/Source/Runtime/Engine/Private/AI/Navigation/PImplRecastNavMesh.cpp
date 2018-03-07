// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/PImplRecastNavMesh.h"
#include "AI/Navigation/NavigationSystem.h"

#if WITH_RECAST

// recast includes
#include "AI/Navigation/RecastHelpers.h"

// recast includes
#include "Detour/DetourNode.h"
#include "Recast/RecastAlloc.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/RecastNavMeshGenerator.h"
#include "AI/Navigation/RecastQueryFilter.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "VisualLogger/VisualLogger.h"


//----------------------------------------------------------------------//
// bunch of compile-time checks to assure types used by Recast and our
// mid-layer are the same size
//----------------------------------------------------------------------//
static_assert(sizeof(NavNodeRef) == sizeof(dtPolyRef), "NavNodeRef and dtPolyRef should be the same size.");
static_assert(RECAST_MAX_AREAS <= DT_MAX_AREAS, "Number of allowed areas cannot exceed DT_MAX_AREAS.");
static_assert(RECAST_STRAIGHTPATH_OFFMESH_CONNECTION == DT_STRAIGHTPATH_OFFMESH_CONNECTION, "Path flags values differ.");
// @todo ps4 compile issue: FLT_MAX constexpr issue
#if !PLATFORM_PS4
static_assert(RECAST_UNWALKABLE_POLY_COST == DT_UNWALKABLE_POLY_COST, "Unwalkable poly cost differ.");
#endif

/// Helper for accessing navigation query from different threads
#define INITIALIZE_NAVQUERY_SIMPLE(NavQueryVariable, NumNodes)	\
	dtNavMeshQuery NavQueryVariable##Private;	\
	dtNavMeshQuery& NavQueryVariable = IsInGameThread() ? SharedNavQuery : NavQueryVariable##Private; \
	NavQueryVariable.init(DetourNavMesh, NumNodes);

#define INITIALIZE_NAVQUERY(NavQueryVariable, NumNodes, LinkFilter)	\
	dtNavMeshQuery NavQueryVariable##Private;	\
	dtNavMeshQuery& NavQueryVariable = IsInGameThread() ? SharedNavQuery : NavQueryVariable##Private; \
	NavQueryVariable.init(DetourNavMesh, NumNodes, &LinkFilter);

static void* DetourMalloc(int Size, dtAllocHint)
{
	void* Result = FMemory::Malloc(uint32(Size));
#if STATS
	const uint32 ActualSize = FMemory::GetAllocSize(Result);
	INC_DWORD_STAT_BY(STAT_NavigationMemory, ActualSize);
	INC_MEMORY_STAT_BY(STAT_Navigation_RecastMemory, ActualSize);
#endif // STATS
	return Result;
}

static void* RecastMalloc(int Size, rcAllocHint)
{
	void* Result = FMemory::Malloc(uint32(Size));
#if STATS
	const uint32 ActualSize = FMemory::GetAllocSize(Result);
	INC_DWORD_STAT_BY(STAT_NavigationMemory, ActualSize);
	INC_MEMORY_STAT_BY(STAT_Navigation_RecastMemory, ActualSize);
#endif // STATS
	return Result;
}

static void RecastFree( void* Original )
{
#if STATS
	const uint32 Size = FMemory::GetAllocSize(Original);
	DEC_DWORD_STAT_BY(STAT_NavigationMemory, Size);	
	DEC_MEMORY_STAT_BY(STAT_Navigation_RecastMemory, Size);
#endif // STATS
	FMemory::Free(Original);
}

struct FRecastInitialSetup
{
	FRecastInitialSetup()
	{
		dtAllocSetCustom(DetourMalloc, RecastFree);
		rcAllocSetCustom(RecastMalloc, RecastFree);
	}
};
static FRecastInitialSetup RecastSetup;

/****************************
 * helpers
 ****************************/

static void Unr2RecastVector(FVector const& V, float* R)
{
	// @todo: speed this up with axis swaps instead of a full transform?
	FVector const RecastV = Unreal2RecastPoint(V);
	R[0] = RecastV.X;
	R[1] = RecastV.Y;
	R[2] = RecastV.Z;
}

static void Unr2RecastSizeVector(FVector const& V, float* R)
{
	// @todo: speed this up with axis swaps instead of a full transform?
	FVector const RecastVAbs = Unreal2RecastPoint(V).GetAbs();
	R[0] = RecastVAbs.X;
	R[1] = RecastVAbs.Y;
	R[2] = RecastVAbs.Z;
}

static FVector Recast2UnrVector(float const* R)
{
	return Recast2UnrealPoint(R);
}

ENavigationQueryResult::Type DTStatusToNavQueryResult(dtStatus Status)
{
	// @todo look at possible dtStatus values (in DetourStatus.h), there's more data that can be retrieved from it

	// Partial paths are treated by Recast as Success while we treat as fail
	return dtStatusSucceed(Status) ? (dtStatusDetail(Status, DT_PARTIAL_RESULT) ? ENavigationQueryResult::Fail : ENavigationQueryResult::Success)
		: (dtStatusDetail(Status, DT_INVALID_PARAM) ? ENavigationQueryResult::Error : ENavigationQueryResult::Fail);
}

//----------------------------------------------------------------------//
// FRecastQueryFilter();
//----------------------------------------------------------------------//

FRecastQueryFilter::FRecastQueryFilter(bool bIsVirtual)
	: dtQueryFilter(bIsVirtual)
{
	SetExcludedArea(RECAST_NULL_AREA);
}

INavigationQueryFilterInterface* FRecastQueryFilter::CreateCopy() const 
{
	return new FRecastQueryFilter(*this);
}

void FRecastQueryFilter::SetIsVirtual(bool bIsVirtual)
{
	dtQueryFilter* Filter = static_cast<dtQueryFilter*>(this);
	Filter = new(Filter)dtQueryFilter(bIsVirtual);
	SetExcludedArea(RECAST_NULL_AREA);
}

void FRecastQueryFilter::Reset()
{
	dtQueryFilter* Filter = static_cast<dtQueryFilter*>(this);
	Filter = new(Filter) dtQueryFilter(isVirtual);
	SetExcludedArea(RECAST_NULL_AREA);
}

void FRecastQueryFilter::SetAreaCost(uint8 AreaType, float Cost)
{
	setAreaCost(AreaType, Cost);
}

void FRecastQueryFilter::SetFixedAreaEnteringCost(uint8 AreaType, float Cost) 
{
#if WITH_FIXED_AREA_ENTERING_COST
	setAreaFixedCost(AreaType, Cost);
#endif // WITH_FIXED_AREA_ENTERING_COST
}

void FRecastQueryFilter::SetExcludedArea(uint8 AreaType)
{
	setAreaCost(AreaType, DT_UNWALKABLE_POLY_COST);
}

void FRecastQueryFilter::SetAllAreaCosts(const float* CostArray, const int32 Count) 
{
	// @todo could get away with memcopying to m_areaCost, but it's private and would require a little hack
	// need to consider if it's wort a try (not sure there'll be any perf gain)
	if (Count > RECAST_MAX_AREAS)
	{
		UE_LOG(LogNavigation, Warning, TEXT("FRecastQueryFilter: Trying to set cost to more areas than allowed! Discarding redundant values."));
	}

	const int32 ElementsCount = FPlatformMath::Min(Count, RECAST_MAX_AREAS);
	for (int32 i = 0; i < ElementsCount; ++i)
	{
		setAreaCost(i, CostArray[i]);
	}
}

void FRecastQueryFilter::GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const
{
	const float* DetourCosts = getAllAreaCosts();
	const float* DetourFixedCosts = getAllFixedAreaCosts();
	
	FMemory::Memcpy(CostArray, DetourCosts, sizeof(float) * FMath::Min(Count, RECAST_MAX_AREAS));
	FMemory::Memcpy(FixedCostArray, DetourFixedCosts, sizeof(float) * FMath::Min(Count, RECAST_MAX_AREAS));
}

void FRecastQueryFilter::SetBacktrackingEnabled(const bool bBacktracking)
{
	setIsBacktracking(bBacktracking);
}

bool FRecastQueryFilter::IsBacktrackingEnabled() const
{
	return getIsBacktracking();
}

bool FRecastQueryFilter::IsEqual(const INavigationQueryFilterInterface* Other) const
{
	// @NOTE: not type safe, should be changed when another filter type is introduced
	return FMemory::Memcmp(this, Other, sizeof(FRecastQueryFilter)) == 0;
}

void FRecastQueryFilter::SetIncludeFlags(uint16 Flags)
{
	setIncludeFlags(Flags);
}

uint16 FRecastQueryFilter::GetIncludeFlags() const
{
	return getIncludeFlags();
}

void FRecastQueryFilter::SetExcludeFlags(uint16 Flags)
{
	setExcludeFlags(Flags);
}

uint16 FRecastQueryFilter::GetExcludeFlags() const
{
	return getExcludeFlags();
}

bool FRecastSpeciaLinkFilter::isLinkAllowed(const int32 UserId) const
{
	const INavLinkCustomInterface* CustomLink = NavSys ? NavSys->GetCustomLink(UserId) : NULL;
	return (CustomLink != NULL) && CustomLink->IsLinkPathfindingAllowed(CachedOwnerOb);
}

void FRecastSpeciaLinkFilter::initialize()
{
	CachedOwnerOb = SearchOwner.Get();
}

//----------------------------------------------------------------------//
// FPImplRecastNavMesh
//----------------------------------------------------------------------//

FPImplRecastNavMesh::FPImplRecastNavMesh(ARecastNavMesh* Owner)
	: NavMeshOwner(Owner)
	, DetourNavMesh(NULL)
{
	check(Owner && "Owner must never be NULL");

	INC_DWORD_STAT_BY( STAT_NavigationMemory
		, Owner->HasAnyFlags(RF_ClassDefaultObject) == false ? sizeof(*this) : 0 );
};

FPImplRecastNavMesh::~FPImplRecastNavMesh()
{
	ReleaseDetourNavMesh();

	DEC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
};

void FPImplRecastNavMesh::ReleaseDetourNavMesh()
{
	// release navmesh only if we own it
	if (DetourNavMesh != nullptr)
	{
		dtFreeNavMesh(DetourNavMesh);
	}
	DetourNavMesh = nullptr;
	
	//
	CompressedTileCacheLayers.Empty();
}

/**
 * Serialization.
 * @param Ar - The archive with which to serialize.
 * @returns true if serialization was successful.
 */
void FPImplRecastNavMesh::Serialize( FArchive& Ar, int32 NavMeshVersion )
{
	//@todo: How to handle loading nav meshes saved w/ recast when recast isn't present????

	if (!Ar.IsLoading() && DetourNavMesh == NULL)
	{
		// nothing to write
		return;
	}
	
	// All we really need to do is read/write the data blob for each tile

	if (Ar.IsLoading())
	{
		// allocate the navmesh object
		ReleaseDetourNavMesh();
		DetourNavMesh = dtAllocNavMesh();

		if (DetourNavMesh == NULL)
		{
			UE_VLOG(NavMeshOwner, LogNavigation, Error, TEXT("Failed to allocate Recast navmesh"));
		}
	}

	int32 NumTiles = 0;
	TArray<int32> TilesToSave;

	if (Ar.IsSaving())
	{
		TilesToSave.Reserve(DetourNavMesh->getMaxTiles());
		
		if (NavMeshOwner->SupportsStreaming() && !IsRunningCommandlet())
		{
			// We save only tiles that belongs to this level
			GetNavMeshTilesIn(NavMeshOwner->GetNavigableBoundsInLevel(NavMeshOwner->GetLevel()), TilesToSave);
		}
		else
		{
			// Otherwise all valid tiles
			dtNavMesh const* ConstNavMesh = DetourNavMesh;
			for (int i = 0; i < ConstNavMesh->getMaxTiles(); ++i)
			{
				const dtMeshTile* Tile = ConstNavMesh->getTile(i);
				if (Tile != NULL && Tile->header != NULL && Tile->dataSize > 0)
				{
					TilesToSave.Add(i);
				}
			}
		}
		
		NumTiles = TilesToSave.Num();
	}

	Ar << NumTiles;

	dtNavMeshParams Params = *DetourNavMesh->getParams();
	Ar << Params.orig[0] << Params.orig[1] << Params.orig[2];
	Ar << Params.tileWidth;				///< The width of each tile. (Along the x-axis.)
	Ar << Params.tileHeight;			///< The height of each tile. (Along the z-axis.)
	Ar << Params.maxTiles;				///< The maximum number of tiles the navigation mesh can contain.
	Ar << Params.maxPolys;

	if (Ar.IsLoading())
	{
		// at this point we can tell whether navmesh being loaded is in line
		// ARecastNavMesh's params. If not, just skip it.
		// assumes tiles are rectangular
		const float ActorsTileSize = float(int32(NavMeshOwner->TileSizeUU / NavMeshOwner->CellSize) * NavMeshOwner->CellSize);

		if (ActorsTileSize != Params.tileWidth)
		{
			// just move archive position
			ReleaseDetourNavMesh();

			for (int i = 0; i < NumTiles; ++i)
			{
				dtTileRef TileRef = MAX_uint64;
				int32 TileDataSize = 0;
				Ar << TileRef << TileDataSize;

				if (TileRef == MAX_uint64 || TileDataSize == 0)
				{
					continue;
				}

				unsigned char* TileData = NULL;
				TileDataSize = 0;
				SerializeRecastMeshTile(Ar, NavMeshVersion, TileData, TileDataSize);
				if (TileData != NULL)
				{
					dtMeshHeader* const TileHeader = (dtMeshHeader*)TileData;
					dtFree(TileHeader);

					//
					if (Ar.UE4Ver() >= VER_UE4_ADD_MODIFIERS_RUNTIME_GENERATION && 
						(Ar.EngineVer().GetMajor() != 4 || Ar.EngineVer().GetMinor() != 7)) // Merged package from 4.7 branch
					{
						unsigned char* ComressedTileData = NULL;
						int32 CompressedTileDataSize = 0;
						SerializeCompressedTileCacheData(Ar, NavMeshVersion, ComressedTileData, CompressedTileDataSize);
						dtFree(ComressedTileData);
					}
				}
			}
		}
		else
		{
			// regular loading
			dtStatus Status = DetourNavMesh->init(&Params);
			if (dtStatusFailed(Status))
			{
				UE_VLOG(NavMeshOwner, LogNavigation, Error, TEXT("Failed to initialize NavMesh"));
			}

			for (int i = 0; i < NumTiles; ++i)
			{
				dtTileRef TileRef = MAX_uint64;
				int32 TileDataSize = 0;
				Ar << TileRef << TileDataSize;

				if (TileRef == MAX_uint64 || TileDataSize == 0)
				{
					continue;
				}
				
				unsigned char* TileData = NULL;
				TileDataSize = 0;
				SerializeRecastMeshTile(Ar, NavMeshVersion, TileData, TileDataSize);

				if (TileData != NULL)
				{
					dtMeshHeader* const TileHeader = (dtMeshHeader*)TileData;
					DetourNavMesh->addTile(TileData, TileDataSize, DT_TILE_FREE_DATA, TileRef, NULL);

					// Serialize compressed tile cache layer
					if (Ar.UE4Ver() >= VER_UE4_ADD_MODIFIERS_RUNTIME_GENERATION &&
						(Ar.EngineVer().GetMajor() != 4 || Ar.EngineVer().GetMinor() != 7)) // Merged package from 4.7 branch
					{
						uint8* ComressedTileData = nullptr;
						int32 CompressedTileDataSize = 0;
						SerializeCompressedTileCacheData(Ar, NavMeshVersion, ComressedTileData, CompressedTileDataSize);
						
						if (CompressedTileDataSize > 0)
						{
							AddTileCacheLayer(TileHeader->x, TileHeader->y, TileHeader->layer,
								FNavMeshTileData(ComressedTileData, CompressedTileDataSize, TileHeader->layer, Recast2UnrealBox(TileHeader->bmin, TileHeader->bmax)));
						}
					}
				}
			}
		}
	}
	else if (Ar.IsSaving())
	{
		const bool bSupportsRuntimeGeneration = NavMeshOwner->SupportsRuntimeGeneration();
		dtNavMesh const* ConstNavMesh = DetourNavMesh;
		
		for (int TileIndex : TilesToSave)
		{
			const dtMeshTile* Tile = ConstNavMesh->getTile(TileIndex);
			dtTileRef TileRef = ConstNavMesh->getTileRef(Tile);
			int32 TileDataSize = Tile->dataSize;
			Ar << TileRef << TileDataSize;

			unsigned char* TileData = Tile->data;
			SerializeRecastMeshTile(Ar, NavMeshVersion, TileData, TileDataSize);

			// Serialize compressed tile cache layer only if navmesh requires it
			{
				FNavMeshTileData TileCacheLayer;
				uint8* CompressedData = nullptr;
				int32 CompressedDataSize = 0;
				if (bSupportsRuntimeGeneration)
				{
					TileCacheLayer = GetTileCacheLayer(Tile->header->x, Tile->header->y, Tile->header->layer);
					CompressedData = TileCacheLayer.GetDataSafe();
					CompressedDataSize = TileCacheLayer.DataSize;
				}
				
				SerializeCompressedTileCacheData(Ar, NavMeshVersion, CompressedData, CompressedDataSize);
			}
		}
	}
}

void FPImplRecastNavMesh::SerializeRecastMeshTile(FArchive& Ar, int32 NavMeshVersion, unsigned char*& TileData, int32& TileDataSize)
{
	// The strategy here is to serialize the data blob that is passed into addTile()
	// @see dtCreateNavMeshData() for details on how this data is laid out


	int32 totVertCount;
	int32 totPolyCount;
	int32 maxLinkCount;
	int32 detailMeshCount;
	int32 detailVertCount;
	int32 detailTriCount;
	int32 bvNodeCount;
	int32 offMeshConCount;
	int32 offMeshSegConCount;
	int32 clusterCount;

	if (Ar.IsSaving())
	{
		// fill in data to write
		dtMeshHeader* const H = (dtMeshHeader*)TileData;
		totVertCount = H->vertCount;
		totPolyCount = H->polyCount;
		maxLinkCount = H->maxLinkCount;
		detailMeshCount = H->detailMeshCount;
		detailVertCount = H->detailVertCount;
		detailTriCount = H->detailTriCount;
		bvNodeCount = H->bvNodeCount;
		offMeshConCount = H->offMeshConCount;
		offMeshSegConCount = H->offMeshSegConCount;
		clusterCount = H->clusterCount;
	}

	Ar << totVertCount << totPolyCount << maxLinkCount;
	Ar << detailMeshCount << detailVertCount << detailTriCount;
	Ar << bvNodeCount << offMeshConCount << offMeshSegConCount;
	Ar << clusterCount;
	int32 polyClusterCount = detailMeshCount;

	// calc sizes for our data so we know how much to allocate and where to read/write stuff
	// note this may not match the on-disk size or the in-memory size on the machine that generated that data
	const int32 headerSize = dtAlign4(sizeof(dtMeshHeader));
	const int32 vertsSize = dtAlign4(sizeof(float)*3*totVertCount);
	const int32 polysSize = dtAlign4(sizeof(dtPoly)*totPolyCount);
	const int32 linksSize = dtAlign4(sizeof(dtLink)*maxLinkCount);
	const int32 detailMeshesSize = dtAlign4(sizeof(dtPolyDetail)*detailMeshCount);
	const int32 detailVertsSize = dtAlign4(sizeof(float)*3*detailVertCount );
	const int32 detailTrisSize = dtAlign4(sizeof(unsigned char)*4*detailTriCount);
	const int32 bvTreeSize = dtAlign4(sizeof(dtBVNode)*bvNodeCount);
	const int32 offMeshConsSize = dtAlign4(sizeof(dtOffMeshConnection)*offMeshConCount);
	const int32 offMeshSegsSize = dtAlign4(sizeof(dtOffMeshSegmentConnection)*offMeshSegConCount);
	const int32 clusterSize = dtAlign4(sizeof(dtCluster)*clusterCount);
	const int32 polyClustersSize = dtAlign4(sizeof(unsigned short)*polyClusterCount);

	if (Ar.IsLoading())
	{
		check(TileData == NULL);

		// allocate data chunk for this navmesh tile.  this is its final destination.
		TileDataSize = headerSize + vertsSize + polysSize + linksSize +
			detailMeshesSize + detailVertsSize + detailTrisSize +
			bvTreeSize + offMeshConsSize + offMeshSegsSize + 
			clusterSize + polyClustersSize;
		TileData = (unsigned char*)dtAlloc(sizeof(unsigned char)*TileDataSize, DT_ALLOC_PERM);
		if (!TileData)
		{
			UE_LOG(LogNavigation, Error, TEXT("Failed to alloc navmesh tile"));
		}
		FMemory::Memset(TileData, 0, TileDataSize);
	}
	else if (Ar.IsSaving())
	{
		// TileData and TileDataSize should already be set, verify
		check(TileData != NULL);
	}

	if (TileData != NULL)
	{
		// sort out where various data types do/will live
		unsigned char* d = TileData;
		dtMeshHeader* Header = (dtMeshHeader*)d; d += headerSize;
		float* NavVerts = (float*)d; d += vertsSize;
		dtPoly* NavPolys = (dtPoly*)d; d += polysSize;
		d += linksSize;			// @fixme, are links autogenerated on addTile?
		dtPolyDetail* DetailMeshes = (dtPolyDetail*)d; d += detailMeshesSize;
		float* DetailVerts = (float*)d; d += detailVertsSize;
		unsigned char* DetailTris = (unsigned char*)d; d += detailTrisSize;
		dtBVNode* BVTree = (dtBVNode*)d; d += bvTreeSize;
		dtOffMeshConnection* OffMeshCons = (dtOffMeshConnection*)d; d += offMeshConsSize;
		dtOffMeshSegmentConnection* OffMeshSegs = (dtOffMeshSegmentConnection*)d; d += offMeshSegsSize;
		dtCluster* Clusters = (dtCluster*)d; d += clusterSize;
		unsigned short* PolyClusters = (unsigned short*)d; d += polyClustersSize;

		check(d==(TileData + TileDataSize));

		// now serialize the data in the blob!

		// header
		Ar << Header->magic << Header->version << Header->x << Header->y;
		Ar << Header->layer << Header->userId << Header->polyCount << Header->vertCount;
		Ar << Header->maxLinkCount << Header->detailMeshCount << Header->detailVertCount << Header->detailTriCount;
		Ar << Header->bvNodeCount << Header->offMeshConCount<< Header->offMeshBase;
		Ar << Header->walkableHeight << Header->walkableRadius << Header->walkableClimb;
		Ar << Header->bmin[0] << Header->bmin[1] << Header->bmin[2];
		Ar << Header->bmax[0] << Header->bmax[1] << Header->bmax[2];
		Ar << Header->bvQuantFactor;
		Ar << Header->clusterCount;
		Ar << Header->offMeshSegConCount << Header->offMeshSegPolyBase << Header->offMeshSegVertBase;

		// mesh and offmesh connection vertices, just an array of floats (one float triplet per vert)
		{
			float* F = NavVerts;
			for (int32 VertIdx=0; VertIdx < totVertCount; VertIdx++)
			{
				Ar << *F; F++;
				Ar << *F; F++;
				Ar << *F; F++;
			}
		}

		// mesh and off-mesh connection polys
		for (int32 PolyIdx=0; PolyIdx < totPolyCount; ++PolyIdx)
		{
			dtPoly& P = NavPolys[PolyIdx];
			Ar << P.firstLink;

			for (uint32 VertIdx=0; VertIdx < DT_VERTS_PER_POLYGON; ++VertIdx)
			{
				Ar << P.verts[VertIdx];
			}
			for (uint32 NeiIdx=0; NeiIdx < DT_VERTS_PER_POLYGON; ++NeiIdx)
			{
				Ar << P.neis[NeiIdx];
			}
			Ar << P.flags << P.vertCount << P.areaAndtype;
		}

		// serialize detail meshes
		for (int32 MeshIdx=0; MeshIdx < detailMeshCount; ++MeshIdx)
		{
			dtPolyDetail& DM = DetailMeshes[MeshIdx];
			Ar << DM.vertBase << DM.triBase << DM.vertCount << DM.triCount;
		}

		// serialize detail verts (one float triplet per vert)
		{
			float* F = DetailVerts;
			for (int32 VertIdx=0; VertIdx < detailVertCount; ++VertIdx)
			{
				Ar << *F; F++;
				Ar << *F; F++;
				Ar << *F; F++;
			}
		}

		// serialize detail tris (4 one-byte indices per tri)
		{
			unsigned char* V = DetailTris;
			for (int32 TriIdx=0; TriIdx < detailTriCount; ++TriIdx)
			{
				Ar << *V; V++;
				Ar << *V; V++;
				Ar << *V; V++;
				Ar << *V; V++;
			}
		}

		// serialize BV tree
		for (int32 NodeIdx=0; NodeIdx < bvNodeCount; ++NodeIdx)
		{
			dtBVNode& Node = BVTree[NodeIdx];
			Ar << Node.bmin[0] << Node.bmin[1] << Node.bmin[2];
			Ar << Node.bmax[0] << Node.bmax[1] << Node.bmax[2];
			Ar << Node.i;
		}

		// serialize off-mesh connections
		for (int32 ConnIdx=0; ConnIdx < offMeshConCount; ++ConnIdx)
		{
			dtOffMeshConnection& Conn = OffMeshCons[ConnIdx];
			Ar << Conn.pos[0] << Conn.pos[1] << Conn.pos[2] << Conn.pos[3] << Conn.pos[4] << Conn.pos[5];
			Ar << Conn.rad << Conn.poly << Conn.flags << Conn.side << Conn.userId;
		}

		if (NavMeshVersion >= NAVMESHVER_OFFMESH_HEIGHT_BUG)
		{
			for (int32 ConnIdx = 0; ConnIdx < offMeshConCount; ++ConnIdx)
			{
				dtOffMeshConnection& Conn = OffMeshCons[ConnIdx];
				Ar << Conn.height;
			}
		}

		for (int32 SegIdx=0; SegIdx < offMeshSegConCount; ++SegIdx)
		{
			dtOffMeshSegmentConnection& Seg = OffMeshSegs[SegIdx];
			Ar << Seg.startA[0] << Seg.startA[1] << Seg.startA[2];
			Ar << Seg.startB[0] << Seg.startB[1] << Seg.startB[2];
			Ar << Seg.endA[0] << Seg.endA[1] << Seg.endA[2];
			Ar << Seg.endB[0] << Seg.endB[1] << Seg.endB[2];
			Ar << Seg.rad << Seg.firstPoly << Seg.npolys << Seg.flags << Seg.userId;
		}

		// serialize clusters
		for (int32 CIdx = 0; CIdx < clusterCount; ++CIdx)
		{
			dtCluster& cluster = Clusters[CIdx];
			Ar << cluster.center[0] << cluster.center[1] << cluster.center[2];
		}

		// serialize poly clusters map
		{
			unsigned short* C = PolyClusters;
			for (int32 PolyClusterIdx = 0; PolyClusterIdx < polyClusterCount; ++PolyClusterIdx)
			{
				Ar << *C; C++;
			}
		}
	}
}

void FPImplRecastNavMesh::SerializeCompressedTileCacheData(FArchive& Ar, int32 NavMeshVersion, unsigned char*& CompressedData, int32& CompressedDataSize)
{
	Ar << CompressedDataSize;

	if (CompressedDataSize > 0)
	{
		if (Ar.IsLoading())
		{
			CompressedData = (unsigned char*)dtAlloc(sizeof(unsigned char)*CompressedDataSize, DT_ALLOC_PERM);
			if (!CompressedData)
			{
				UE_LOG(LogNavigation, Error, TEXT("Failed to alloc tile compressed data"));
			}
			FMemory::Memset(CompressedData, 0, CompressedDataSize);
		}

		Ar.Serialize(CompressedData, CompressedDataSize);
	}
}

void FPImplRecastNavMesh::SetRecastMesh(dtNavMesh* NavMesh)
{
	if (NavMesh == DetourNavMesh)
	{
		return;
	}

	ReleaseDetourNavMesh();
	DetourNavMesh = NavMesh;

	if (NavMeshOwner)
	{
		NavMeshOwner->UpdateNavObject();
	}

	// reapply area sort order in new detour navmesh
	OnAreaCostChanged();
}

void FPImplRecastNavMesh::Raycast(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner, 
	ARecastNavMesh::FRaycastResult& RaycastResult, NavNodeRef StartNode) const
{
	if (DetourNavMesh == NULL || NavMeshOwner == NULL)
	{
		return;
	}

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(InQueryFilter.GetImplementation()))->GetAsDetourQueryFilter();
	if (QueryFilter == NULL)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::Raycast failing due to QueryFilter == NULL"));
		return;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, InQueryFilter.GetMaxSearchNodes(), LinkFilter);

	const FVector NavExtent = NavMeshOwner->GetModifiedQueryExtent(NavMeshOwner->GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	const FVector RecastStart = Unreal2RecastPoint(StartLoc);
	const FVector RecastEnd = Unreal2RecastPoint(EndLoc);

	if (StartNode == INVALID_NAVNODEREF)
	{
		NavQuery.findNearestContainingPoly(&RecastStart.X, Extent, QueryFilter, &StartNode, NULL);
	}

	NavNodeRef EndNode = INVALID_NAVNODEREF;
	NavQuery.findNearestContainingPoly(&RecastEnd.X, Extent, QueryFilter, &EndNode, NULL);

	if (StartNode != INVALID_NAVNODEREF)
	{
		float RecastHitNormal[3];

		const dtStatus RaycastStatus = NavQuery.raycast(StartNode, &RecastStart.X, &RecastEnd.X
			, QueryFilter, &RaycastResult.HitTime, RecastHitNormal
			, RaycastResult.CorridorPolys, &RaycastResult.CorridorPolysCount, RaycastResult.GetMaxCorridorSize());

		RaycastResult.HitNormal = Recast2UnrVector(RecastHitNormal);
		RaycastResult.bIsRaycastEndInCorridor = dtStatusSucceed(RaycastStatus) && (RaycastResult.GetLastNodeRef() == EndNode);
	}
	else
	{
		RaycastResult.HitTime = 0.f;
		RaycastResult.HitNormal = (StartLoc - EndLoc).GetSafeNormal();
	}
}

// @TODONAV
ENavigationQueryResult::Type FPImplRecastNavMesh::FindPath(const FVector& StartLoc, const FVector& EndLoc, FNavMeshPath& Path, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner) const
{
	// temporarily disabling this check due to it causing too much "crashes"
	// @todo but it needs to be back at some point since it realy checks for a buggy setup
	//ensure(DetourNavMesh != NULL || NavMeshOwner->bRebuildAtRuntime == false);

	if (DetourNavMesh == NULL || NavMeshOwner == NULL)
	{
		return ENavigationQueryResult::Error;
	}

	const FRecastQueryFilter* FilterImplementation = (const FRecastQueryFilter*)(InQueryFilter.GetImplementation());
	if (FilterImplementation == NULL)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Error, TEXT("FPImplRecastNavMesh::FindPath failed due to passed filter having NULL implementation!"));
		return ENavigationQueryResult::Error;
	}

	const dtQueryFilter* QueryFilter = FilterImplementation->GetAsDetourQueryFilter();
	if (QueryFilter == NULL)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::FindPath failing due to QueryFilter == NULL"));
		return ENavigationQueryResult::Error;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, InQueryFilter.GetMaxSearchNodes(), LinkFilter);

	FVector RecastStartPos, RecastEndPos;
	NavNodeRef StartPolyID, EndPolyID;
	const bool bCanSearch = InitPathfinding(StartLoc, EndLoc, NavQuery, QueryFilter, RecastStartPos, StartPolyID, RecastEndPos, EndPolyID);
	if (!bCanSearch)
	{
		return ENavigationQueryResult::Error;
	}

	// get path corridor
	dtQueryResult PathResult;
	const dtStatus FindPathStatus = NavQuery.findPath(StartPolyID, EndPolyID, &RecastStartPos.X, &RecastEndPos.X, QueryFilter, PathResult, 0);

	// check for special case, where path has not been found, and starting polygon
	// was the one closest to the target
	if (PathResult.size() == 1 && dtStatusDetail(FindPathStatus, DT_PARTIAL_RESULT))
	{
		// in this case we find a point on starting polygon, that's closest to destination
		// and store it as path end
		FVector RecastHandPlacedPathEnd;
		NavQuery.closestPointOnPolyBoundary(StartPolyID, &RecastEndPos.X, &RecastHandPlacedPathEnd.X);

		new(Path.GetPathPoints()) FNavPathPoint(Recast2UnrVector(&RecastStartPos.X), StartPolyID);
		new(Path.GetPathPoints()) FNavPathPoint(Recast2UnrVector(&RecastHandPlacedPathEnd.X), StartPolyID);

		Path.PathCorridor.Add(PathResult.getRef(0));
		Path.PathCorridorCost.Add(CalcSegmentCostOnPoly(StartPolyID, QueryFilter, RecastHandPlacedPathEnd, RecastStartPos));
	}
	else
	{
		PostProcessPath(FindPathStatus, Path, NavQuery, QueryFilter,
			StartPolyID, EndPolyID, Recast2UnrVector(&RecastStartPos.X), Recast2UnrVector(&RecastEndPos.X), RecastStartPos, RecastEndPos,
			PathResult);
	}

	if (dtStatusDetail(FindPathStatus, DT_PARTIAL_RESULT))
	{
		Path.SetIsPartial(true);
		// this means path finding algorithm reached the limit of InQueryFilter.GetMaxSearchNodes()
		// nodes in A* node pool. This can mean resulting path is way off.
		Path.SetSearchReachedLimit(dtStatusDetail(FindPathStatus, DT_OUT_OF_NODES));
	}

#if ENABLE_VISUAL_LOG
	if (dtStatusDetail(FindPathStatus, DT_INVALID_CYCLE_PATH))
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Error, TEXT("FPImplRecastNavMesh::FindPath resulted in a cyclic path!"));
		FVisualLogEntry* Entry = FVisualLogger::Get().GetLastEntryForObject(NavMeshOwner);
		if (Entry)
		{
			Path.DescribeSelfToVisLog(Entry);
		}
	}
#endif // ENABLE_VISUAL_LOG

	Path.MarkReady();

	return DTStatusToNavQueryResult(FindPathStatus);
}

ENavigationQueryResult::Type FPImplRecastNavMesh::TestPath(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& InQueryFilter, const UObject* Owner, int32* NumVisitedNodes) const
{
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(InQueryFilter.GetImplementation()))->GetAsDetourQueryFilter();
	if (QueryFilter == NULL)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::FindPath failing due to QueryFilter == NULL"));
		return ENavigationQueryResult::Error;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, InQueryFilter.GetMaxSearchNodes(), LinkFilter);

	FVector RecastStartPos, RecastEndPos;
	NavNodeRef StartPolyID, EndPolyID;
	const bool bCanSearch = InitPathfinding(StartLoc, EndLoc, NavQuery, QueryFilter, RecastStartPos, StartPolyID, RecastEndPos, EndPolyID);
	if (!bCanSearch)
	{
		return ENavigationQueryResult::Error;
	}

	// get path corridor
	dtQueryResult PathResult;
	const dtStatus FindPathStatus = NavQuery.findPath(StartPolyID, EndPolyID,
		&RecastStartPos.X, &RecastEndPos.X, QueryFilter, PathResult, 0);

	if (NumVisitedNodes)
	{
		*NumVisitedNodes = NavQuery.getQueryNodes();
	}

	return DTStatusToNavQueryResult(FindPathStatus);
}

ENavigationQueryResult::Type FPImplRecastNavMesh::TestClusterPath(const FVector& StartLoc, const FVector& EndLoc, int32* NumVisitedNodes) const
{
	FVector RecastStartPos, RecastEndPos;
	NavNodeRef StartPolyID, EndPolyID;
	const dtQueryFilter* ClusterFilter = ((const FRecastQueryFilter*)NavMeshOwner->GetDefaultQueryFilterImpl())->GetAsDetourQueryFilter();

	INITIALIZE_NAVQUERY_SIMPLE(ClusterQuery, NavMeshOwner->DefaultMaxHierarchicalSearchNodes);

	const bool bCanSearch = InitPathfinding(StartLoc, EndLoc, ClusterQuery, ClusterFilter, RecastStartPos, StartPolyID, RecastEndPos, EndPolyID);
	if (!bCanSearch)
	{
		return ENavigationQueryResult::Error;
	}

	const dtStatus status = ClusterQuery.testClusterPath(StartPolyID, EndPolyID);
	if (NumVisitedNodes)
	{
		*NumVisitedNodes = ClusterQuery.getQueryNodes();
	}

	return DTStatusToNavQueryResult(status);
}

bool FPImplRecastNavMesh::InitPathfinding(const FVector& UnrealStart, const FVector& UnrealEnd,
	const dtNavMeshQuery& Query, const dtQueryFilter* Filter,
	FVector& RecastStart, dtPolyRef& StartPoly,
	FVector& RecastEnd, dtPolyRef& EndPoly) const
{
	const FVector NavExtent = NavMeshOwner->GetModifiedQueryExtent(NavMeshOwner->GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	const FVector RecastStartToProject = Unreal2RecastPoint(UnrealStart);
	const FVector RecastEndToProject = Unreal2RecastPoint(UnrealEnd);

	StartPoly = INVALID_NAVNODEREF;
	Query.findNearestPoly(&RecastStartToProject.X, Extent, Filter, &StartPoly, &RecastStart.X);
	if (StartPoly == INVALID_NAVNODEREF)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::InitPathfinding start point not on navmesh"));
		UE_VLOG_SEGMENT(NavMeshOwner, LogNavigation, Warning, UnrealStart, UnrealEnd, FColor::Red, TEXT("Failed path"));
		UE_VLOG_LOCATION(NavMeshOwner, LogNavigation, Warning, UnrealStart, 15, FColor::Red, TEXT("Start failed"));
		UE_VLOG_BOX(NavMeshOwner, LogNavigation, Warning, FBox(UnrealStart - NavExtent, UnrealStart + NavExtent), FColor::Red, TEXT_EMPTY);

		return false;
	}

	EndPoly = INVALID_NAVNODEREF;
	Query.findNearestPoly(&RecastEndToProject.X, Extent, Filter, &EndPoly, &RecastEnd.X);
	if (EndPoly == INVALID_NAVNODEREF)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::InitPathfinding end point not on navmesh"));
		UE_VLOG_SEGMENT(NavMeshOwner, LogNavigation, Warning, UnrealEnd, UnrealEnd, FColor::Red, TEXT("Failed path"));
		UE_VLOG_LOCATION(NavMeshOwner, LogNavigation, Warning, UnrealEnd, 15, FColor::Red, TEXT("End failed"));
		UE_VLOG_BOX(NavMeshOwner, LogNavigation, Warning, FBox(UnrealEnd - NavExtent, UnrealEnd + NavExtent), FColor::Red, TEXT_EMPTY);

		return false;
	}

	return true;
}

float FPImplRecastNavMesh::CalcSegmentCostOnPoly(NavNodeRef PolyID, const dtQueryFilter* Filter, const FVector& StartLoc, const FVector& EndLoc) const
{
	uint8 AreaID = RECAST_DEFAULT_AREA;
	DetourNavMesh->getPolyArea(PolyID, &AreaID);

	const float AreaTravelCost = Filter->getAreaCost(AreaID);
	return AreaTravelCost * (EndLoc - StartLoc).Size();
}

void FPImplRecastNavMesh::PostProcessPath(dtStatus FindPathStatus, FNavMeshPath& Path,
	const dtNavMeshQuery& NavQuery, const dtQueryFilter* Filter,
	NavNodeRef StartPolyID, NavNodeRef EndPolyID,
	const FVector& StartLoc, const FVector& EndLoc,
	const FVector& RecastStartPos, FVector& RecastEndPos,
	dtQueryResult& PathResult) const
{
	// note that for recast partial path is successful, while we treat it as failed, just marking it as partial
	if (dtStatusSucceed(FindPathStatus))
	{
		// check if navlink poly at end of path is allowed
		int32 PathSize = PathResult.size();
		if ((PathSize > 1) && NavMeshOwner && !NavMeshOwner->bAllowNavLinkAsPathEnd)
		{
			uint16 PolyFlags = 0;
			DetourNavMesh->getPolyFlags(PathResult.getRef(PathSize - 1), &PolyFlags);

			if (PolyFlags & ARecastNavMesh::GetNavLinkFlag())
			{
				PathSize--;
			}
		}

		Path.PathCorridorCost.AddUninitialized(PathSize);

		if (PathSize == 1)
		{
			// failsafe cost for single poly corridor
			Path.PathCorridorCost[0] = CalcSegmentCostOnPoly(StartPolyID, Filter, EndLoc, StartLoc);
		}
		else
		{
			for (int32 i = 0; i < PathSize; i++)
			{
				Path.PathCorridorCost[i] = PathResult.getCost(i);
			}
		}
		
		// copy over corridor poly data
		Path.PathCorridor.AddUninitialized(PathSize);
		NavNodeRef* DestCorridorPoly = Path.PathCorridor.GetData();
		for (int i = 0; i < PathSize; ++i, ++DestCorridorPoly)
		{
			*DestCorridorPoly = PathResult.getRef(i);
		}

		Path.OnPathCorridorUpdated(); 

#if STATS
		if (dtStatusDetail(FindPathStatus, DT_OUT_OF_NODES))
		{
			INC_DWORD_STAT(STAT_Navigation_OutOfNodesPath);
		}

		if (dtStatusDetail(FindPathStatus, DT_PARTIAL_RESULT))
		{
			INC_DWORD_STAT(STAT_Navigation_PartialPath);
		}
#endif

		if (Path.WantsStringPulling())
		{
			FVector UseEndLoc = EndLoc;
			
			// if path is partial (path corridor doesn't contain EndPolyID), find new RecastEndPos on last poly in corridor
			if (dtStatusDetail(FindPathStatus, DT_PARTIAL_RESULT))
			{
				NavNodeRef LastPolyID = Path.PathCorridor.Last();
				float NewEndPoint[3];

				const dtStatus NewEndPointStatus = NavQuery.closestPointOnPoly(LastPolyID, &RecastEndPos.X, NewEndPoint);
				if (dtStatusSucceed(NewEndPointStatus))
				{
					UseEndLoc = Recast2UnrealPoint(NewEndPoint);
				}
			}

			Path.PerformStringPulling(StartLoc, UseEndLoc);
		}
		else
		{
			// make sure at least beginning and end of path are added
			new(Path.GetPathPoints()) FNavPathPoint(StartLoc, StartPolyID);
			new(Path.GetPathPoints()) FNavPathPoint(EndLoc, EndPolyID);

			// collect all custom links Ids
			for (int32 Idx = 0; Idx < Path.PathCorridor.Num(); Idx++)
			{
				const dtOffMeshConnection* OffMeshCon = DetourNavMesh->getOffMeshConnectionByRef(Path.PathCorridor[Idx]);
				if (OffMeshCon)
				{
					Path.CustomLinkIds.Add(OffMeshCon->userId);
				}
			}
		}

		if (Path.WantsPathCorridor())
		{
			TArray<FNavigationPortalEdge> PathCorridorEdges;
			GetEdgesForPathCorridorImpl(&Path.PathCorridor, &PathCorridorEdges, NavQuery);
			Path.SetPathCorridorEdges(PathCorridorEdges);
		}
	}
}

bool FPImplRecastNavMesh::FindStraightPath(const FVector& StartLoc, const FVector& EndLoc, const TArray<NavNodeRef>& PathCorridor, TArray<FNavPathPoint>& PathPoints, TArray<uint32>* CustomLinks) const
{
	INITIALIZE_NAVQUERY_SIMPLE(NavQuery, RECAST_MAX_SEARCH_NODES);

	const FVector RecastStartPos = Unreal2RecastPoint(StartLoc);
	const FVector RecastEndPos = Unreal2RecastPoint(EndLoc);
	bool bResult = false;

	dtQueryResult StringPullResult;
	const dtStatus StringPullStatus = NavQuery.findStraightPath(&RecastStartPos.X, &RecastEndPos.X,
		PathCorridor.GetData(), PathCorridor.Num(), StringPullResult, DT_STRAIGHTPATH_AREA_CROSSINGS);

	PathPoints.Reset();
	if (dtStatusSucceed(StringPullStatus))
	{
		PathPoints.AddZeroed(StringPullResult.size());

		// convert to desired format
		FNavPathPoint* CurVert = PathPoints.GetData();

		for (int32 VertIdx = 0; VertIdx < StringPullResult.size(); ++VertIdx)
		{
			const float* CurRecastVert = StringPullResult.getPos(VertIdx);
			CurVert->Location = Recast2UnrVector(CurRecastVert);
			CurVert->NodeRef = StringPullResult.getRef(VertIdx);

			FNavMeshNodeFlags CurNodeFlags(0);
			CurNodeFlags.PathFlags = StringPullResult.getFlag(VertIdx);

			uint8 AreaID = RECAST_DEFAULT_AREA;
			DetourNavMesh->getPolyArea(CurVert->NodeRef, &AreaID);
			CurNodeFlags.Area = AreaID;

			const UClass* AreaClass = NavMeshOwner->GetAreaClass(AreaID);
			const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
			CurNodeFlags.AreaFlags = DefArea ? DefArea->GetAreaFlags() : 0;

			CurVert->Flags = CurNodeFlags.Pack();

			// include smart link data
			// if there will be more "edge types" we change this implementation to something more generic
			if (CustomLinks && (CurNodeFlags.PathFlags & DT_STRAIGHTPATH_OFFMESH_CONNECTION))
			{
				const dtOffMeshConnection* OffMeshCon = DetourNavMesh->getOffMeshConnectionByRef(CurVert->NodeRef);
				if (OffMeshCon)
				{
					CurVert->CustomLinkId = OffMeshCon->userId;
					CustomLinks->Add(OffMeshCon->userId);
				}
			}

			CurVert++;
		}

		// findStraightPath returns 0 for polyId of last point for some reason, even though it knows the poly id.  We will fill that in correctly with the last poly id of the corridor.
		// @TODO shouldn't it be the same as EndPolyID? (nope, it could be partial path)
		PathPoints.Last().NodeRef = PathCorridor.Last();
		bResult = true;
	}

	return bResult;
}

static bool IsDebugNodeModified(const FRecastDebugPathfindingNode& NodeData, const FRecastDebugPathfindingData& PreviousStep)
{
	const FRecastDebugPathfindingNode* PrevNodeData = PreviousStep.Nodes.Find(NodeData);
	if (PrevNodeData)
	{
		const bool bModified = PrevNodeData->bOpenSet != NodeData.bOpenSet ||
			PrevNodeData->TotalCost != NodeData.TotalCost ||
			PrevNodeData->Cost != NodeData.Cost ||
			PrevNodeData->ParentRef != NodeData.ParentRef ||
			!PrevNodeData->NodePos.Equals(NodeData.NodePos, SMALL_NUMBER);

		return bModified;
	}

	return true;
}

static void StorePathfindingDebugLength(FRecastDebugPathfindingNode& Node, FRecastDebugPathfindingData& Data)
{
	if (Node.Length >= 0.0f)
	{
		return;
	}

	FRecastDebugPathfindingNode* ParentNode = Data.Nodes.Find(FRecastDebugPathfindingNode(Node.ParentRef));
	if (ParentNode)
	{
		StorePathfindingDebugLength(*ParentNode, Data);
		Node.Length = ParentNode->Length + FVector::Dist(Node.NodePos, ParentNode->NodePos);
	}
	else
	{
		Node.Length = 0.0f;
	}
}

static void StorePathfindingDebugData(const dtNavMeshQuery& NavQuery, const dtNavMesh* NavMesh, FRecastDebugPathfindingData& Data)
{
	dtNode* BestNode = 0;
	float BestNodeCost = 0.0f;
	NavQuery.getCurrentBestResult(BestNode, BestNodeCost);

	const dtNodePool* NodePool = NavQuery.getNodePool();
	for (int32 i = 0; i < NodePool->getNodeCount(); i++)
	{
		const dtNode* Node = NodePool->getNodeAtIdx(i + 1);

		FRecastDebugPathfindingNode NodeInfo;
		NodeInfo.PolyRef = Node->id;
		NodeInfo.ParentRef = Node->pidx ? NodePool->getNodeAtIdx(Node->pidx)->id : 0;
		NodeInfo.Cost = Node->cost;
		NodeInfo.TotalCost = Node->total;
		NodeInfo.Length = -1.0f;
		NodeInfo.bOpenSet = !NavQuery.isInClosedList(Node->id);
		NodeInfo.bModified = true;
		NodeInfo.NodePos = Recast2UnrealPoint(&Node->pos[0]);

		const dtPoly* NavPoly = 0;
		const dtMeshTile* NavTile = 0;
		NavMesh->getTileAndPolyByRef(Node->id, &NavTile, &NavPoly);

		NodeInfo.bOffMeshLink = NavPoly ? (NavPoly->getType() != DT_POLYTYPE_GROUND) : false;
		if (Data.Flags & ERecastDebugPathfindingFlags::Vertices)
		{
			check(NavPoly);
			for (int32 iv = 0; iv < NavPoly->vertCount; iv++)
			{
				NodeInfo.Verts.Add(Recast2UnrealPoint(&NavTile->verts[NavPoly->verts[iv] * 3]));
			}
		}

		FSetElementId SetId = Data.Nodes.Add(NodeInfo);
		if (Node == BestNode && (Data.Flags & ERecastDebugPathfindingFlags::BestNode))
		{
			Data.BestNode = SetId;
		}
	}

	if (Data.Flags & ERecastDebugPathfindingFlags::PathLength)
	{
		for (TSet<FRecastDebugPathfindingNode>::TIterator It(Data.Nodes); It; ++It)
		{
			FRecastDebugPathfindingNode& Node = *It;
			StorePathfindingDebugLength(Node, Data);
		}
	}
}

static void StorePathfindingDebugStep(const dtNavMeshQuery& NavQuery, const dtNavMesh* NavMesh, TArray<FRecastDebugPathfindingData>& Steps)
{
	const int StepIdx = Steps.AddZeroed(1);
	FRecastDebugPathfindingData& StepInfo = Steps[StepIdx];
	StepInfo.Flags = ERecastDebugPathfindingFlags::BestNode | ERecastDebugPathfindingFlags::Vertices;
	
	StorePathfindingDebugData(NavQuery, NavMesh, StepInfo);

	if (Steps.Num() > 1)
	{
		FRecastDebugPathfindingData& PrevStepInfo = Steps[StepIdx - 1];
		for (TSet<FRecastDebugPathfindingNode>::TIterator It(StepInfo.Nodes); It; ++It)
		{
			FRecastDebugPathfindingNode& NodeData = *It;
			NodeData.bModified = IsDebugNodeModified(NodeData, PrevStepInfo);
		}
	}
}

int32 FPImplRecastNavMesh::DebugPathfinding(const FVector& StartLoc, const FVector& EndLoc, const FNavigationQueryFilter& Filter, const UObject* Owner, TArray<FRecastDebugPathfindingData>& Steps)
{
	int32 NumSteps = 0;

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	if (QueryFilter == NULL)
	{
		UE_VLOG(NavMeshOwner, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::DebugPathfinding failing due to QueryFilter == NULL"));
		return NumSteps;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	FVector RecastStartPos, RecastEndPos;
	NavNodeRef StartPolyID, EndPolyID;
	const bool bCanSearch = InitPathfinding(StartLoc, EndLoc, NavQuery, QueryFilter, RecastStartPos, StartPolyID, RecastEndPos, EndPolyID);
	if (!bCanSearch)
	{
		return NumSteps;
	}

	dtStatus status = NavQuery.initSlicedFindPath(StartPolyID, EndPolyID, &RecastStartPos.X, &RecastEndPos.X, QueryFilter);
	while (dtStatusInProgress(status))
	{
		StorePathfindingDebugStep(NavQuery, DetourNavMesh, Steps);
		NumSteps++;

		status = NavQuery.updateSlicedFindPath(1, 0);
	}

	static const int32 MAX_TEMP_POLYS = 16;
	NavNodeRef TempPolys[MAX_TEMP_POLYS];
	int32 NumTempPolys;
	NavQuery.finalizeSlicedFindPath(TempPolys, &NumTempPolys, MAX_TEMP_POLYS);

	return NumSteps;
}

NavNodeRef FPImplRecastNavMesh::GetClusterRefFromPolyRef(const NavNodeRef PolyRef) const
{
	if (DetourNavMesh)
	{
		const dtMeshTile* Tile = DetourNavMesh->getTileByRef(PolyRef);
		uint32 PolyIdx = DetourNavMesh->decodePolyIdPoly(PolyRef);
		if (Tile && Tile->polyClusters && PolyIdx < (uint32)Tile->header->offMeshBase)
		{
			return DetourNavMesh->getClusterRefBase(Tile) | Tile->polyClusters[PolyIdx];
		}
	}

	return 0;
}

FNavLocation FPImplRecastNavMesh::GetRandomPoint(const FNavigationQueryFilter& Filter, const UObject* Owner) const
{
	FNavLocation OutLocation;
	if (DetourNavMesh == NULL)
	{
		return OutLocation;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	// inits to "pass all"
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter)
	{
		dtPolyRef Poly;
		float RandPt[3];
		dtStatus Status = NavQuery.findRandomPoint(QueryFilter, FMath::FRand, &Poly, RandPt);
		if (dtStatusSucceed(Status))
		{
			// arrange output
			OutLocation.Location = Recast2UnrVector(RandPt);
			OutLocation.NodeRef = Poly;
		}
	}

	return OutLocation;
}

bool FPImplRecastNavMesh::GetRandomPointInCluster(NavNodeRef ClusterRef, FNavLocation& OutLocation) const
{
	if (DetourNavMesh == NULL || ClusterRef == 0)
	{
		return false;
	}

	INITIALIZE_NAVQUERY_SIMPLE(NavQuery, RECAST_MAX_SEARCH_NODES);

	dtPolyRef Poly;
	float RandPt[3];
	dtStatus Status = NavQuery.findRandomPointInCluster(ClusterRef, FMath::FRand, &Poly, RandPt);

	if (dtStatusSucceed(Status))
	{
		OutLocation = FNavLocation(Recast2UnrVector(RandPt), Poly);
		return true;
	}

	return false;
}

bool FPImplRecastNavMesh::ProjectPointToNavMesh(const FVector& Point, FNavLocation& Result, const FVector& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const
{
	// sanity check
	if (DetourNavMesh == NULL)
	{
		return false;
	}

	bool bSuccess = false;

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter)
	{
		float ClosestPoint[3];

		const FVector ModifiedExtent = NavMeshOwner->GetModifiedQueryExtent(Extent);
		FVector RcExtent = Unreal2RecastPoint(ModifiedExtent).GetAbs();
	
		FVector RcPoint = Unreal2RecastPoint(Point);
		dtPolyRef PolyRef;
		NavQuery.findNearestPoly2D(&RcPoint.X, &RcExtent.X, QueryFilter, &PolyRef, ClosestPoint);

		if( PolyRef > 0 )
		{
			// one last step required due to recast's BVTree imprecision
			const FVector& UnrealClosestPoint = Recast2UnrVector(ClosestPoint);			
			const FVector ClosestPointDelta = UnrealClosestPoint - Point;
			if (-ModifiedExtent.X <= ClosestPointDelta.X && ClosestPointDelta.X <= ModifiedExtent.X
				&& -ModifiedExtent.Y <= ClosestPointDelta.Y && ClosestPointDelta.Y <= ModifiedExtent.Y
				&& -ModifiedExtent.Z <= ClosestPointDelta.Z && ClosestPointDelta.Z <= ModifiedExtent.Z)
			{
				bSuccess = true;
				Result = FNavLocation(UnrealClosestPoint, PolyRef);
			}
		}
	}

	return (bSuccess);
}

bool FPImplRecastNavMesh::ProjectPointMulti(const FVector& Point, TArray<FNavLocation>& Result, const FVector& Extent,
	float MinZ, float MaxZ, const FNavigationQueryFilter& Filter, const UObject* Owner) const
{
	// sanity check
	if (DetourNavMesh == NULL)
	{
		return false;
	}

	bool bSuccess = false;

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter)
	{
		const FVector ModifiedExtent = NavMeshOwner->GetModifiedQueryExtent(Extent);
		const FVector AdjustedPoint(Point.X, Point.Y, (MaxZ + MinZ) * 0.5f);
		const FVector AdjustedExtent(ModifiedExtent.X, ModifiedExtent.Y, (MaxZ - MinZ) * 0.5f);

		const FVector RcPoint = Unreal2RecastPoint( AdjustedPoint );
		const FVector RcExtent = Unreal2RecastPoint( AdjustedExtent ).GetAbs();

		const int32 MaxHitPolys = 256;
		dtPolyRef HitPolys[MaxHitPolys];
		int32 NumHitPolys = 0;

		dtStatus status = NavQuery.queryPolygons(&RcPoint.X, &RcExtent.X, QueryFilter, HitPolys, &NumHitPolys, MaxHitPolys);
		if (dtStatusSucceed(status))
		{
			for (int32 i = 0; i < NumHitPolys; i++)
			{
				float ClosestPoint[3];
				
				status = NavQuery.projectedPointOnPoly(HitPolys[i], &RcPoint.X, ClosestPoint);
				if (dtStatusSucceed(status))
				{
					float ExactZ = 0.0f;
					status = NavQuery.getPolyHeight(HitPolys[i], ClosestPoint, &ExactZ);
					if (dtStatusSucceed(status))
					{
						FNavLocation HitLocation(Recast2UnrealPoint(ClosestPoint), HitPolys[i]);
						HitLocation.Location.Z = ExactZ;

						ensure((HitLocation.Location - AdjustedPoint).SizeSquared2D() < KINDA_SMALL_NUMBER);

						Result.Add(HitLocation);
						bSuccess = true;
					}
				}
			}
		}
	}

	return bSuccess;
}

NavNodeRef FPImplRecastNavMesh::FindNearestPoly(FVector const& Loc, FVector const& Extent, const FNavigationQueryFilter& Filter, const UObject* Owner) const
{
	// sanity check
	if (DetourNavMesh == NULL)
	{
		return INVALID_NAVNODEREF;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	// inits to "pass all"
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter)
	{
		float RecastLoc[3];
		Unr2RecastVector(Loc, RecastLoc);
		float RecastExtent[3];
		Unr2RecastSizeVector(NavMeshOwner->GetModifiedQueryExtent(Extent), RecastExtent);

		NavNodeRef OutRef;
		dtStatus Status = NavQuery.findNearestPoly(RecastLoc, RecastExtent, QueryFilter, &OutRef, NULL);
		if (dtStatusSucceed(Status))
		{
			return OutRef;
		}
	}

	return INVALID_NAVNODEREF;
}

bool FPImplRecastNavMesh::GetPolysWithinPathingDistance(FVector const& StartLoc, const float PathingDistance, 
	const FNavigationQueryFilter& Filter, const UObject* Owner,
	TArray<NavNodeRef>& FoundPolys, FRecastDebugPathfindingData* DebugData) const
{
	ensure(PathingDistance > 0.0f && "PathingDistance <= 0 doesn't make sense");

	// sanity check
	if (DetourNavMesh == NULL)
	{
		return false;
	}

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(NavMeshOwner->GetWorld()), Owner);
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes(), LinkFilter);

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter == NULL)
	{
		return false;
	}

	// @todo this should be configurable in some kind of FindPathQuery structure
	const FVector NavExtent = NavMeshOwner->GetModifiedQueryExtent(NavMeshOwner->GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	float RecastStartPos[3];
	Unr2RecastVector(StartLoc, RecastStartPos);
	// @TODO add failure handling
	NavNodeRef StartPolyID = INVALID_NAVNODEREF;
	NavQuery.findNearestPoly(RecastStartPos, Extent, QueryFilter, &StartPolyID, NULL);
		
	FoundPolys.AddUninitialized(Filter.GetMaxSearchNodes());
	int32 NumPolys;

	dtStatus Status = NavQuery.findPolysInPathDistance(StartPolyID, RecastStartPos
		, PathingDistance, QueryFilter, FoundPolys.GetData(), &NumPolys, Filter.GetMaxSearchNodes());

	FoundPolys.RemoveAt(NumPolys, FoundPolys.Num() - NumPolys);

	if (DebugData)
	{
		StorePathfindingDebugData(NavQuery, DetourNavMesh, *DebugData);
	}

	return FoundPolys.Num() > 0;
}

void FPImplRecastNavMesh::UpdateNavigationLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const
{
	if (DetourNavMesh)
	{
		DetourNavMesh->updateOffMeshConnectionByUserId(UserId, AreaType, PolyFlags);
	}
}

void FPImplRecastNavMesh::UpdateSegmentLinkArea(int32 UserId, uint8 AreaType, uint16 PolyFlags) const
{
	if (DetourNavMesh)
	{
		DetourNavMesh->updateOffMeshSegmentConnectionByUserId(UserId, AreaType, PolyFlags);
	}
}

bool FPImplRecastNavMesh::GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const
{
	if (DetourNavMesh)
	{
		// get poly data from recast
		dtPoly const* Poly;
		dtMeshTile const* Tile;
		dtStatus Status = DetourNavMesh->getTileAndPolyByRef((dtPolyRef)PolyID, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			// average verts
			float Center[3] = {0,0,0};

			for (uint32 VertIdx=0; VertIdx < Poly->vertCount; ++VertIdx)
			{
				const float* V = &Tile->verts[Poly->verts[VertIdx]*3];
				Center[0] += V[0];
				Center[1] += V[1];
				Center[2] += V[2];
			}
			const float InvCount = 1.0f / Poly->vertCount;
			Center[0] *= InvCount;
			Center[1] *= InvCount;
			Center[2] *= InvCount;

			// convert output to UE4 coords
			OutCenter = Recast2UnrVector(Center);

			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetPolyVerts(NavNodeRef PolyID, TArray<FVector>& OutVerts) const
{
	if (DetourNavMesh)
	{
		// get poly data from recast
		dtPoly const* Poly;
		dtMeshTile const* Tile;
		dtStatus Status = DetourNavMesh->getTileAndPolyByRef((dtPolyRef)PolyID, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			// flush and pre-size output array
			OutVerts.Reset(Poly->vertCount);

			// convert to UE4 coords and copy verts into output array 
			for (uint32 VertIdx=0; VertIdx < Poly->vertCount; ++VertIdx)
			{
				const float* V = &Tile->verts[Poly->verts[VertIdx]*3];
				OutVerts.Add( Recast2UnrVector(V) );
			}

			return true;
		}
	}

	return false;
}

uint32 FPImplRecastNavMesh::GetPolyAreaID(NavNodeRef PolyID) const
{
	uint32 AreaID = RECAST_NULL_AREA;

	if (DetourNavMesh)
	{
		// get poly data from recast
		dtPoly const* Poly;
		dtMeshTile const* Tile;
		dtStatus Status = DetourNavMesh->getTileAndPolyByRef((dtPolyRef)PolyID, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			AreaID = Poly->getArea();
		}
	}

	return AreaID;
}

void FPImplRecastNavMesh::SetPolyAreaID(NavNodeRef PolyID, uint8 AreaID)
{
	if (DetourNavMesh)
	{
		DetourNavMesh->setPolyArea((dtPolyRef)PolyID, AreaID);
	}
}

bool FPImplRecastNavMesh::GetPolyData(NavNodeRef PolyID, uint16& Flags, uint8& AreaType) const
{
	if (DetourNavMesh)
	{
		// get poly data from recast
		dtPoly const* Poly;
		dtMeshTile const* Tile;
		dtStatus Status = DetourNavMesh->getTileAndPolyByRef((dtPolyRef)PolyID, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			Flags = Poly->flags;
			AreaType = Poly->getArea();
			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetPolyNeighbors(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const
{
	if (DetourNavMesh)
	{
		dtPolyRef PolyRef = (dtPolyRef)PolyID;
		dtPoly const* Poly = 0;
		dtMeshTile const* Tile = 0;

		dtStatus Status = DetourNavMesh->getTileAndPolyByRef(PolyRef, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			INITIALIZE_NAVQUERY_SIMPLE(NavQuery, RECAST_MAX_SEARCH_NODES);

			float RcLeft[3], RcRight[3];
			uint8 DummyType1, DummyType2;

			uint32 LinkIdx = Poly->firstLink;
			while (LinkIdx != DT_NULL_LINK)
			{
				const dtLink& Link = DetourNavMesh->getLink(Tile, LinkIdx);
				LinkIdx = Link.next;
				
				Status = NavQuery.getPortalPoints(PolyRef, Link.ref, RcLeft, RcRight, DummyType1, DummyType2);
				if (dtStatusSucceed(Status))
				{
					FNavigationPortalEdge NeiData;
					NeiData.ToRef = Link.ref;
					NeiData.Left = Recast2UnrealPoint(RcLeft);
					NeiData.Right = Recast2UnrealPoint(RcRight);

					Neighbors.Add(NeiData);
				}
			}

			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetPolyNeighbors(NavNodeRef PolyID, TArray<NavNodeRef>& Neighbors) const
{
	if (DetourNavMesh)
	{
		const dtPolyRef PolyRef = static_cast<dtPolyRef>(PolyID);
		dtPoly const* Poly = 0;
		dtMeshTile const* Tile = 0;

		const dtStatus Status = DetourNavMesh->getTileAndPolyByRef(PolyRef, &Tile, &Poly);

		if (dtStatusSucceed(Status))
		{
			uint32 LinkIdx = Poly->firstLink;
			Neighbors.Reserve(DT_VERTS_PER_POLYGON);

			while (LinkIdx != DT_NULL_LINK)
			{
				const dtLink& Link = DetourNavMesh->getLink(Tile, LinkIdx);
				LinkIdx = Link.next;

				Neighbors.Add(Link.ref);
			}

			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetPolyEdges(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Edges) const
{
	if (DetourNavMesh)
	{
		dtPolyRef PolyRef = (dtPolyRef)PolyID;
		dtPoly const* Poly = 0;
		dtMeshTile const* Tile = 0;

		dtStatus Status = DetourNavMesh->getTileAndPolyByRef(PolyRef, &Tile, &Poly);
		if (dtStatusSucceed(Status))
		{
			const bool bIsNavLink = (Poly->getType() != DT_POLYTYPE_GROUND);

			for (uint32 LinkIt = Poly->firstLink; LinkIt != DT_NULL_LINK;)
			{
				const dtLink& LinkInfo = DetourNavMesh->getLink(Tile, LinkIt);
				if (LinkInfo.edge >= 0 && LinkInfo.edge < Poly->vertCount)
				{
					FNavigationPortalEdge NeiData;
					NeiData.Left = Recast2UnrealPoint(&Tile->verts[3 * Poly->verts[LinkInfo.edge]]);
					NeiData.Right = bIsNavLink ? NeiData.Left : Recast2UnrealPoint(&Tile->verts[3 * Poly->verts[(LinkInfo.edge + 1) % Poly->vertCount]]);
					NeiData.ToRef = LinkInfo.ref;
					Edges.Add(NeiData);
				}

				LinkIt = LinkInfo.next;
			}

			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetPolyTileIndex(NavNodeRef PolyID, uint32& PolyIndex, uint32& TileIndex) const
{
	if (DetourNavMesh && PolyID)
	{
		uint32 SaltIdx = 0;
		DetourNavMesh->decodePolyId(PolyID, SaltIdx, TileIndex, PolyIndex);
		return true;
	}

	return false;
}

bool FPImplRecastNavMesh::GetClosestPointOnPoly(NavNodeRef PolyID, const FVector& TestPt, FVector& PointOnPoly) const
{
	if (DetourNavMesh && PolyID)
	{
		INITIALIZE_NAVQUERY_SIMPLE(NavQuery, RECAST_MAX_SEARCH_NODES);

		float RcTestPos[3] = { 0.0f };
		float RcClosestPos[3] = { 0.0f };
		Unr2RecastVector(TestPt, RcTestPos);

		const dtStatus Status = NavQuery.closestPointOnPoly(PolyID, RcTestPos, RcClosestPos);
		if (dtStatusSucceed(Status))
		{
			PointOnPoly = Recast2UnrealPoint(RcClosestPos);
			return true;
		}
	}

	return false;
}

uint32 FPImplRecastNavMesh::GetLinkUserId(NavNodeRef LinkPolyID) const
{
	uint32 UserID = 0;
	if (DetourNavMesh)
	{
		const dtOffMeshConnection* offmeshCon = DetourNavMesh->getOffMeshConnectionByRef(LinkPolyID);
		if (offmeshCon)
		{
			UserID = offmeshCon->userId;
		}
	}

	return UserID;
}

bool FPImplRecastNavMesh::GetLinkEndPoints(NavNodeRef LinkPolyID, FVector& PointA, FVector& PointB) const
{
	if (DetourNavMesh)
	{
		float RcPointA[3] = { 0 };
		float RcPointB[3] = { 0 };
		
		dtStatus status = DetourNavMesh->getOffMeshConnectionPolyEndPoints(0, LinkPolyID, 0, RcPointA, RcPointB);
		if (dtStatusSucceed(status))
		{
			PointA = Recast2UnrealPoint(RcPointA);
			PointB = Recast2UnrealPoint(RcPointB);
			return true;
		}
	}

	return false;
}

bool FPImplRecastNavMesh::IsCustomLink(NavNodeRef PolyRef) const
{
	if (DetourNavMesh)
	{
		const dtOffMeshConnection* offMeshCon = DetourNavMesh->getOffMeshConnectionByRef(PolyRef);
		return offMeshCon && offMeshCon->userId;
	}

	return false;
}

bool FPImplRecastNavMesh::GetClusterBounds(NavNodeRef ClusterRef, FBox& OutBounds) const
{
	if (DetourNavMesh == NULL || !ClusterRef)
	{
		return false;
	}

	const dtMeshTile* Tile = DetourNavMesh->getTileByRef(ClusterRef);
	uint32 ClusterIdx = DetourNavMesh->decodeClusterIdCluster(ClusterRef);

	int32 NumPolys = 0;
	if (Tile && ClusterIdx < (uint32)Tile->header->clusterCount)
	{
		for (int32 i = 0; i < Tile->header->offMeshBase; i++)
		{
			if (Tile->polyClusters[i] == ClusterIdx)
			{
				const dtPoly* Poly = &Tile->polys[i];
				for (int32 iVert = 0; iVert < Poly->vertCount; iVert++)
				{
					const float* V = &Tile->verts[Poly->verts[iVert]*3];
					OutBounds += Recast2UnrealPoint(V);
				}

				NumPolys++;
			}
		}
	}

	return NumPolys > 0;
}

FORCEINLINE void FPImplRecastNavMesh::GetEdgesForPathCorridorImpl(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges, const dtNavMeshQuery& NavQuery) const
{
	const int32 CorridorLenght = PathCorridor->Num();

	PathCorridorEdges->Empty(CorridorLenght - 1);
	for (int32 i = 0; i < CorridorLenght - 1; ++i)
	{
		unsigned char FromType = 0, ToType = 0;
		float Left[3] = {0.f}, Right[3] = {0.f};

		NavQuery.getPortalPoints((*PathCorridor)[i], (*PathCorridor)[i+1], Left, Right, FromType, ToType);

		PathCorridorEdges->Add(FNavigationPortalEdge(Recast2UnrVector(Left), Recast2UnrVector(Right), (*PathCorridor)[i+1]));
	}
}

void FPImplRecastNavMesh::GetEdgesForPathCorridor(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges) const
{
	// sanity check
	if (DetourNavMesh == NULL)
	{
		return;
	}

	INITIALIZE_NAVQUERY_SIMPLE(NavQuery, RECAST_MAX_SEARCH_NODES);

	GetEdgesForPathCorridorImpl(PathCorridor, PathCorridorEdges, NavQuery);
}

bool FPImplRecastNavMesh::FilterPolys(TArray<NavNodeRef>& PolyRefs, const FRecastQueryFilter* Filter, const UObject* Owner) const
{
	if (Filter == NULL || DetourNavMesh == NULL)
	{
		return false;
	}

	for (int32 PolyIndex = PolyRefs.Num() - 1; PolyIndex >= 0; --PolyIndex)
	{
		dtPolyRef TestRef = PolyRefs[PolyIndex];

		// get poly data from recast
		dtPoly const* Poly = NULL;
		dtMeshTile const* Tile = NULL;
		const dtStatus Status = DetourNavMesh->getTileAndPolyByRef(TestRef, &Tile, &Poly);

		if (dtStatusSucceed(Status))
		{
			const bool bPassedFilter = Filter->passFilter(TestRef, Tile, Poly);
			const bool bWalkableArea = Filter->getAreaCost(Poly->getArea()) > 0.0f;
			if (bPassedFilter && bWalkableArea)
			{
				continue;
			}
		}
		
		PolyRefs.RemoveAt(PolyIndex, 1);
	}

	return true;
}

bool FPImplRecastNavMesh::GetPolysInTile(int32 TileIndex, TArray<FNavPoly>& Polys) const
{
	if (DetourNavMesh == NULL || TileIndex < 0 || TileIndex >= DetourNavMesh->getMaxTiles())
	{
		return false;
	}

	const dtMeshTile* Tile = ((const dtNavMesh*)DetourNavMesh)->getTile(TileIndex);
	const int32 MaxPolys = Tile && Tile->header ? Tile->header->offMeshBase : 0;
	if (MaxPolys > 0)
	{
		// only ground type polys
		int32 BaseIdx = Polys.Num();
		Polys.AddZeroed(MaxPolys);

		dtPoly* Poly = Tile->polys;
		for (int32 i = 0; i < MaxPolys; i++, Poly++)
		{
			FVector PolyCenter(0);
			for (int k = 0; k < Poly->vertCount; ++k)
			{
				PolyCenter += Recast2UnrealPoint(&Tile->verts[Poly->verts[k]*3]);
			}
			PolyCenter /= Poly->vertCount;

			FNavPoly& OutPoly = Polys[BaseIdx + i];
			OutPoly.Ref = DetourNavMesh->encodePolyId(Tile->salt, TileIndex, i);
			OutPoly.Center = PolyCenter;
		}
	}

	return (MaxPolys > 0);
}

/** Internal. Calculates squared 2d distance of given point PT to segment P-Q. Values given in Recast coordinates */
static FORCEINLINE float PointDistToSegment2DSquared(const float* PT, const float* P, const float* Q)
{
	float pqx = Q[0] - P[0];
	float pqz = Q[2] - P[2];
	float dx = PT[0] - P[0];
	float dz = PT[2] - P[2];
	float d = pqx*pqx + pqz*pqz;
	float t = pqx*dx + pqz*dz;
	if (d != 0) t /= d;
	dx = P[0] + t*pqx - PT[0];
	dz = P[2] + t*pqz - PT[2];
	return dx*dx + dz*dz;
}

/** 
 * Traverses given tile's edges and detects the ones that are either poly (i.e. not triangle, but whole navmesh polygon) 
 * or navmesh edge. Returns a pair of verts for each edge found.
 */
void FPImplRecastNavMesh::GetDebugPolyEdges(const dtMeshTile& Tile, bool bInternalEdges, bool bNavMeshEdges, TArray<FVector>& InternalEdgeVerts, TArray<FVector>& NavMeshEdgeVerts) const
{
	static const float thr = FMath::Square(0.01f);

	ensure(bInternalEdges || bNavMeshEdges);
	const bool bExportAllEdges = bInternalEdges && !bNavMeshEdges;
	
	for (int i = 0; i < Tile.header->polyCount; ++i)
	{
		const dtPoly* Poly = &Tile.polys[i];

		if (Poly->getType() != DT_POLYTYPE_GROUND)
		{
			continue;
		}

		const dtPolyDetail* pd = &Tile.detailMeshes[i];
		for (int j = 0, nj = (int)Poly->vertCount; j < nj; ++j)
		{
			bool bIsExternal = !bExportAllEdges && (Poly->neis[j] == 0 || Poly->neis[j] & DT_EXT_LINK);
			bool bIsConnected = !bIsExternal;

			if (Poly->getArea() == RECAST_NULL_AREA)
			{
				if (Poly->neis[j] && !(Poly->neis[j] & DT_EXT_LINK) &&
					Poly->neis[j] <= Tile.header->offMeshBase &&
					Tile.polys[Poly->neis[j] - 1].getArea() != RECAST_NULL_AREA)
				{
					bIsExternal = true;
					bIsConnected = false;
				}
				else if (Poly->neis[j] == 0)
				{
					bIsExternal = true;
					bIsConnected = false;
				}
			}
			else if (bIsExternal)
			{
				unsigned int k = Poly->firstLink;
				while (k != DT_NULL_LINK)
				{
					const dtLink& link = DetourNavMesh->getLink(&Tile, k);
					k = link.next;

					if (link.edge == j)
					{
						bIsConnected = true;
						break;
					}
				}
			}

			TArray<FVector>* EdgeVerts = bInternalEdges && bIsConnected ? &InternalEdgeVerts 
				: (bNavMeshEdges && bIsExternal && !bIsConnected ? &NavMeshEdgeVerts : NULL);
			if (EdgeVerts == NULL)
			{
				continue;
			}

			const float* V0 = &Tile.verts[Poly->verts[j] * 3];
			const float* V1 = &Tile.verts[Poly->verts[(j + 1) % nj] * 3];

			// Draw detail mesh edges which align with the actual poly edge.
			// This is really slow.
			for (int32 k = 0; k < pd->triCount; ++k)
			{
				const unsigned char* t = &(Tile.detailTris[(pd->triBase + k) * 4]);
				const float* tv[3];

				for (int32 m = 0; m < 3; ++m)
				{
					if (t[m] < Poly->vertCount)
					{
						tv[m] = &Tile.verts[Poly->verts[t[m]] * 3];
					}
					else
					{
						tv[m] = &Tile.detailVerts[(pd->vertBase + (t[m] - Poly->vertCount)) * 3];
					}
				}
				for (int m = 0, n = 2; m < 3; n=m++)
				{
					if (((t[3] >> (n*2)) & 0x3) == 0)
					{
						continue;	// Skip inner detail edges.
					}
					
					if (PointDistToSegment2DSquared(tv[n],V0,V1) < thr && PointDistToSegment2DSquared(tv[m],V0,V1) < thr)
					{
						int32 const AddIdx = (*EdgeVerts).AddZeroed(2);
						(*EdgeVerts)[AddIdx] = Recast2UnrVector(tv[n]);
						(*EdgeVerts)[AddIdx+1] = Recast2UnrVector(tv[m]);
					}
				}
			}
		}
	}
}

uint8 GetValidEnds(const dtNavMesh& NavMesh, const dtMeshTile& Tile, const dtPoly& Poly)
{
	if (Poly.getType() == DT_POLYTYPE_GROUND)
	{
		return false;
	}

	uint8 ValidEnds = FRecastDebugGeometry::OMLE_None;

	unsigned int k = Poly.firstLink;
	while (k != DT_NULL_LINK)
	{
		const dtLink& link = NavMesh.getLink(&Tile, k);
		k = link.next;

		if (link.edge == 0)
		{
			ValidEnds |= FRecastDebugGeometry::OMLE_Left;
		}
		if (link.edge == 1)
		{
			ValidEnds |= FRecastDebugGeometry::OMLE_Right;
		}
	}

	return ValidEnds;
}

/** 
 * @param PolyEdges			[out] Array of worldspace vertex locations for tile edges.  Edges are pairwise verts, i.e. [0,1], [2,3], etc
 * @param NavMeshEdges		[out] Array of worldspace vertex locations for the edge of the navmesh.  Edges are pairwise verts, i.e. [0,1], [2,3], etc
 * 
 * @todo PolyEdges and NavMeshEdges could probably be Index arrays into MeshVerts and be generated in the master loop instead of separate traversals. 
 */
void FPImplRecastNavMesh::GetDebugGeometry(FRecastDebugGeometry& OutGeometry, int32 TileIndex) const
{
	if (DetourNavMesh == nullptr || TileIndex >= DetourNavMesh->getMaxTiles())
	{
		return;
	}
				
	check(NavMeshOwner);

	const dtNavMesh* const ConstNavMesh = DetourNavMesh;
		
	// presize our tarrays for efficiency
	const int32 NumTiles = TileIndex == INDEX_NONE ? ConstNavMesh->getMaxTiles() : TileIndex + 1;
	const int32 StartingTile = TileIndex == INDEX_NONE ? 0 : TileIndex;

	int32 NumVertsToReserve = 0;
	int32 NumIndicesToReserve = 0;

	const FRecastNavMeshGenerator* Generator = static_cast<const FRecastNavMeshGenerator*>(NavMeshOwner->GetGenerator());

	if (Generator && Generator->IsBuildingRestrictedToActiveTiles())
	{
		const TArray<FIntPoint>& ActiveTiles = NavMeshOwner->GetActiveTiles();
		for (const FIntPoint& TileLocation : ActiveTiles)
		{
			const int32 LayersCount = ConstNavMesh->getTileCountAt(TileLocation.X, TileLocation.Y);

			for (int32 Layer = 0; Layer < LayersCount; ++Layer)
			{
				dtMeshTile const* const Tile = ConstNavMesh->getTileAt(TileLocation.X, TileLocation.Y, Layer);
				if (Tile != nullptr && Tile->header != nullptr)
				{
					NumVertsToReserve += Tile->header->vertCount + Tile->header->detailVertCount;

					for (int32 PolyIdx = 0; PolyIdx < Tile->header->polyCount; ++PolyIdx)
					{
						dtPolyDetail const* const DetailPoly = &Tile->detailMeshes[PolyIdx];
						NumIndicesToReserve += (DetailPoly->triCount * 3);
					}
				}
			}
		}

		OutGeometry.MeshVerts.Reserve(OutGeometry.MeshVerts.Num() + NumVertsToReserve);
		OutGeometry.AreaIndices[0].Reserve(OutGeometry.AreaIndices[0].Num() + NumIndicesToReserve);
		OutGeometry.BuiltMeshIndices.Reserve(OutGeometry.BuiltMeshIndices.Num() + NumIndicesToReserve);

		uint32 VertBase = OutGeometry.MeshVerts.Num();
		for (const FIntPoint& TileLocation : ActiveTiles)
		{
			const int32 LayersCount = ConstNavMesh->getTileCountAt(TileLocation.X, TileLocation.Y);

			for (int32 Layer = 0; Layer < LayersCount; ++Layer)
			{
				dtMeshTile const* const Tile = ConstNavMesh->getTileAt(TileLocation.X, TileLocation.Y, Layer);
				if (Tile != nullptr && Tile->header != nullptr)
				{
					VertBase += GetTilesDebugGeometry(Generator, *Tile, VertBase, OutGeometry);
				}
			}
		}
	}
	else
	{
		for (int32 TileIdx = StartingTile; TileIdx < NumTiles; ++TileIdx)
		{
			dtMeshTile const* const Tile = ConstNavMesh->getTile(TileIdx);
			dtMeshHeader const* const Header = Tile->header;

			if (Header != NULL)
			{
				NumVertsToReserve += Header->vertCount + Header->detailVertCount;

				for (int32 PolyIdx = 0; PolyIdx < Header->polyCount; ++PolyIdx)
				{
					dtPolyDetail const* const DetailPoly = &Tile->detailMeshes[PolyIdx];
					NumIndicesToReserve += (DetailPoly->triCount * 3);
				}
			}
		}

		OutGeometry.MeshVerts.Reserve(OutGeometry.MeshVerts.Num() + NumVertsToReserve);
		OutGeometry.AreaIndices[0].Reserve(OutGeometry.AreaIndices[0].Num() + NumIndicesToReserve);
		OutGeometry.BuiltMeshIndices.Reserve(OutGeometry.BuiltMeshIndices.Num() + NumIndicesToReserve);

		uint32 VertBase = OutGeometry.MeshVerts.Num();
		for (int32 TileIdx = StartingTile; TileIdx < NumTiles; ++TileIdx)
		{
			dtMeshTile const* const Tile = ConstNavMesh->getTile(TileIdx);

			if (Tile == nullptr || Tile->header == nullptr)
			{
				continue;
			}

			VertBase += GetTilesDebugGeometry(Generator, *Tile, VertBase, OutGeometry, TileIdx);
		}
	}
}

int32 FPImplRecastNavMesh::GetTilesDebugGeometry(const FRecastNavMeshGenerator* Generator, const dtMeshTile& Tile, int32 VertBase, FRecastDebugGeometry& OutGeometry, int32 TileIdx) const
{
	check(NavMeshOwner && DetourNavMesh);
	dtMeshHeader const* const Header = Tile.header;
	check(Header);
	
	const bool bIsBeingBuilt = Generator != nullptr && !!NavMeshOwner->bDistinctlyDrawTilesBeingBuilt
		&& Generator->IsTileChanged(TileIdx == INDEX_NONE ? DetourNavMesh->decodePolyIdTile(DetourNavMesh->getTileRef(&Tile)) : TileIdx);

	// add all the poly verts
	float* F = Tile.verts;
	for (int32 VertIdx = 0; VertIdx < Header->vertCount; ++VertIdx)
	{
		FVector const VertPos = Recast2UnrVector(F);
		OutGeometry.MeshVerts.Add(VertPos);
		F += 3;
	}
	int32 const DetailVertIndexBase = Header->vertCount;
	// add the detail verts
	F = Tile.detailVerts;
	for (int32 DetailVertIdx = 0; DetailVertIdx < Header->detailVertCount; ++DetailVertIdx)
	{
		FVector const VertPos = Recast2UnrVector(F);
		OutGeometry.MeshVerts.Add(VertPos);
		F += 3;
	}

	// add all the indices
	for (int32 PolyIdx = 0; PolyIdx < Header->polyCount; ++PolyIdx)
	{
		dtPoly const* const Poly = &Tile.polys[PolyIdx];

		if (Poly->getType() == DT_POLYTYPE_GROUND)
		{
			dtPolyDetail const* const DetailPoly = &Tile.detailMeshes[PolyIdx];

			TArray<int32>* Indices = bIsBeingBuilt ? &OutGeometry.BuiltMeshIndices : &OutGeometry.AreaIndices[Poly->getArea()];

			// one triangle at a time
			for (int32 TriIdx = 0; TriIdx < DetailPoly->triCount; ++TriIdx)
			{
				int32 DetailTriIdx = (DetailPoly->triBase + TriIdx) * 4;
				const unsigned char* DetailTri = &Tile.detailTris[DetailTriIdx];

				// calc indices into the vert buffer we just populated
				int32 TriVertIndices[3];
				for (int32 TriVertIdx = 0; TriVertIdx < 3; ++TriVertIdx)
				{
					if (DetailTri[TriVertIdx] < Poly->vertCount)
					{
						TriVertIndices[TriVertIdx] = VertBase + Poly->verts[DetailTri[TriVertIdx]];
					}
					else
					{
						TriVertIndices[TriVertIdx] = VertBase + DetailVertIndexBase + (DetailPoly->vertBase + DetailTri[TriVertIdx] - Poly->vertCount);
					}
				}

				Indices->Add(TriVertIndices[0]);
				Indices->Add(TriVertIndices[1]);
				Indices->Add(TriVertIndices[2]);

				if (Tile.polyClusters)
				{
					const uint16 ClusterId = Tile.polyClusters[PolyIdx];
					if (ClusterId < MAX_uint8)
					{
						if (ClusterId >= OutGeometry.Clusters.Num())
						{
							OutGeometry.Clusters.AddDefaulted(ClusterId - OutGeometry.Clusters.Num() + 1);
						}

						TArray<int32>& ClusterIndices = OutGeometry.Clusters[ClusterId].MeshIndices;
						ClusterIndices.Add(TriVertIndices[0]);
						ClusterIndices.Add(TriVertIndices[1]);
						ClusterIndices.Add(TriVertIndices[2]);
					}
				}
			}
		}
	}

	for (int32 i = 0; i < Header->offMeshConCount; ++i)
	{
		const dtOffMeshConnection* OffMeshConnection = &Tile.offMeshCons[i];

		if (OffMeshConnection != NULL)
		{
			dtPoly const* const LinkPoly = &Tile.polys[OffMeshConnection->poly];
			const float* va = &Tile.verts[LinkPoly->verts[0] * 3]; //OffMeshConnection->pos;
			const float* vb = &Tile.verts[LinkPoly->verts[1] * 3]; //OffMeshConnection->pos[3];

			const FRecastDebugGeometry::FOffMeshLink Link = {
				Recast2UnrVector(va)
				, Recast2UnrVector(vb)
				, LinkPoly->getArea()
				, (uint8)OffMeshConnection->getBiDirectional()
				, GetValidEnds(*DetourNavMesh, Tile, *LinkPoly)
				, OffMeshConnection->rad
			};

			OutGeometry.OffMeshLinks.Add(Link);
		}
	}

	for (int32 i = 0; i < Header->offMeshSegConCount; ++i)
	{
		const dtOffMeshSegmentConnection* OffMeshSeg = &Tile.offMeshSeg[i];
		if (OffMeshSeg != NULL)
		{
			const int32 polyBase = Header->offMeshSegPolyBase + OffMeshSeg->firstPoly;
			for (int32 j = 0; j < OffMeshSeg->npolys; j++)
			{
				dtPoly const* const LinkPoly = &Tile.polys[polyBase + j];

				FRecastDebugGeometry::FOffMeshSegment Link;
				Link.LeftStart = Recast2UnrealPoint(&Tile.verts[LinkPoly->verts[0] * 3]);
				Link.LeftEnd = Recast2UnrealPoint(&Tile.verts[LinkPoly->verts[1] * 3]);
				Link.RightStart = Recast2UnrealPoint(&Tile.verts[LinkPoly->verts[2] * 3]);
				Link.RightEnd = Recast2UnrealPoint(&Tile.verts[LinkPoly->verts[3] * 3]);
				Link.AreaID = LinkPoly->getArea();
				Link.Direction = (uint8)OffMeshSeg->getBiDirectional();
				Link.ValidEnds = GetValidEnds(*DetourNavMesh, Tile, *LinkPoly);

				const int LinkIdx = OutGeometry.OffMeshSegments.Add(Link);
				OutGeometry.OffMeshSegmentAreas[Link.AreaID].Add(LinkIdx);
			}
		}
	}

	for (int32 i = 0; i < Header->clusterCount; i++)
	{
		const dtCluster& c0 = Tile.clusters[i];
		uint32 iLink = c0.firstLink;
		while (iLink != DT_NULL_LINK)
		{
			const dtClusterLink& link = DetourNavMesh->getClusterLink(&Tile, iLink);
			iLink = link.next;

			dtMeshTile const* const OtherTile = DetourNavMesh->getTileByRef(link.ref);
			if (OtherTile)
			{
				int32 linkedIdx = DetourNavMesh->decodeClusterIdCluster(link.ref);
				const dtCluster& c1 = OtherTile->clusters[linkedIdx];

				FRecastDebugGeometry::FClusterLink LinkGeom;
				LinkGeom.FromCluster = Recast2UnrealPoint(c0.center);
				LinkGeom.ToCluster = Recast2UnrealPoint(c1.center);

				if (linkedIdx > i || TileIdx > (int32)DetourNavMesh->decodeClusterIdTile(link.ref))
				{
					FVector UpDir(0, 0, 1.0f);
					FVector LinkDir = (LinkGeom.ToCluster - LinkGeom.FromCluster).GetSafeNormal();
					FVector SideDir = FVector::CrossProduct(LinkDir, UpDir);
					LinkGeom.FromCluster += SideDir * 40.0f;
					LinkGeom.ToCluster += SideDir * 40.0f;
				}

				OutGeometry.ClusterLinks.Add(LinkGeom);
			}
		}
	}

	// Get tile edges and navmesh edges
	if (OutGeometry.bGatherPolyEdges || OutGeometry.bGatherNavMeshEdges)
	{
		GetDebugPolyEdges(Tile, !!OutGeometry.bGatherPolyEdges, !!OutGeometry.bGatherNavMeshEdges
			, OutGeometry.PolyEdges, OutGeometry.NavMeshEdges);
	}

	return Header->vertCount + Header->detailVertCount;
}

FBox FPImplRecastNavMesh::GetNavMeshBounds() const
{
	FBox Bbox(ForceInit);

	// @todo, calc once and cache it
	if (DetourNavMesh)
	{
		// workaround for privacy issue in the recast API
		dtNavMesh const* const ConstRecastNavMesh = DetourNavMesh;

		// spin through all the tiles and accumulate the bounds
		for (int32 TileIdx=0; TileIdx < DetourNavMesh->getMaxTiles(); ++TileIdx)
		{
			dtMeshTile const* const Tile = ConstRecastNavMesh->getTile(TileIdx);
			if (Tile)
			{
				dtMeshHeader const* const Header = Tile->header;
				if (Header)
				{
					const FBox NodeBox = Recast2UnrealBox(Header->bmin, Header->bmax);
					Bbox += NodeBox;
				}
			}
		}
	}

	return Bbox;
}

FBox FPImplRecastNavMesh::GetNavMeshTileBounds(int32 TileIndex) const
{
	FBox Bbox(ForceInit);

	if (DetourNavMesh && TileIndex >= 0 && TileIndex < DetourNavMesh->getMaxTiles())
	{
		// workaround for privacy issue in the recast API
		dtNavMesh const* const ConstRecastNavMesh = DetourNavMesh;

		dtMeshTile const* const Tile = ConstRecastNavMesh->getTile(TileIndex);
		if (Tile)
		{
			dtMeshHeader const* const Header = Tile->header;
			if (Header)
			{
				Bbox = Recast2UnrealBox(Header->bmin, Header->bmax);
			}
		}
	}

	return Bbox;
}

/** Retrieves XY coordinates of tile specified by index */
bool FPImplRecastNavMesh::GetNavMeshTileXY(int32 TileIndex, int32& OutX, int32& OutY, int32& OutLayer) const
{
	if (DetourNavMesh && TileIndex >= 0 && TileIndex < DetourNavMesh->getMaxTiles())
	{
		// workaround for privacy issue in the recast API
		dtNavMesh const* const ConstRecastNavMesh = DetourNavMesh;

		dtMeshTile const* const Tile = ConstRecastNavMesh->getTile(TileIndex);
		if (Tile)
		{
			dtMeshHeader const* const Header = Tile->header;
			if (Header)
			{
				OutX = Header->x;
				OutY = Header->y;
				OutLayer = Header->layer;
				return true;
			}
		}
	}

	return false;
}

bool FPImplRecastNavMesh::GetNavMeshTileXY(const FVector& Point, int32& OutX, int32& OutY) const
{
	if (DetourNavMesh)
	{
		// workaround for privacy issue in the recast API
		dtNavMesh const* const ConstRecastNavMesh = DetourNavMesh;

		const FVector RecastPt = Unreal2RecastPoint(Point);
		int32 TileX = 0;
		int32 TileY = 0;

		ConstRecastNavMesh->calcTileLoc(&RecastPt.X, &TileX, &TileY);
		OutX = TileX;
		OutY = TileY;
		return true;
	}

	return false;
}

void FPImplRecastNavMesh::GetNavMeshTilesAt(int32 TileX, int32 TileY, TArray<int32>& Indices) const
{
	if (DetourNavMesh)
	{
		// workaround for privacy issue in the recast API
		dtNavMesh const* const ConstRecastNavMesh = DetourNavMesh;

		const int32 MaxTiles = ConstRecastNavMesh->getTileCountAt(TileX, TileY);
		TArray<const dtMeshTile*> Tiles;
		Tiles.AddZeroed(MaxTiles);

		const int32 NumTiles = ConstRecastNavMesh->getTilesAt(TileX, TileY, Tiles.GetData(), MaxTiles);
		for (int32 i = 0; i < NumTiles; i++)
		{
			dtTileRef TileRef = ConstRecastNavMesh->getTileRef(Tiles[i]);
			if (TileRef)
			{
				const int32 TileIndex = (int32)ConstRecastNavMesh->decodePolyIdTile(TileRef);
				Indices.Add(TileIndex);
			}
		}
	}
}

void FPImplRecastNavMesh::GetNavMeshTilesIn(const TArray<FBox>& InclusionBounds, TArray<int32>& Indices) const
{
	if (DetourNavMesh)
	{
		const float* NavMeshOrigin = DetourNavMesh->getParams()->orig;
		const float TileSize = DetourNavMesh->getParams()->tileWidth;

		// Generate a set of all possible tile coordinates that belong to requested bounds
		TSet<FIntPoint>	TileCoords;	
		for (const FBox& Bounds : InclusionBounds)
		{
			const FBox RcBounds = Unreal2RecastBox(Bounds);
			const int32 XMin = FMath::FloorToInt((RcBounds.Min.X - NavMeshOrigin[0]) / TileSize);
			const int32 XMax = FMath::FloorToInt((RcBounds.Max.X - NavMeshOrigin[0]) / TileSize);
			const int32 YMin = FMath::FloorToInt((RcBounds.Min.Z - NavMeshOrigin[2]) / TileSize);
			const int32 YMax = FMath::FloorToInt((RcBounds.Max.Z - NavMeshOrigin[2]) / TileSize);

			for (int32 y = YMin; y <= YMax; ++y)
			{
				for (int32 x = XMin; x <= XMax; ++x)
				{
					TileCoords.Add(FIntPoint(x, y));
				}
			}
		}

		// We guess that each tile has 3 layers in average
		Indices.Reserve(TileCoords.Num()*3);

		TArray<const dtMeshTile*> MeshTiles;
		MeshTiles.Reserve(3);

		for (const FIntPoint& TileCoord : TileCoords)
		{
			int32 MaxTiles = DetourNavMesh->getTileCountAt(TileCoord.X, TileCoord.Y);
			if (MaxTiles > 0)
			{
				MeshTiles.SetNumZeroed(MaxTiles, false);
				
				const int32 MeshTilesCount = DetourNavMesh->getTilesAt(TileCoord.X, TileCoord.Y, MeshTiles.GetData(), MaxTiles);
				for (int32 i = 0; i < MeshTilesCount; ++i)
				{
					const dtMeshTile* MeshTile = MeshTiles[i];
					dtTileRef TileRef = DetourNavMesh->getTileRef(MeshTile);
					if (TileRef)
					{
						// Consider only mesh tiles that actually belong to a requested bounds
						FBox TileBounds = Recast2UnrealBox(MeshTile->header->bmin, MeshTile->header->bmax);
						for (const FBox& RequestedBounds : InclusionBounds)
						{
							if (TileBounds.Intersect(RequestedBounds))
							{
								int32 TileIndex = (int32)DetourNavMesh->decodePolyIdTile(TileRef);
								Indices.Add(TileIndex);
								break;
							}
						}
					}
				}
			}
		}
	}
}

float FPImplRecastNavMesh::GetTotalDataSize() const
{
	float TotalBytes = sizeof(*this);

	if (DetourNavMesh)
	{
		// iterate all tiles and sum up their DataSize
		dtNavMesh const* ConstNavMesh = DetourNavMesh;
		for (int i = 0; i < ConstNavMesh->getMaxTiles(); ++i)
		{
			const dtMeshTile* Tile = ConstNavMesh->getTile(i);
			if (Tile != NULL && Tile->header != NULL)
			{
				TotalBytes += Tile->dataSize;
			}
		}
	}

	return TotalBytes / 1024;
}

void FPImplRecastNavMesh::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	if (DetourNavMesh != NULL)
	{
		// transform offset to Recast space
		const FVector OffsetRC = Unreal2RecastPoint(InOffset);
		// apply offset
		DetourNavMesh->applyWorldOffset(&OffsetRC.X);
	}

}

uint16 FPImplRecastNavMesh::GetFilterForbiddenFlags(const FRecastQueryFilter* Filter)
{
	return ((const dtQueryFilter*)Filter)->getExcludeFlags();
}

void FPImplRecastNavMesh::SetFilterForbiddenFlags(FRecastQueryFilter* Filter, uint16 ForbiddenFlags)
{
	((dtQueryFilter*)Filter)->setExcludeFlags(ForbiddenFlags);
	// include-exclude don't need to be symmetrical, filter will check both conditions
}

void FPImplRecastNavMesh::OnAreaCostChanged()
{
	struct FFloatIntPair
	{
		float Score;
		int32 Index;

		FFloatIntPair() : Score(MAX_FLT), Index(0) {}
		FFloatIntPair(int32 AreaId, float TravelCost, float EntryCost) : Score(TravelCost + EntryCost), Index(AreaId) {}

		bool operator <(const FFloatIntPair& Other) const { return Score < Other.Score; }
	};

	if (NavMeshOwner && DetourNavMesh)
	{
		const INavigationQueryFilterInterface* NavFilter = NavMeshOwner->GetDefaultQueryFilterImpl();
		const dtQueryFilter* DetourFilter = ((const FRecastQueryFilter*)NavFilter)->GetAsDetourQueryFilter();

		TArray<FFloatIntPair> AreaData;
		AreaData.Reserve(RECAST_MAX_AREAS);
		for (int32 Idx = 0; Idx < RECAST_MAX_AREAS; Idx++)
		{
			AreaData.Add(FFloatIntPair(Idx, DetourFilter->getAreaCost(Idx), DetourFilter->getAreaFixedCost(Idx)));
		}

		AreaData.Sort();

		uint8 AreaCostOrder[RECAST_MAX_AREAS];
		for (int32 Idx = 0; Idx < RECAST_MAX_AREAS; Idx++)
		{
			AreaCostOrder[AreaData[Idx].Index] = Idx;
		}

		DetourNavMesh->applyAreaCostOrder(AreaCostOrder);
	}
}

void FPImplRecastNavMesh::RemoveTileCacheLayers(int32 TileX, int32 TileY)
{
	CompressedTileCacheLayers.Remove(FIntPoint(TileX, TileY));
}

void FPImplRecastNavMesh::RemoveTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx)
{
	TArray<FNavMeshTileData>* ExistingLayersList = CompressedTileCacheLayers.Find(FIntPoint(TileX, TileY));
	if (ExistingLayersList)
	{
		if (ExistingLayersList->IsValidIndex(LayerIdx))
		{
			ExistingLayersList->RemoveAt(LayerIdx);

			for (int32 Idx = LayerIdx; Idx < ExistingLayersList->Num(); Idx++)
			{
				(*ExistingLayersList)[Idx].LayerIndex = Idx;
			}
		}
		
		if (ExistingLayersList->Num() == 0)
		{
			CompressedTileCacheLayers.Remove(FIntPoint(TileX, TileY));
		}
	}
}

void FPImplRecastNavMesh::AddTileCacheLayers(int32 TileX, int32 TileY, const TArray<FNavMeshTileData>& Layers)
{
	CompressedTileCacheLayers.Add(FIntPoint(TileX, TileY), Layers);
}

void FPImplRecastNavMesh::AddTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx, const FNavMeshTileData& LayerData)
{
	TArray<FNavMeshTileData>* ExistingLayersList = CompressedTileCacheLayers.Find(FIntPoint(TileX, TileY));
	
	if (ExistingLayersList)
	{
		ExistingLayersList->SetNum(FMath::Max(ExistingLayersList->Num(), LayerIdx + 1));
		(*ExistingLayersList)[LayerIdx] = LayerData;
	}
	else
	{
		TArray<FNavMeshTileData> LayersList;
		LayersList.SetNum(FMath::Max(LayersList.Num(), LayerIdx + 1));
		LayersList[LayerIdx] = LayerData;
		CompressedTileCacheLayers.Add(FIntPoint(TileX, TileY), LayersList);
	}
}

void FPImplRecastNavMesh::MarkEmptyTileCacheLayers(int32 TileX, int32 TileY)
{
	if (!CompressedTileCacheLayers.Contains(FIntPoint(TileX, TileY)))
	{
		TArray<FNavMeshTileData> EmptyLayersList;
		CompressedTileCacheLayers.Add(FIntPoint(TileX, TileY), EmptyLayersList);
	}
}

FNavMeshTileData FPImplRecastNavMesh::GetTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx) const
{
	const TArray<FNavMeshTileData>* LayersList = CompressedTileCacheLayers.Find(FIntPoint(TileX, TileY));
	if (LayersList && LayersList->IsValidIndex(LayerIdx))
	{
		return (*LayersList)[LayerIdx];
	}

	return FNavMeshTileData();
}

TArray<FNavMeshTileData> FPImplRecastNavMesh::GetTileCacheLayers(int32 TileX, int32 TileY) const
{
	return CompressedTileCacheLayers.FindRef(FIntPoint(TileX, TileY));
}

bool FPImplRecastNavMesh::HasTileCacheLayers(int32 TileX, int32 TileY) const
{
	return CompressedTileCacheLayers.Contains(FIntPoint(TileX, TileY));
}

#undef INITIALIZE_NAVQUERY

#endif // WITH_RECAST
