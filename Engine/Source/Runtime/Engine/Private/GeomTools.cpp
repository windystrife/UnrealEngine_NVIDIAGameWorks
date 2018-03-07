// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 =============================================================================*/

#include "GeomTools.h"
#include "EngineDefines.h"
#include "RawIndexBuffer.h"
#include "StaticMeshResources.h"
#include "Engine/Polys.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogGeomTools, Log, All);

/** Util struct for storing a set of planes defining a convex region, with neighbour information. */
struct FHullPlanes
{
	TArray<FPlane>	Planes;
	TArray<int32>		PlaneNeighbour;
	TArray<float>	PlaneNeighbourArea;

	/** 
	 *	Util for removing redundany planes from the hull. 
	 *	Also calculates area of connection
	 */
	void RemoveRedundantPlanes()
	{
		check(Planes.Num() == PlaneNeighbour.Num());

		// For each plane
		for(int32 i=Planes.Num()-1; i>=0; i--)
		{
			// Build big-ass polygon
			FPoly Polygon;
			Polygon.Normal = Planes[i];

			FVector AxisX, AxisY;
			Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

			const FVector Base = Planes[i] * Planes[i].W;

			new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);

			// Clip away each plane
			for(int32 j=0; j<Planes.Num(); j++)
			{
				if(i != j)
				{
					if(!Polygon.Split(-FVector(Planes[j]), Planes[j] * Planes[j].W))
					{
						// At this point we clipped away all the polygon - so this plane is redundant - remove!
						Polygon.Vertices.Empty();

						Planes.RemoveAt(i);
						PlaneNeighbour.RemoveAt(i);

						break;
					}
				}
			}

			// If we still have a polygon left - this is a neighbour - calculate area of connection.
			if(Polygon.Vertices.Num() > 0)
			{
				// Insert at front - we are walking planes from end to start.
				PlaneNeighbourArea.Insert(Polygon.Area(), 0);
			}
		}

		check(PlaneNeighbourArea.Num() == PlaneNeighbour.Num());
		check(Planes.Num() == PlaneNeighbour.Num());
	}
};
	
/** */
static FClipSMVertex GetVert(const UStaticMesh* StaticMesh, int32 VertIndex)
{
	FClipSMVertex Result;
	FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
	Result.Pos = LODModel.PositionVertexBuffer.VertexPosition(VertIndex);
	Result.TangentX = LODModel.VertexBuffer.VertexTangentX(VertIndex);
	Result.TangentY = LODModel.VertexBuffer.VertexTangentY(VertIndex);
	Result.TangentZ = LODModel.VertexBuffer.VertexTangentZ(VertIndex);
	const int32 NumUVs = LODModel.VertexBuffer.GetNumTexCoords();
	for(int32 UVIndex = 0;UVIndex < NumUVs;UVIndex++)
	{
		Result.UVs[UVIndex] = LODModel.VertexBuffer.GetVertexUV(VertIndex,UVIndex);
	}
	for(int32 UVIndex = NumUVs;UVIndex < ARRAY_COUNT(Result.UVs);UVIndex++)
	{
		Result.UVs[UVIndex] = FVector2D::ZeroVector;
	}
	Result.Color = FColor( 255, 255, 255 );
	if( LODModel.ColorVertexBuffer.GetNumVertices() > 0 )
	{
		Result.Color = LODModel.ColorVertexBuffer.VertexColor(VertIndex);
	}
	return Result;
}

/** Take two static mesh verts and interpolate all values between them */
FClipSMVertex InterpolateVert(const FClipSMVertex& V0, const FClipSMVertex& V1, float Alpha)
{
	FClipSMVertex Result;

	// Handle dodgy alpha
	if(FMath::IsNaN(Alpha) || !FMath::IsFinite(Alpha))
	{
		Result = V1;
		return Result;
	}

	Result.Pos = FMath::Lerp(V0.Pos, V1.Pos, Alpha);
	Result.TangentX = FMath::Lerp(V0.TangentX,V1.TangentX,Alpha);
	Result.TangentY = FMath::Lerp(V0.TangentY,V1.TangentY,Alpha);
	Result.TangentZ = FMath::Lerp(V0.TangentZ,V1.TangentZ,Alpha);
	for(int32 i=0; i<8; i++)
	{
		Result.UVs[i] = FMath::Lerp(V0.UVs[i], V1.UVs[i], Alpha);
	}
	
	Result.Color.R = FMath::Clamp( FMath::TruncToInt(FMath::Lerp(float(V0.Color.R), float(V1.Color.R), Alpha)), 0, 255 );
	Result.Color.G = FMath::Clamp( FMath::TruncToInt(FMath::Lerp(float(V0.Color.G), float(V1.Color.G), Alpha)), 0, 255 );
	Result.Color.B = FMath::Clamp( FMath::TruncToInt(FMath::Lerp(float(V0.Color.B), float(V1.Color.B), Alpha)), 0, 255 );
	Result.Color.A = FMath::Clamp( FMath::TruncToInt(FMath::Lerp(float(V0.Color.A), float(V1.Color.A), Alpha)), 0, 255 );
	return Result;
}

