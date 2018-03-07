// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMeshSorting.h: Static sorting for skeletal mesh triangles
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

struct FSoftSkinVertex;

/**
* Group triangles into connected strips 
*
* @param NumTriangles - The number of triangles to group
* @param Indices - pointer to the index buffer data
* @outparam OutTriSet - an array containing the set number for each triangle.
* @return the maximum set number of any triangle.
*/
ENGINE_API int32 GetConnectedTriangleSets( int32 NumTriangles, const uint32* Indices, TArray<uint32>& OutTriSet );

/**
* Reorder the indices within a connected strip for cache efficiency
*
* @parm Indices - pointer to the index data
* @parm NumIndices - number of indices to optimize
*/
ENGINE_API void CacheOptimizeSortStrip(uint32* Indices, int32 NumIndices);

/**
* Remove all sorting and reset the index buffer order to that which is optimal
* in terms of cache efficiency.
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_None( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices );


/**
* Sort connected strips by distance from the center of each strip to the specified center position.
*
* @param SortCenter - Center point from which to sort
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_CenterRadialDistance( FVector SortCenter, int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices );

/**
* Sort connected strips by distance from the center of each strip to the center of all vertices.
*
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_CenterRadialDistance( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices );

/**
* Sort triangles randomly (for testing)
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_Random( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices );

/**
* Sort triangles by merging contiguous strips but otherwise retaining draw order
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_MergeContiguous( int32 NumTriangles, int32 NumVertices, const FSoftSkinVertex* Vertices, uint32* Indices );
