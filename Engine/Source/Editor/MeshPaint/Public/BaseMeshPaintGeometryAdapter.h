// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMeshPaintGeometryAdapter.h"
#include "TMeshPaintOctree.h"

/** Base mesh paint geometry adapter, handles basic sphere intersection using a Octree */
class MESHPAINT_API FBaseMeshPaintGeometryAdapter : public IMeshPaintGeometryAdapter
{
public:
	/** Start IMeshPaintGeometryAdapter Overrides */
	virtual bool Initialize() override;
	virtual const TArray<FVector>& GetMeshVertices() const override;
	virtual const TArray<uint32>& GetMeshIndices() const override;
	virtual void GetVertexPosition(int32 VertexIndex, FVector& OutVertex) const override;
	virtual TArray<uint32> SphereIntersectTriangles(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing) const override;
	virtual void GetInfluencedVertexIndices(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing, TSet<int32> &InfluencedVertices) const override;
	virtual void GetInfluencedVertexData(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing, TArray<TPair<int32, FVector>>& OutData) const override;
	virtual TArray<FVector> SphereIntersectVertices(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing) const override;
	/** End IMeshPaintGeometryAdapter Overrides */

	virtual bool InitializeVertexData() = 0;
protected:
	bool BuildOctree();
protected:
	/** Index and Vertex data populated by derived classes in InitializeVertexData */
	TArray<FVector> MeshVertices;
	TArray<uint32> MeshIndices;
	/** Octree used for reducing the cost of sphere intersecting with triangles / vertices */
	TUniquePtr<FMeshPaintTriangleOctree> MeshTriOctree;
};