/** Extracts the triangles from a static-mesh as clippable triangles. */
void FGeomTools::GetClippableStaticMeshTriangles(TArray<FClipSMTriangle>& OutClippableTriangles,const UStaticMesh* StaticMesh)
{
	const FStaticMeshLODResources& RenderData = StaticMesh->RenderData->LODResources[0];
	FIndexArrayView Indices = RenderData.IndexBuffer.GetArrayView();
	for(int32 SectionIndex = 0;SectionIndex < RenderData.Sections.Num();SectionIndex++)
	{
		const FStaticMeshSection& Section = RenderData.Sections[SectionIndex];
		for(uint32 TriangleIndex = 0;TriangleIndex < Section.NumTriangles;TriangleIndex++)
		{
			FClipSMTriangle ClipTriangle(0);

			// Copy the triangle's attributes.
			ClipTriangle.MaterialIndex = Section.MaterialIndex;
			ClipTriangle.NumUVs = RenderData.VertexBuffer.GetNumTexCoords();
			ClipTriangle.SmoothingMask = 0;
			ClipTriangle.bOverrideTangentBasis = true;

			// Extract the vertices for this triangle.
			const uint32 BaseIndex = Section.FirstIndex + TriangleIndex * 3;
			for(uint32 TriangleVertexIndex = 0;TriangleVertexIndex < 3;TriangleVertexIndex++)
			{
				const uint32 VertexIndex = Indices[BaseIndex + TriangleVertexIndex];
				ClipTriangle.Vertices[TriangleVertexIndex] = GetVert(StaticMesh,VertexIndex);
			}

			// Compute the triangle's gradients and normal.
			ClipTriangle.ComputeGradientsAndNormal();

			// Add the triangle to the output array.
			OutClippableTriangles.Add(ClipTriangle);
		}
	}
}

/** Take the input mesh and cut it with supplied plane, creating new verts etc. Also outputs new edges created on the plane. */
void FGeomTools::ClipMeshWithPlane( TArray<FClipSMTriangle>& OutTris, TArray<FUtilEdge3D>& OutClipEdges, const TArray<FClipSMTriangle>& InTris, const FPlane& Plane )
{
	// Iterate over each source triangle
	for(int32 TriIdx=0; TriIdx<InTris.Num(); TriIdx++)
	{
		const FClipSMTriangle* SrcTri = &InTris[TriIdx];

		// Calculate which verts are beyond clipping plane
		float PlaneDist[3];
		for(int32 i=0; i<3; i++)
		{
			PlaneDist[i] = Plane.PlaneDot(SrcTri->Vertices[i].Pos);
		}

		TArray<FClipSMVertex> FinalVerts;
		FUtilEdge3D NewClipEdge;
		int32 ClippedEdges = 0;

		for(int32 EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++)
		{
			int32 ThisVert = EdgeIdx;

			// If start vert is inside, add it.
			if(FMath::IsNegativeFloat(PlaneDist[ThisVert]))
			{
				FinalVerts.Add( SrcTri->Vertices[ThisVert] );
			}

			// If start and next vert are on opposite sides, add intersection
			int32 NextVert = (EdgeIdx+1)%3;

			if(FMath::IsNegativeFloat(PlaneDist[EdgeIdx]) != FMath::IsNegativeFloat(PlaneDist[NextVert]))
			{
				// Find distance along edge that plane is
				float Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);
				// Interpolate vertex params to that point
				FClipSMVertex InterpVert = InterpolateVert(SrcTri->Vertices[ThisVert], SrcTri->Vertices[NextVert], FMath::Clamp(Alpha,0.0f,1.0f));
				// Save vert
				FinalVerts.Add(InterpVert);

				// When we make a new edge on the surface of the clip plane, save it off.
				if(ClippedEdges == 0)
				{
					NewClipEdge.V0 = InterpVert.Pos;
				}
				else
				{
					NewClipEdge.V1 = InterpVert.Pos;
				}
				ClippedEdges++;
			}
		}

		// Triangulate the clipped polygon.
		for(int32 VertexIndex = 2;VertexIndex < FinalVerts.Num();VertexIndex++)
		{
			FClipSMTriangle NewTri = *SrcTri;
			NewTri.Vertices[0] = FinalVerts[0];
			NewTri.Vertices[1] = FinalVerts[VertexIndex - 1];
			NewTri.Vertices[2] = FinalVerts[VertexIndex];
			NewTri.bOverrideTangentBasis = true;
			OutTris.Add(NewTri);
		}

		// If we created a new edge, save that off here as well
		if(ClippedEdges == 2)
		{
			OutClipEdges.Add(NewClipEdge);
		}
	}
}

