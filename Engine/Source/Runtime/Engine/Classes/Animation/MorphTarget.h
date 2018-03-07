// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "PackedNormal.h"
#include "MorphTarget.generated.h"

class FStaticLODModel;
class USkeletalMesh;
class UStaticMesh;

/**
* Morph mesh vertex data used for comparisons and importing
*/
struct FMorphMeshVertexRaw
{
	FVector Position;
	FVector TanX, TanY, TanZ;
};


/**
* Converts a mesh to raw vertex data used to generate a morph target mesh
*/
class FMorphMeshRawSource
{
public:
	/** vertex data used for comparisons */
	TArray<FMorphMeshVertexRaw> Vertices;

	/** index buffer used for comparison */
	TArray<uint32> Indices;

	/** indices to original imported wedge points */
	TArray<uint32> WedgePointIndices;

	/** Constructor (default) */
	ENGINE_API FMorphMeshRawSource() { }
	ENGINE_API FMorphMeshRawSource( USkeletalMesh* SrcMesh, int32 LODIndex = 0 );
	ENGINE_API FMorphMeshRawSource( UStaticMesh* SrcMesh, int32 LODIndex = 0 );
	ENGINE_API FMorphMeshRawSource( FStaticLODModel& LODModel );

	ENGINE_API bool IsValidTarget( const FMorphMeshRawSource& Target ) const;

private:
	void Initialize(FStaticLODModel& LODModel);
};


/** Morph mesh vertex data used for rendering */
struct FMorphTargetDelta
{
	/** change in position */
	FVector			PositionDelta;

	/** Tangent basis normal */
	FVector			TangentZDelta;

	/** index of source vertex to apply deltas to */
	uint32			SourceIdx;

	/** pipe operator */
	friend FArchive& operator<<(FArchive& Ar, FMorphTargetDelta& V)
	{
		if (Ar.UE4Ver() < VER_UE4_MORPHTARGET_CPU_TANGENTZDELTA_FORMATCHANGE)
		{
			/** old format of change in tangent basis normal */
			FPackedNormal	TangentZDelta_DEPRECATED;

			if (Ar.IsSaving())
			{
				TangentZDelta_DEPRECATED = FPackedNormal(V.TangentZDelta);
			}

			Ar << V.PositionDelta << TangentZDelta_DEPRECATED << V.SourceIdx;

			if (Ar.IsLoading())
			{
				V.TangentZDelta = TangentZDelta_DEPRECATED;
			}
		}
		else
		{
			Ar << V.PositionDelta << V.TangentZDelta << V.SourceIdx;
		}
		return Ar;
	}
};


/**
* Mesh data for a single LOD model of a morph target
*/
struct FMorphTargetLODModel
{
	/** vertex data for a single LOD morph mesh */
	TArray<FMorphTargetDelta> Vertices;

	/** number of original verts in the base mesh */
	int32 NumBaseMeshVerts;
	
	/** Get Resource Size */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const;
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;

	FMorphTargetLODModel()
		: NumBaseMeshVerts(0)
	{ }

	/** pipe operator */
	friend FArchive& operator<<(FArchive& Ar, FMorphTargetLODModel& M)
	{
		Ar << M.Vertices << M.NumBaseMeshVerts;
		return Ar;
	}
};


UCLASS(hidecategories=Object, MinimalAPI)
class UMorphTarget
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** USkeletalMesh that this vertex animation works on. */
	UPROPERTY(AssetRegistrySearchable)
	class USkeletalMesh* BaseSkelMesh;

	/** morph mesh vertex data for each LOD */
	TArray<FMorphTargetLODModel>	MorphLODModels;

public:
	/** Remap vertex indices with base mesh. */
	void RemapVertexIndices( USkeletalMesh* InBaseMesh, const TArray< TArray<uint32> > & BasedWedgePointIndices );

	/** Get Morphtarget Delta array for the given input Index */
	FMorphTargetDelta* GetMorphTargetDelta(int32 LODIndex, int32& OutNumDeltas);
	ENGINE_API bool HasDataForLOD(int32 LODIndex);
	/** return true if this morphtarget contains valid vertices */
	ENGINE_API bool HasValidData() const;

	/** Populates the given morph target LOD model with the provided deltas */
	ENGINE_API void PopulateDeltas(const TArray<FMorphTargetDelta>& Deltas, const int32 LODIndex, const bool bCompareNormal = false);

public:

	//~ UObject interface

	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

private:

	/**
	* Generate the streams for this morph target mesh using
	* a base mesh and a target mesh to find the positon differences
	* and other vertex attributes.
	*
	* @param BaseSource Source mesh for comparing position differences
	* @param TargetSource Final target vertex positions/attributes 
	* @param LODIndex Level of detail to use for the geometry
	*/
	void CreateMorphMeshStreams( const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex, bool bCompareNormal );
};
