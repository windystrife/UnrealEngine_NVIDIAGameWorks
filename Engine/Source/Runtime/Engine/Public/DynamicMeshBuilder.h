// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicMeshBuilder.h: Dynamic mesh builder definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PackedNormal.h"
#include "HitProxies.h"
#include "RenderUtils.h"

class FMaterialRenderProxy;
class FMeshElementCollector;
class FPrimitiveDrawInterface;

/** The vertex type used for dynamic meshes. */
struct FDynamicMeshVertex
{
	FDynamicMeshVertex() {}
	FDynamicMeshVertex( const FVector& InPosition ):
		Position(InPosition),
		TextureCoordinate(FVector2D::ZeroVector),
		TangentX(FVector(1,0,0)),
		TangentZ(FVector(0,0,1)),
		Color(FColor(255,255,255)) 
	{
		// basis determinant default to +1.0
		TangentZ.Vector.W = 255;
	}

	FDynamicMeshVertex(const FVector& InPosition,const FVector& InTangentX,const FVector& InTangentZ,const FVector2D& InTexCoord, const FColor& InColor):
		Position(InPosition),
		TextureCoordinate(InTexCoord),
		TangentX(InTangentX),
		TangentZ(InTangentZ),
		Color(InColor)
	{
		// basis determinant default to +1.0
		TangentZ.Vector.W = 255;
	}

	void SetTangents( const FVector& InTangentX, const FVector& InTangentY, const FVector& InTangentZ )
	{
		TangentX = InTangentX;
		TangentZ = InTangentZ;
		// store determinant of basis in w component of normal vector
		TangentZ.Vector.W = GetBasisDeterminantSign(InTangentX,InTangentY,InTangentZ) < 0.0f ? 0 : 255;
	}

	FVector GetTangentY()
	{
		FVector TanX = TangentX;
		FVector TanZ = TangentZ;

		return (TanZ ^ TanX) * ((float)TangentZ.Vector.W / 127.5f - 1.0f);
	};

	FVector Position;
	FVector2D TextureCoordinate;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
};

/**
 * A utility used to construct dynamically generated meshes, and render them to a FPrimitiveDrawInterface.
 * Note: This is meant to be easy to use, not fast.  It moves the data around more than necessary, and requires dynamically allocating RHI
 * resources.  Exercise caution.
 */
class FDynamicMeshBuilder
{
public:

	/** Initialization constructor. */
	ENGINE_API FDynamicMeshBuilder();

	/** Destructor. */
	ENGINE_API ~FDynamicMeshBuilder();

	/** Adds a vertex to the mesh. */
	ENGINE_API int32 AddVertex(
		const FVector& InPosition,
		const FVector2D& InTextureCoordinate,
		const FVector& InTangentX,
		const FVector& InTangentY,
		const FVector& InTangentZ,
		const FColor& InColor
		);

	/** Adds a vertex to the mesh. */
	ENGINE_API int32 AddVertex(const FDynamicMeshVertex &InVertex);

	/** Adds a triangle to the mesh. */
	ENGINE_API void AddTriangle(int32 V0,int32 V1,int32 V2);

	/** Adds many vertices to the mesh. */
	ENGINE_API int32 AddVertices(const TArray<FDynamicMeshVertex> &InVertices);

	/** Add many indices to the mesh. */
	ENGINE_API void AddTriangles(const TArray<int32> &InIndices);

	/** Adds a mesh of what's been built so far to the collector. */
	ENGINE_API void GetMesh(const FMatrix& LocalToWorld, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup, bool bDisableBackfaceCulling, bool bReceivesDecals, int32 ViewIndex, FMeshElementCollector& Collector);
	ENGINE_API void GetMesh(const FMatrix& LocalToWorld, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup, bool bDisableBackfaceCulling, bool bReceivesDecals, bool bUseSelectionOutline, int32 ViewIndex, 
							FMeshElementCollector& Collector, HHitProxy* HitProxy);
	ENGINE_API void GetMesh(const FMatrix& LocalToWorld, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup, bool bDisableBackfaceCulling, bool bReceivesDecals, bool bUseSelectionOutline, int32 ViewIndex, 
							FMeshElementCollector& Collector, const FHitProxyId HitProxyId = FHitProxyId());

	/**
	 * Draws the mesh to the given primitive draw interface.
	 * @param PDI - The primitive draw interface to draw the mesh on.
	 * @param LocalToWorld - The local to world transform to apply to the vertices of the mesh.
	 * @param FMaterialRenderProxy - The material instance to render on the mesh.
	 * @param DepthPriorityGroup - The depth priority group to render the mesh in.
	 * @param HitProxyId - Hit proxy to use for this mesh.  Use INDEX_NONE for no hit proxy.
	 */
	ENGINE_API void Draw(FPrimitiveDrawInterface* PDI,const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriorityGroup,bool bDisableBackfaceCulling=false, bool bReceivesDecals=true, const FHitProxyId HitProxyId = FHitProxyId());

private:
	class FDynamicMeshIndexBuffer* IndexBuffer;
	class FDynamicMeshVertexBuffer* VertexBuffer;
};