/** Take a set of 3D Edges and project them onto the supplied plane. Also returns matrix use to convert them back into 3D edges. */
void FGeomTools::ProjectEdges( TArray<FUtilEdge2D>& Out2DEdges, FMatrix& ToWorld, const TArray<FUtilEdge3D>& In3DEdges, const FPlane& InPlane )
{
	// Build matrix to transform verts into plane space
	FVector BasisX, BasisY, BasisZ;
	BasisZ = InPlane;
	BasisZ.FindBestAxisVectors(BasisX, BasisY);
	ToWorld = FMatrix( BasisX, BasisY, InPlane, BasisZ * InPlane.W );

	Out2DEdges.AddUninitialized( In3DEdges.Num() );
	for(int32 i=0; i<In3DEdges.Num(); i++)
	{
		FVector P = ToWorld.InverseTransformPosition(In3DEdges[i].V0);
		Out2DEdges[i].V0.X = P.X;
		Out2DEdges[i].V0.Y = P.Y;

		P = ToWorld.InverseTransformPosition(In3DEdges[i].V1);
		Out2DEdges[i].V1.X = P.X;
		Out2DEdges[i].V1.Y = P.Y;
	}
}

/** End of one edge and start of next must be less than this to connect them. */
static float EdgeMatchTolerance = 0.01f;

/** Util to look for best next edge start from Start. Returns false if no good next edge found. Edge is removed from InEdgeSet when found. */
static bool FindNextEdge(FUtilEdge2D& OutNextEdge, const FVector2D& Start, TArray<FUtilEdge2D>& InEdgeSet)
{
	float ClosestDistSqr = BIG_NUMBER;
	FUtilEdge2D OutEdge;
	int32 OutEdgeIndex = INDEX_NONE;
	// Search set of edges for one that starts closest to Start
	for(int32 i=0; i<InEdgeSet.Num(); i++)
	{
		float DistSqr = (InEdgeSet[i].V0 - Start).SizeSquared();
		if(DistSqr < ClosestDistSqr)
		{
			ClosestDistSqr = DistSqr;
			OutNextEdge = InEdgeSet[i];
			OutEdgeIndex = i;
		}

		DistSqr = (InEdgeSet[i].V1 - Start).SizeSquared();
		if(DistSqr < ClosestDistSqr)
		{
			ClosestDistSqr = DistSqr;
			OutNextEdge = InEdgeSet[i];
			Swap(OutNextEdge.V0, OutNextEdge.V1);
			OutEdgeIndex = i;
		}
	}

	// If next edge starts close enough return it
	if(ClosestDistSqr < FMath::Square(EdgeMatchTolerance))
	{
		check(OutEdgeIndex != INDEX_NONE);
		InEdgeSet.RemoveAt(OutEdgeIndex);
		return true;
	}

	// No next edge found.
	return false;
}

/** 
 *	Make sure that polygon winding is always consistent - cross product between successive edges is positive. 
 *	This function also remove co-linear edges.
 */
static void FixPolyWinding(FUtilPoly2D& Poly)
{
	float TotalAngle = 0.f;
	for(int32 i=Poly.Verts.Num()-1; i>=0; i--)
	{
		// Triangle is 'this' vert plus the one before and after it
		int32 AIndex = (i==0) ? Poly.Verts.Num()-1 : i-1;
		int32 BIndex = i;
		int32 CIndex = (i+1)%Poly.Verts.Num();

		float ABDistSqr = (Poly.Verts[BIndex].Pos - Poly.Verts[AIndex].Pos).SizeSquared();
		FVector2D ABEdge = (Poly.Verts[BIndex].Pos - Poly.Verts[AIndex].Pos).GetSafeNormal();

		float BCDistSqr = (Poly.Verts[CIndex].Pos - Poly.Verts[BIndex].Pos).SizeSquared();
		FVector2D BCEdge = (Poly.Verts[CIndex].Pos - Poly.Verts[BIndex].Pos).GetSafeNormal();

		// See if points are co-incident or edges are co-linear - if so, remove.
		if(ABDistSqr < 0.0001f || BCDistSqr < 0.0001f || ABEdge.Equals(BCEdge, 0.01f))
		{
			Poly.Verts.RemoveAt(i);
		}
		else
		{
			TotalAngle += FMath::Asin(ABEdge ^ BCEdge);
		}
	}

	// If total angle is negative, reverse order.
	if(TotalAngle < 0.f)
	{
		int32 NumVerts = Poly.Verts.Num();

		TArray<FUtilVertex2D> NewVerts;
		NewVerts.AddUninitialized(NumVerts);

		for(int32 i=0; i<NumVerts; i++)
		{
			NewVerts[i] = Poly.Verts[NumVerts-(1+i)];
		}
		Poly.Verts = NewVerts;
	}
}

