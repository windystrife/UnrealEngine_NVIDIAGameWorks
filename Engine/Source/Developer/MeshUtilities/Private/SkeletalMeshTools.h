// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "PackedNormal.h"
#include "GPUSkinPublicDefs.h"
#include "SkeletalMeshTypes.h"

class USkeletalMesh;

// this is used for a sub-quadratic routine to find "equal" verts
struct FSkeletalMeshVertIndexAndZ
{
	int32 Index;
	float Z;
};

struct FSoftSkinBuildVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentY,	// Binormal, V-direction
					TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// VertexColor
	FBoneIndexType	InfluenceBones[MAX_TOTAL_INFLUENCES];
	uint8			InfluenceWeights[MAX_TOTAL_INFLUENCES];
	uint32 PointWedgeIdx;
};

/**
 * A chunk of skinned mesh vertices used as intermediate data to build a renderable
 * skinned mesh.
 */
struct FSkinnedMeshChunk
{
	/** The material index with which this chunk should be rendered. */
	int32 MaterialIndex;
	/** The original section index for which this chunk was generated. */
	int32 OriginalSectionIndex;
	/** The vertices associated with this chunk. */
	TArray<FSoftSkinBuildVertex> Vertices;
	/** The indices of the triangles in this chunk. */
	TArray<uint32> Indices;
	/** If not empty, contains a map from bones referenced in this chunk to the skeleton. */
	TArray<FBoneIndexType> BoneMap;
};

/**
 * Skinned model data needed to generate skinned mesh chunks for reprocessing.
 */
struct FSkinnedModelData
{
	/** Vertices of the model. */
	TArray<FSoftSkinVertex> Vertices;
	/** Indices of the model. */
	TArray<uint32> Indices;
	/** Contents of the model's RawPointIndices bulk data. */
	TArray<uint32> RawPointIndices;
	/** Map of vertex index to the original import index. */
	TArray<int32> MeshToImportVertexMap;
	/** Per-section information. */
	TArray<FSkelMeshSection> Sections;
	/** Per-section bone maps. */
	TIndirectArray<TArray<FBoneIndexType> > BoneMaps;
	/** The number of valid texture coordinates. */
	int32 NumTexCoords;
};

namespace SkeletalMeshTools
{
	inline bool SkeletalMesh_UVsEqual(const FMeshWedge& V1, const FMeshWedge& V2, const int32 UVIndex = 0)
	{
		const FVector2D& UV1 = V1.UVs[UVIndex];
		const FVector2D& UV2 = V2.UVs[UVIndex];

		if(FMath::Abs(UV1.X - UV2.X) > (1.0f / 1024.0f))
			return 0;

		if(FMath::Abs(UV1.Y - UV2.Y) > (1.0f / 1024.0f))
			return 0;

		return 1;
	}

	/** @return true if V1 and V2 are equal */
	bool AreSkelMeshVerticesEqual( const FSoftSkinBuildVertex& V1, const FSoftSkinBuildVertex& V2 );

	/**
	 * Creates chunks and populates the vertex and index arrays inside each chunk
	 *
	 * @param Faces						List of raw faces
	 * @param RawVertices				List of raw created, unordered, unwelded vertices
	 * @param RawVertIndexAndZ			List of indices into the RawVertices array and each raw vertex Z position.  This is used for fast lookup of overlapping vertices
	 * @param bKeepOverlappingVertices	Whether or not to do the overlapping vertices or not.  If true each vertex is considered unique
	 * @param OutChunks					Created array of chunks
	 */
	void BuildSkeletalMeshChunks( const TArray<FMeshFace>& Faces, const TArray<FSoftSkinBuildVertex>& RawVertices, TArray<FSkeletalMeshVertIndexAndZ>& RawVertIndexAndZ, bool bKeepOverlappingVertices, TArray<FSkinnedMeshChunk*>& OutChunks, bool& bOutTooManyVerts );

	/**
	 * Adds a new vertex to the list of vertices.  Note this method is very slow with a lot of vertices as this will amount to n^2 checks that the passed in vertex is not the same as another vertex in the vertices list
	 */
	int32 AddSkinVertex(TArray<FSoftSkinBuildVertex>& Vertices,FSoftSkinBuildVertex& Vertex, bool bKeepOverlappingVertices );

	/**
	 * Splits chunks to satisfy the requested maximum number of bones per chunk
	 * @param Chunks			Chunks to split. Upon return contains the results of splitting chunks.
	 * @param MaxBonesPerChunk	The maximum number of bones a chunk may reference.
	*/
	void ChunkSkinnedVertices(TArray<FSkinnedMeshChunk*>& Chunks,int32 MaxBonesPerChunk);

	/**
	 * Copies data out of Model so that the data can be processed in the background.
	 */
	void CopySkinnedModelData(FSkinnedModelData& OutData, FStaticLODModel& Model);
	
	void UnchunkSkeletalModel(TArray<FSkinnedMeshChunk*>& Chunks, TArray<int32>& PointToOriginalMap, const FSkinnedModelData& SrcModel);

	void CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant);
};
