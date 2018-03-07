// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayTypes.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY(LogHMD);
DEFINE_LOG_CATEGORY(LogLoadingSplash);

FHMDViewMesh::FHMDViewMesh() :
	pVertices(nullptr),
	pIndices(nullptr),
	NumVertices(0),
	NumIndices(0),
	NumTriangles(0)
{}

FHMDViewMesh::~FHMDViewMesh()
{
	if (pVertices)
	{
		delete[] pVertices;
	}

	if (pIndices)
	{
		delete[] pIndices;
	}
}

void FHMDViewMesh::BuildMesh(const FVector2D Positions[], uint32 VertexCount, EHMDMeshType MeshType)
{
	if( pVertices != nullptr )
	{
		delete[] pVertices;
		pVertices = nullptr;
	}
	if( pIndices != nullptr )
	{
		delete[] pIndices;
		pIndices = nullptr;
	}

	check(VertexCount > 2 && VertexCount % 3 == 0);

	NumVertices = VertexCount;
	NumTriangles = NumVertices / 3;
	NumIndices = NumVertices;

	pVertices = new FFilterVertex[NumVertices];
	pIndices = new uint16[NumIndices];

	uint32 DataIndex = 0;
	for (uint32 TriangleIter = 0; TriangleIter < NumTriangles; ++TriangleIter)
	{
		for (uint32 VertexIter = 0; VertexIter < 3; ++VertexIter)
		{
			const FVector2D& Position = Positions[DataIndex];
			FFilterVertex& Vertex = pVertices[DataIndex];

			if (MeshType == MT_HiddenArea)
			{
				// Remap from to NDC space [0 1] -> [-1 1]
				Vertex.Position.X = (Position.X * 2.0f) - 1.0f;
				Vertex.Position.Y = (Position.Y * 2.0f) - 1.0f;
				Vertex.Position.Z = 1.0f;
				Vertex.Position.W = 1.0f;

				// Not used for hidden area
				Vertex.UV.X = 0.0f;
				Vertex.UV.Y = 0.0f;
			}
			else
			{
				// Remap the viewport origin from the bottom left to the top left
				Vertex.Position.X = Position.X;
				Vertex.Position.Y = 1.0f - Position.Y;
				Vertex.Position.Z = 0.0f;
				Vertex.Position.W = 1.0f;

				Vertex.UV.X = Position.X;
				Vertex.UV.Y = 1.0f - Position.Y;
			}

			pIndices[DataIndex] = DataIndex;

			++DataIndex;
		}
	}
}