/** Given a set of edges, find the set of closed polygons created by them. */
void FGeomTools::Buid2DPolysFromEdges(TArray<FUtilPoly2D>& OutPolys, const TArray<FUtilEdge2D>& InEdges, const FColor& VertColor)
{
	TArray<FUtilEdge2D> EdgeSet = InEdges;

	// While there are still edges to process..
	while(EdgeSet.Num() > 0)
	{
		// Initialise new polygon with the first edge in the set
		FUtilPoly2D NewPoly;
		FUtilEdge2D FirstEdge = EdgeSet.Pop();

		NewPoly.Verts.Add(FUtilVertex2D(FirstEdge.V0, VertColor));
		NewPoly.Verts.Add(FUtilVertex2D(FirstEdge.V1, VertColor));

		// Now we keep adding edges until we can't find any more
		FVector2D PolyEnd = NewPoly.Verts[ NewPoly.Verts.Num()-1 ].Pos;
		FUtilEdge2D NextEdge;
		while( FindNextEdge(NextEdge, PolyEnd, EdgeSet) )
		{
			NewPoly.Verts.Add(FUtilVertex2D(NextEdge.V1, VertColor));
			PolyEnd = NewPoly.Verts[ NewPoly.Verts.Num()-1 ].Pos;
		}

		// After walking edges see if we have a closed polygon.
		float CloseDistSqr = (NewPoly.Verts[0].Pos - NewPoly.Verts[ NewPoly.Verts.Num()-1 ].Pos).SizeSquared();
		if(NewPoly.Verts.Num() >= 4 && CloseDistSqr < FMath::Square(EdgeMatchTolerance))
		{
			// Remove last vert - its basically a duplicate of the first.
			NewPoly.Verts.RemoveAt( NewPoly.Verts.Num()-1 );

			// Make sure winding is correct.
			FixPolyWinding(NewPoly);

			// Add to set of output polys.
			OutPolys.Add(NewPoly);
		}
	}
}

/** Given three direction vectors, indicates if A and B are on the same 'side' of Vec. */
bool FGeomTools::VectorsOnSameSide(const FVector& Vec, const FVector& A, const FVector& B)
{
	const FVector CrossA = Vec ^ A;
	const FVector CrossB = Vec ^ B;
	return !FMath::IsNegativeFloat(CrossA | CrossB);
}

