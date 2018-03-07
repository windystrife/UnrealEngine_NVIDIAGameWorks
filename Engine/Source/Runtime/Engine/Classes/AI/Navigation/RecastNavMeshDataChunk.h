// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationDataChunk.h"
#include "RecastNavMeshDataChunk.generated.h"

class FPImplRecastNavMesh;

struct FRecastTileData
{
	struct FRawData
	{
		FRawData(uint8* InData);
		~FRawData();

		uint8* RawData;
	};

	FRecastTileData();
	FRecastTileData(int32 TileDataSize, uint8* TileRawData, int32 TileCacheDataSize, uint8* TileCacheRawData);
	
	// Location of attached tile
	int32					X;					
	int32					Y;					
	int32					Layer;
		
	// Tile data
	int32					TileDataSize;
	TSharedPtr<FRawData>	TileRawData;

	// Compressed tile cache layer 
	int32					TileCacheDataSize;
	TSharedPtr<FRawData>	TileCacheRawData;

	// Whether this tile is attached to NavMesh
	bool					bAttached;	
};

class dtNavMesh;
class FPImplRecastNavMesh;

/** 
 * 
 */
UCLASS()
class ENGINE_API URecastNavMeshDataChunk : public UNavigationDataChunk
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl);
	
	/** Detaches tiles from specified navmesh, taking tile ownership */
	TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl);
	
	/** Number of tiles in this chunk */
	int32 GetNumTiles() const;

	/** Releases all tiles that this chunk holds */
	void ReleaseTiles();

public:
#if WITH_EDITOR
	void GatherTiles(const FPImplRecastNavMesh* NavMeshImpl, const TArray<int32>& TileIndices);
#endif// WITH_EDITOR

private:
#if WITH_RECAST
	void SerializeRecastData(FArchive& Ar, int32 NavMeshVersion);
#endif//WITH_RECAST

private:
	TArray<FRecastTileData> Tiles;
};
