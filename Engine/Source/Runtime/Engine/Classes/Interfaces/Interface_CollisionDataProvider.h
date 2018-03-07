// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Interface for objects that have a PhysX collision representation and need their geometry cooked
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Interface_CollisionDataProvider.generated.h"

// Vertex indices necessary to describe the vertices listed in TriMeshCollisionData
struct FTriIndices
{
	int32 v0;
	int32 v1;
	int32 v2;

	FTriIndices()
		: v0(0)
		, v1(0)
		, v2(0)
	{
	}
};


// Description of triangle mesh collision data necessary for cooking physics data
struct FTriMeshCollisionData
{
	/** Array of vertices included in the triangle mesh */
	TArray<FVector> Vertices;

	/** Array of indices defining the ordering of triangles in the mesh */
	TArray<FTriIndices> Indices;

	/** Array of optional material indices (must equal num triangles) */
	TArray<uint16>	MaterialIndices;

	/** Optional UV co-ordinates (each array must be zero of equal num vertices) */
	TArray< TArray<FVector2D> > UVs;

	/** Does the mesh require its normals flipped (see PxMeshFlag) */
	uint32 bFlipNormals:1;

	/** If mesh is deformable, we don't clean it, so that vertex layout does not change and it can be updated */
	uint32 bDeformableMesh:1;

	/** Prioritize cooking speed over runtime speed */
	uint32 bFastCook : 1;

	FTriMeshCollisionData()
	: bFlipNormals(false)
	, bDeformableMesh(false)
	, bFastCook(false)
	{
	}
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class UInterface_CollisionDataProvider : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IInterface_CollisionDataProvider
{
	GENERATED_IINTERFACE_BODY()


	/**	 Interface for retrieving triangle mesh collision data from the implementing object 
	 *
	 * @param CollisionData - structure given by the caller to be filled with tri mesh collision data
	 * @return true if successful, false if unable to successfully fill in data structure
	 */
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) { return false; }

	/**	 Interface for checking if the implementing objects contains triangle mesh collision data 
	 *
	 * @return true if the implementing object contains triangle mesh data, false otherwise
	 */
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const { return false; }

	/** Do we want to create a negative version of this mesh */
	virtual bool WantsNegXTriMesh() { return false; }

	/** An optional string identifying the mesh data. */
	virtual void GetMeshId(FString& OutMeshId) {}
};