/** Util to see if P lies within triangle created by A, B and C. */
bool FGeomTools::PointInTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& P)
{
	// Cross product indicates which 'side' of the vector the point is on
	// If its on the same side as the remaining vert for all edges, then its inside.	
	if( VectorsOnSameSide(B-A, P-A, C-A) &&
		VectorsOnSameSide(C-B, P-B, A-B) &&
		VectorsOnSameSide(A-C, P-C, B-C) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/** Compare all aspects of two verts of two triangles to see if they are the same. */
static bool VertsAreEqual(const FClipSMVertex& A,const FClipSMVertex& B)
{
	if( !A.Pos.Equals(B.Pos, THRESH_POINTS_ARE_SAME) )
	{
		return false;
	}

	if( !A.TangentX.Equals(B.TangentX, THRESH_NORMALS_ARE_SAME) )
	{
		return false;
	}
	
	if( !A.TangentY.Equals(B.TangentY, THRESH_NORMALS_ARE_SAME) )
	{
		return false;
	}
	
	if( !A.TangentZ.Equals(B.TangentZ, THRESH_NORMALS_ARE_SAME) )
	{
		return false;
	}

	if( A.Color != B.Color )
	{
		return false;
	}

	for(int32 i=0; i<ARRAY_COUNT(A.UVs); i++)
	{
		if( !A.UVs[i].Equals(B.UVs[i], 1.0f / 1024.0f) )
		{
			return false;
		}
	}

	return true;
}

/** Determines whether two edges may be merged. */
static bool AreEdgesMergeable(
	const FClipSMVertex& V0,
	const FClipSMVertex& V1,
	const FClipSMVertex& V2
	)
{
	const FVector MergedEdgeVector = V2.Pos - V0.Pos;
	const float MergedEdgeLengthSquared = MergedEdgeVector.SizeSquared();
	if(MergedEdgeLengthSquared > DELTA)
	{
		// Find the vertex closest to A1/B0 that is on the hypothetical merged edge formed by A0-B1.
		const float IntermediateVertexEdgeFraction =
			((V2.Pos - V0.Pos) | (V1.Pos - V0.Pos)) / MergedEdgeLengthSquared;
		const FClipSMVertex InterpolatedVertex = InterpolateVert(V0,V2,IntermediateVertexEdgeFraction);

		// The edges are merge-able if the interpolated vertex is close enough to the intermediate vertex.
		return VertsAreEqual(InterpolatedVertex,V1);
	}
	else
	{
		return true;
	}
}

/** Given a polygon, decompose into triangles and append to OutTris. */
bool FGeomTools::TriangulatePoly(TArray<FClipSMTriangle>& OutTris, const FClipSMPolygon& InPoly, bool bKeepColinearVertices)
{
	// Can't work if not enough verts for 1 triangle
	if(InPoly.Vertices.Num() < 3)
	{
		// Return true because poly is already a tri
		return true;
	}

	// Vertices of polygon in order - make a copy we are going to modify.
	TArray<FClipSMVertex> PolyVerts = InPoly.Vertices;

	// Keep iterating while there are still vertices
	while(true)
	{
		if (!bKeepColinearVertices)
		{
			// Cull redundant vertex edges from the polygon.
			for (int32 VertexIndex = 0; VertexIndex < PolyVerts.Num(); VertexIndex++)
			{
				const int32 I0 = (VertexIndex + 0) % PolyVerts.Num();
				const int32 I1 = (VertexIndex + 1) % PolyVerts.Num();
				const int32 I2 = (VertexIndex + 2) % PolyVerts.Num();
				if (AreEdgesMergeable(PolyVerts[I0], PolyVerts[I1], PolyVerts[I2]))
				{
					PolyVerts.RemoveAt(I1);
					VertexIndex--;
				}
			}
		}

		if(PolyVerts.Num() < 3)
		{
			break;
		}
		else
		{
			// Look for an 'ear' triangle
			bool bFoundEar = false;
			for(int32 EarVertexIndex = 0;EarVertexIndex < PolyVerts.Num();EarVertexIndex++)
			{
				// Triangle is 'this' vert plus the one before and after it
				const int32 AIndex = (EarVertexIndex==0) ? PolyVerts.Num()-1 : EarVertexIndex-1;
				const int32 BIndex = EarVertexIndex;
				const int32 CIndex = (EarVertexIndex+1)%PolyVerts.Num();

				// Check that this vertex is convex (cross product must be positive)
				const FVector ABEdge = PolyVerts[BIndex].Pos - PolyVerts[AIndex].Pos;
				const FVector ACEdge = PolyVerts[CIndex].Pos - PolyVerts[AIndex].Pos;
				const float TriangleDeterminant = (ABEdge ^ ACEdge) | InPoly.FaceNormal;
				if(FMath::IsNegativeFloat(TriangleDeterminant))
				{
					continue;
				}

				bool bFoundVertInside = false;
				// Look through all verts before this in array to see if any are inside triangle
				for(int32 VertexIndex = 0;VertexIndex < PolyVerts.Num();VertexIndex++)
				{
					if(	VertexIndex != AIndex && VertexIndex != BIndex && VertexIndex != CIndex &&
						PointInTriangle(PolyVerts[AIndex].Pos, PolyVerts[BIndex].Pos, PolyVerts[CIndex].Pos, PolyVerts[VertexIndex].Pos) )
					{
						bFoundVertInside = true;
						break;
					}
				}

				// Triangle with no verts inside - its an 'ear'! 
				if(!bFoundVertInside)
				{
					// Add to output list..
					FClipSMTriangle NewTri(0);
					NewTri.CopyFace(InPoly);
					NewTri.Vertices[0] = PolyVerts[AIndex];
					NewTri.Vertices[1] = PolyVerts[BIndex];
					NewTri.Vertices[2] = PolyVerts[CIndex];
					OutTris.Add(NewTri);

					// And remove vertex from polygon
					PolyVerts.RemoveAt(EarVertexIndex);

					bFoundEar = true;
					break;
				}
			}

			// If we couldn't find an 'ear' it indicates something is bad with this polygon - discard triangles and return.
			if(!bFoundEar)
			{
				UE_LOG(LogGeomTools, Log, TEXT("Triangulation of poly failed."));
				OutTris.Empty();
				return false;
			}
		}
	}

	return true;
}


/** Transform triangle from 2D to 3D static-mesh triangle. */
FClipSMPolygon FGeomTools::Transform2DPolygonToSMPolygon(const FUtilPoly2D& InPoly, const FMatrix& InMatrix)
{
	FClipSMPolygon Result(0);

	for(int32 VertexIndex = 0;VertexIndex < InPoly.Verts.Num();VertexIndex++)
	{
		const FUtilVertex2D& InVertex = InPoly.Verts[VertexIndex];

		FClipSMVertex* OutVertex = new(Result.Vertices) FClipSMVertex;
		FMemory::Memzero(OutVertex,sizeof(*OutVertex));
		OutVertex->Pos = InMatrix.TransformPosition( FVector(InVertex.Pos.X, InVertex.Pos.Y, 0.f) );
		OutVertex->Color = InVertex.Color;
		OutVertex->UVs[0] = InVertex.UV;
	}

	// Assume that the matrix defines the polygon's normal.
	Result.FaceNormal = InMatrix.TransformVector(FVector(0,0,-1)).GetSafeNormal();

	return Result;
}

/** Does a simple box map onto this 2D polygon. */
void FGeomTools::GeneratePlanarFitPolyUVs(FUtilPoly2D& Polygon)
{
	// First work out 2D bounding box for tris.
	FVector2D Min(BIG_NUMBER, BIG_NUMBER);
	FVector2D Max(-BIG_NUMBER, -BIG_NUMBER);
	for(int32 VertexIndex = 0;VertexIndex < Polygon.Verts.Num();VertexIndex++)
	{
		const FUtilVertex2D& Vertex = Polygon.Verts[VertexIndex];
		Min.X = FMath::Min(Vertex.Pos.X, Min.X);
		Min.Y = FMath::Min(Vertex.Pos.Y, Min.Y);
		Max.X = FMath::Max(Vertex.Pos.X, Max.X);
		Max.Y = FMath::Max(Vertex.Pos.Y, Max.Y);
	}

	const FVector2D Extent = Max - Min;

	// Then use this to generate UVs
	for(int32 VertexIndex = 0;VertexIndex < Polygon.Verts.Num();VertexIndex++)
	{
		FUtilVertex2D& Vertex = Polygon.Verts[VertexIndex];
		Vertex.UV.X = (Vertex.Pos.X - Min.X)/Extent.X;
		Vertex.UV.Y = (Vertex.Pos.Y - Min.Y)/Extent.Y;
	}
}

void FGeomTools::GeneratePlanarTilingPolyUVs(FUtilPoly2D& Polygon, float TileSize)
{
	for (int32 VertexIndex = 0; VertexIndex < Polygon.Verts.Num(); VertexIndex++)
	{
		FUtilVertex2D& Vertex = Polygon.Verts[VertexIndex];
		Vertex.UV.X = Vertex.Pos.X / TileSize;
		Vertex.UV.Y = Vertex.Pos.Y / TileSize;
	}
}


/** Computes a transform from triangle parameter space into the space defined by an attribute that varies on the triangle's surface. */
static FMatrix ComputeTriangleParameterToAttribute(
	const FVector AttributeV0,
	const FVector AttributeV1,
	const FVector AttributeV2
	)
{
	const FVector AttributeOverS = AttributeV1 - AttributeV0;
	const FVector AttributeOverT = AttributeV2 - AttributeV0;
	const FVector AttributeOverNormal = (AttributeOverS ^ AttributeOverT).GetSafeNormal();
	return FMatrix(
		FPlane(	AttributeOverS.X,		AttributeOverS.Y,		AttributeOverS.Z,		0	),
		FPlane(	AttributeOverT.X,		AttributeOverT.Y,		AttributeOverT.Z,		0	),
		FPlane(	AttributeOverNormal.X,	AttributeOverNormal.Y,	AttributeOverNormal.Z,	0	),
		FPlane(	0,						0,						0,						1	)
		);
}

/** Converts a color into a vector. */
static FVector ColorToVector(const FLinearColor& Color)
{
	return FVector(Color.R,Color.G,Color.B);
}

void FClipSMTriangle::ComputeGradientsAndNormal()
{
	// Compute the transform from triangle parameter space to local space.
	const FMatrix ParameterToLocal = ComputeTriangleParameterToAttribute(Vertices[0].Pos,Vertices[1].Pos,Vertices[2].Pos);
	const FMatrix LocalToParameter = ParameterToLocal.Inverse();

	// Compute the triangle's normal.
	FaceNormal = ParameterToLocal.TransformVector(FVector(0,0,1));

	// Compute the normal's gradient in local space.
	const FMatrix ParameterToTangentX = ComputeTriangleParameterToAttribute(Vertices[0].TangentX,Vertices[1].TangentX,Vertices[2].TangentX);
	const FMatrix ParameterToTangentY = ComputeTriangleParameterToAttribute(Vertices[0].TangentY,Vertices[1].TangentY,Vertices[2].TangentY);
	const FMatrix ParameterToTangentZ = ComputeTriangleParameterToAttribute(Vertices[0].TangentZ,Vertices[1].TangentZ,Vertices[2].TangentZ);
	TangentXGradient = LocalToParameter * ParameterToTangentX;
	TangentYGradient = LocalToParameter * ParameterToTangentY;
	TangentZGradient = LocalToParameter * ParameterToTangentZ;

	// Compute the color's gradient in local space.
	const FVector Color0 = ColorToVector(Vertices[0].Color);
	const FVector Color1 = ColorToVector(Vertices[1].Color);
	const FVector Color2 = ColorToVector(Vertices[2].Color);
	const FMatrix ParameterToColor = ComputeTriangleParameterToAttribute(Color0,Color1,Color2);
	ColorGradient = LocalToParameter * ParameterToColor;

	for(int32 UVIndex = 0;UVIndex < NumUVs;UVIndex++)
	{
		// Compute the UV's gradient in local space.
		const FVector UV0(Vertices[0].UVs[UVIndex],0);
		const FVector UV1(Vertices[1].UVs[UVIndex],0);
		const FVector UV2(Vertices[2].UVs[UVIndex],0);
		const FMatrix ParameterToUV = ComputeTriangleParameterToAttribute(UV0,UV1,UV2);
		UVGradient[UVIndex] = LocalToParameter * ParameterToUV;
	}
}

/** Util that tries to combine two triangles if possible. */
static bool MergeTriangleIntoPolygon(
	FClipSMPolygon& Polygon,
	const FClipSMTriangle& Triangle)
{
	// The triangles' attributes must match the polygon.
	if(Polygon.MaterialIndex != Triangle.MaterialIndex)
	{
		return false;
	}
	if(Polygon.bOverrideTangentBasis != Triangle.bOverrideTangentBasis)
	{
		return false;
	}
	if(Polygon.NumUVs != Triangle.NumUVs)
	{
		return false;
	}
	if(!Polygon.bOverrideTangentBasis && Polygon.SmoothingMask != Triangle.SmoothingMask)
	{
		return false;
	}

	// The triangle must have the same normal as the polygon
	if(!Triangle.FaceNormal.Equals(Polygon.FaceNormal,THRESH_NORMALS_ARE_SAME))
	{
		return false;
	}

	// The triangle must have the same attribute gradients as the polygon
	if(!Triangle.TangentXGradient.Equals(Polygon.TangentXGradient))
	{
		return false;
	}
	if(!Triangle.TangentYGradient.Equals(Polygon.TangentYGradient))
	{
		return false;
	}
	if(!Triangle.TangentZGradient.Equals(Polygon.TangentZGradient))
	{
		return false;
	}
	if(!Triangle.ColorGradient.Equals(Polygon.ColorGradient))
	{
		return false;
	}
	for(int32 UVIndex = 0;UVIndex < Triangle.NumUVs;UVIndex++)
	{
		if(!Triangle.UVGradient[UVIndex].Equals(Polygon.UVGradient[UVIndex]))
		{
			return false;
		}
	}

	for(int32 PolygonEdgeIndex = 0;PolygonEdgeIndex < Polygon.Vertices.Num();PolygonEdgeIndex++)
	{
		const uint32 PolygonEdgeVertex0 = PolygonEdgeIndex + 0;
		const uint32 PolygonEdgeVertex1 = (PolygonEdgeIndex + 1) % Polygon.Vertices.Num();

		for(uint32 TriangleEdgeIndex = 0;TriangleEdgeIndex < 3;TriangleEdgeIndex++)
		{
			const uint32 TriangleEdgeVertex0 = TriangleEdgeIndex + 0;
			const uint32 TriangleEdgeVertex1 = (TriangleEdgeIndex + 1) % 3;

			// If the triangle and polygon share an edge, then the triangle is in the same plane (implied by the above normal check),
			// and may be merged into the polygon.
			if(	VertsAreEqual(Polygon.Vertices[PolygonEdgeVertex0],Triangle.Vertices[TriangleEdgeVertex1]) &&
				VertsAreEqual(Polygon.Vertices[PolygonEdgeVertex1],Triangle.Vertices[TriangleEdgeVertex0]))
			{
				// Add the triangle's vertex that isn't in the adjacent edge to the polygon in between the vertices of the adjacent edge.
				const int32 TriangleOppositeVertexIndex = (TriangleEdgeIndex + 2) % 3;
				Polygon.Vertices.Insert(Triangle.Vertices[TriangleOppositeVertexIndex],PolygonEdgeVertex1);

				return true;
			}
		}
	}

	// Could not merge triangles.
	return false;
}

/** Given a set of triangles, remove those which share an edge and could be collapsed into one triangle. */
void FGeomTools::RemoveRedundantTriangles(TArray<FClipSMTriangle>& Tris)
{
	TArray<FClipSMPolygon> Polygons;

	// Merge the triangles into polygons.
	while(Tris.Num() > 0)
	{
		// Start building a polygon from the last triangle in the array.
		const FClipSMTriangle InitialTriangle = Tris.Pop();
		FClipSMPolygon MergedPolygon(0);
		MergedPolygon.CopyFace(InitialTriangle);
		MergedPolygon.Vertices.Add(InitialTriangle.Vertices[0]);
		MergedPolygon.Vertices.Add(InitialTriangle.Vertices[1]);
		MergedPolygon.Vertices.Add(InitialTriangle.Vertices[2]);

		// Find triangles that can be merged into the polygon.
		for(int32 CandidateTriangleIndex = 0;CandidateTriangleIndex < Tris.Num();CandidateTriangleIndex++)
		{
			const FClipSMTriangle& MergeCandidateTriangle = Tris[CandidateTriangleIndex];
			if(MergeTriangleIntoPolygon(MergedPolygon, MergeCandidateTriangle))
			{
				// Remove the merged triangle from the array.
				Tris.RemoveAtSwap(CandidateTriangleIndex);

				// Restart the search for mergeable triangles from the start of the array.
				CandidateTriangleIndex = -1;
			}
		}

		// Add the merged polygon to the array.
		Polygons.Add(MergedPolygon);
	}

	// Triangulate each polygon and add it to the output triangle array.
	for(int32 PolygonIndex = 0;PolygonIndex < Polygons.Num();PolygonIndex++)
	{
		TArray<FClipSMTriangle> Triangles;
		TriangulatePoly(Triangles,Polygons[PolygonIndex]);
		Tris.Append(Triangles);
	}
}

/** Util class for clipping polygon to a half space in 2D */
class FSplitLine2D
{
private:
	float X, Y, W;

public:
	FSplitLine2D() 
	{}

	FSplitLine2D( const FVector2D& InBase, const FVector2D &InNormal )
	{
		X = InNormal.X;
		Y = InNormal.Y;
		W = (InBase | InNormal);
	}


	float PlaneDot( const FVector2D &P ) const
	{
		return X*P.X + Y*P.Y - W;
	}
};

/** Split 2D polygons with a 3D plane. */
void FGeomTools::Split2DPolysWithPlane(FUtilPoly2DSet& PolySet, const FPlane& Plane, const FColor& ExteriorVertColor, const FColor& InteriorVertColor)
{
	// Break down world-space plane into normal and base
	FVector WNormal =  FVector(Plane.X, Plane.Y, Plane.Z);
	FVector WBase = WNormal * Plane.W;

	// Convert other plane into local space
	FVector LNormal = PolySet.PolyToWorld.InverseTransformVector(WNormal);

	// If planes are parallel, see if it clips away everything
	if(FMath::Abs(LNormal.Z) > (1.f - 0.001f))
	{
		// Check distance of this plane from the other
		float Dist = Plane.PlaneDot(PolySet.PolyToWorld.GetOrigin());
		// Its in front - remove all polys
		if(Dist > 0.f)
		{
			PolySet.Polys.Empty();
		}

		return;
	}

	FVector LBase = PolySet.PolyToWorld.InverseTransformPosition(WBase);

	// Project 0-plane normal into other plane - we will trace along this line to find intersection of two planes.
	FVector NormInOtherPlane = FVector(0,0,1) - (LNormal * (FVector(0,0,1) | LNormal));

	// Find direction of plane-plane intersect line
	//FVector LineDir = LNormal ^ FVector(0,0,1);
	// Cross this with other plane normal to find vector in other plane which will intersect this plane (0-plane)
	//FVector V = LineDir ^ LNormal;

	// Find second point along vector
	FVector VEnd = LBase - (10.f * NormInOtherPlane);
	// Find intersection
	FVector Intersect = FMath::LinePlaneIntersection(LBase, VEnd, FVector::ZeroVector, FVector(0,0,1));
	check(!Intersect.ContainsNaN());

	// Make 2D line.
	FVector2D Normal2D = FVector2D(LNormal.X, LNormal.Y).GetSafeNormal();
	FVector2D Base2D = FVector2D(Intersect.X, Intersect.Y);
	FSplitLine2D Plane2D(Base2D, Normal2D);

	for(int32 PolyIndex=PolySet.Polys.Num()-1; PolyIndex>=0; PolyIndex--)
	{
		FUtilPoly2D& Poly = PolySet.Polys[PolyIndex];
		int32 NumPVerts = Poly.Verts.Num();

		// Calculate distance of verts from clipping line
		TArray<float> PlaneDist;
		PlaneDist.AddZeroed(NumPVerts);
		for(int32 i=0; i<NumPVerts; i++)
		{
			PlaneDist[i] = Plane2D.PlaneDot(Poly.Verts[i].Pos);
		}

		TArray<FUtilVertex2D> FinalVerts;
		for(int32 ThisVert=0; ThisVert<NumPVerts; ThisVert++)
		{
			bool bStartInside = (PlaneDist[ThisVert] < 0.f);
			// If start vert is inside, add it.
			if(bStartInside)
			{
				FinalVerts.Add( Poly.Verts[ThisVert] );
			}

			// If start and next vert are on opposite sides, add intersection
			int32 NextVert = (ThisVert+1)%NumPVerts;

			if(PlaneDist[ThisVert] * PlaneDist[NextVert] < 0.f)
			{
				// Find distance along edge that plane is
				float Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);
				FVector2D NewVertPos = FMath::Lerp(Poly.Verts[ThisVert].Pos, Poly.Verts[NextVert].Pos, Alpha);

				// New color based on whether we are cutting an 'interior' edge.
				FColor NewVertColor = (Poly.Verts[ThisVert].bInteriorEdge) ? InteriorVertColor : ExteriorVertColor;

				FUtilVertex2D NewVert = FUtilVertex2D(NewVertPos, NewVertColor);

				// We mark this the start of an interior edge if the edge we cut started inside.
				if(bStartInside || Poly.Verts[ThisVert].bInteriorEdge)
				{
					NewVert.bInteriorEdge = true;
				}

				// Save vert
				FinalVerts.Add(NewVert);
			}
		}

		// If we have no verts left, all clipped away, remove from set of polys
		if(FinalVerts.Num() == 0)
		{
			PolySet.Polys.RemoveAt(PolyIndex);
		}
		// Copy new set of verts back to poly.
		else
		{
			Poly.Verts = FinalVerts;
		}
	}
}
