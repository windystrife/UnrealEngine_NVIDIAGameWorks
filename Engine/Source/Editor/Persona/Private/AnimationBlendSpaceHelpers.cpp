// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationBlendSpaceHelpers.h"

#define BLENDSPACE_MINSAMPLE	3
#define BLENDSPACE_MINAXES		1
#define BLENDSPACE_MAXAXES		3

#define LOCTEXT_NAMESPACE "AnimationBlendSpaceHelpers"

struct FPointWithIndex
{
	FPoint Point;
	int32 OriginalIndex;

	FPointWithIndex(const FPoint& P, int32 InOriginalIndex)
		:	Point(P),
			OriginalIndex(InOriginalIndex)
	{
	}
};

struct FSortByDistance
{
	int32 Index;
	float Distance;
	FSortByDistance(int32 InIndex, float InDistance) 
		: Index(InIndex), Distance(InDistance) {}
};

void FDelaunayTriangleGenerator::EmptyTriangles()
{
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleList.Num(); ++TriangleIndex)
	{
		delete TriangleList[TriangleIndex];
	}

	TriangleList.Empty();
}
void FDelaunayTriangleGenerator::EmptySamplePoints()
{
	SamplePointList.Empty();
}

void FDelaunayTriangleGenerator::Reset()
{
	EmptyTriangles();
	EmptySamplePoints();
	IndiceMappingTable.Empty();
}

void FDelaunayTriangleGenerator::AddSamplePoint(const FVector& NewPoint, const int32 SampleIndex)
{
	checkf(!SamplePointList.Contains(NewPoint), TEXT("Found duplicate points in blendspace"));
	SamplePointList.Add(FPoint(NewPoint));
	IndiceMappingTable.Add(SampleIndex);
}

void FDelaunayTriangleGenerator::Triangulate()
{
	if (SamplePointList.Num() == 0)
	{
		return;
	}
	else if (SamplePointList.Num() == 1)
	{
		// degenerate case 1
		FTriangle Triangle(&SamplePointList[0]);
		AddTriangle(Triangle);
	}
	else if (SamplePointList.Num() == 2)
	{
		// degenerate case 2
		FTriangle Triangle(&SamplePointList[0], &SamplePointList[1]);
		AddTriangle(Triangle);
	}
	else
	{
		SortSamples();

		// first choose first 3 points
		for (int32 I = 2; I<SamplePointList.Num(); ++I)
		{
			GenerateTriangles(SamplePointList, I + 1);
		}

		// degenerate case 3: many points all collinear or coincident
		if (TriangleList.Num() == 0)
		{
			if (AllCoincident(SamplePointList))
			{
				// coincident case - just create one triangle
				FTriangle Triangle(&SamplePointList[0]);
				AddTriangle(Triangle);
			}
			else
			{
				// collinear case: create degenerate triangles between pairs of points
				for (int32 PointIndex = 0; PointIndex < SamplePointList.Num() - 1; ++PointIndex)
				{
					FTriangle Triangle(&SamplePointList[PointIndex], &SamplePointList[PointIndex + 1]);
					AddTriangle(Triangle);
				}
			}
		}
	}
}

void FDelaunayTriangleGenerator::SortSamples()
{
	// Populate sorting array with sample points and their original (blend space -> sample data) indices
	TArray<FPointWithIndex> SortedPoints;
	SortedPoints.Reserve(SamplePointList.Num());
	for (int32 SampleIndex = 0; SampleIndex < SamplePointList.Num(); ++SampleIndex)
	{
		SortedPoints.Add(FPointWithIndex(SamplePointList[SampleIndex], IndiceMappingTable[SampleIndex]));
	}

	// return A-B
	struct FComparePoints
	{
		FORCEINLINE bool operator()( const FPointWithIndex &A, const FPointWithIndex&B ) const
		{
			// the sorting happens from -> +X, -> +Y,  -> for now ignore Z ->+Z
			if( A.Point.Position.X == B.Point.Position.X ) // same, then compare Y
			{
				if( A.Point.Position.Y == B.Point.Position.Y )
				{
					return A.Point.Position.Z < B.Point.Position.Z;
				}
				else
				{
					return A.Point.Position.Y < B.Point.Position.Y;
				}
			}
			
			return A.Point.Position.X < B.Point.Position.X;			
		}
	};
	// sort all points
	SortedPoints.Sort( FComparePoints() );

	// now copy back to SamplePointList
	IndiceMappingTable.Empty(SamplePointList.Num());
	IndiceMappingTable.AddUninitialized(SamplePointList.Num());
	for (int32 SampleIndex = 0; SampleIndex < SamplePointList.Num(); ++SampleIndex)
	{
		SamplePointList[SampleIndex] = SortedPoints[SampleIndex].Point;
		IndiceMappingTable[SampleIndex] = SortedPoints[SampleIndex].OriginalIndex;
	}
}

/** 
* The key function in Delaunay Triangulation
* return true if the TestPoint is WITHIN the triangle circumcircle
*	http://en.wikipedia.org/wiki/Delaunay_triangulation 
*/
FDelaunayTriangleGenerator::ECircumCircleState FDelaunayTriangleGenerator::GetCircumcircleState(const FTriangle* T, const FPoint& TestPoint)
{
	const int32 NumPointsPerTriangle = 3;

	// First off, normalize all the points
	FVector NormalizedPositions[NumPointsPerTriangle];
	
	// Unrolled loop
	NormalizedPositions[0] = (T->Vertices[0]->Position - GridMin) * RecipGridSize;
	NormalizedPositions[1] = (T->Vertices[1]->Position - GridMin) * RecipGridSize;
	NormalizedPositions[2] = (T->Vertices[2]->Position - GridMin) * RecipGridSize;

	const FVector NormalizedTestPoint = ( TestPoint.Position - GridMin ) * RecipGridSize;

	// ignore Z, eventually this has to be on plane
	// http://en.wikipedia.org/wiki/Delaunay_triangulation - determinant
	const float M00 = NormalizedPositions[0].X - NormalizedTestPoint.X;
	const float M01 = NormalizedPositions[0].Y - NormalizedTestPoint.Y;
	const float M02 = NormalizedPositions[0].X * NormalizedPositions[0].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[0].Y*NormalizedPositions[0].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float M10 = NormalizedPositions[1].X - NormalizedTestPoint.X;
	const float M11 = NormalizedPositions[1].Y - NormalizedTestPoint.Y;
	const float M12 = NormalizedPositions[1].X * NormalizedPositions[1].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[1].Y * NormalizedPositions[1].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float M20 = NormalizedPositions[2].X - NormalizedTestPoint.X;
	const float M21 = NormalizedPositions[2].Y - NormalizedTestPoint.Y;
	const float M22 = NormalizedPositions[2].X * NormalizedPositions[2].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[2].Y * NormalizedPositions[2].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float Det = M00*M11*M22+M01*M12*M20+M02*M10*M21 - (M02*M11*M20+M01*M10*M22+M00*M12*M21);
	
	// When the vertices are sorted in a counterclockwise order, the determinant is positive if and only if Testpoint lies inside the circumcircle of T.
	if (FMath::IsNegativeFloat(Det))
	{
		return ECCS_Outside;
	}
	else
	{
		// On top of the triangle edge
		if (FMath::IsNearlyZero(Det))
		{
			return ECCS_On;
		}
		else
		{
			return ECCS_Inside;
		}
	}
}

/** 
* return true if 3 points are collinear
* by that if those 3 points create straight line
*/
bool FDelaunayTriangleGenerator::IsCollinear(const FPoint* A, const FPoint* B, const FPoint* C)
{
	// 		FVector A2B = (B.Position-A.Position).SafeNormal();
	// 		FVector A2C = (C.Position-A.Position).SafeNormal();
	// 		
	// 		float fDot = fabs(A2B | A2C);
	// 		return (fDot > SMALL_NUMBER);

	// this eventually has to happen on the plane that contains this 3 pages
	// for now we ignore Z
	const FVector Diff1 = B->Position - A->Position;
	const FVector Diff2 = C->Position - A->Position;

	const float Result = Diff1.X*Diff2.Y - Diff1.Y*Diff2.X;

	return (Result == 0.f);
}

bool FDelaunayTriangleGenerator::AllCoincident(const TArray<FPoint>& InPoints)
{
	if (InPoints.Num() > 0)
	{
		const FPoint& FirstPoint = InPoints[0];
		for (int32 PointIndex = 0; PointIndex < InPoints.Num(); ++PointIndex)
		{
			const FPoint& Point = InPoints[PointIndex];
			if (Point.Position != FirstPoint.Position)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FDelaunayTriangleGenerator::FlipTriangles(const int32 TriangleIndexOne, const int32 TriangleIndexTwo)
{
	const FTriangle* A = TriangleList[TriangleIndexOne];
	const FTriangle* B = TriangleList[TriangleIndexTwo];

	// if already optimized, don't have to do any
	FPoint* TestPt = A->FindNonSharingPoint(B);

	// If it's not inside, we don't have to do any
	if (GetCircumcircleState(A, *TestPt) != ECCS_Inside)
	{
		return false;
	}

	FTriangle NewTriangles[2];
	int32 TrianglesMade = 0;

	for (int32 VertexIndexOne = 0; VertexIndexOne < 2; ++VertexIndexOne)
	{
		for (int32 VertexIndexTwo = VertexIndexOne + 1; VertexIndexTwo < 3; ++VertexIndexTwo)
		{
			// Check if these vertices form a valid triangle (should be non-colinear)
			if (IsEligibleForTriangulation(A->Vertices[VertexIndexOne], A->Vertices[VertexIndexTwo], TestPt))
			{
				// Create the new triangle and check if the final (original) vertex falls inside or outside of it's circumcircle
				const FTriangle NewTriangle(A->Vertices[VertexIndexOne], A->Vertices[VertexIndexTwo], TestPt);
				const int32 VertexIndexThree = 3 - (VertexIndexTwo + VertexIndexOne);
				if (GetCircumcircleState(&NewTriangle, *A->Vertices[VertexIndexThree]) == ECCS_Outside)
				{
					// If so store the triangle and increment the number of triangles
					checkf(TrianglesMade < 2, TEXT("Incorrect number of triangles created"));
					NewTriangles[TrianglesMade] = NewTriangle;
					++TrianglesMade;
				}
			}
		}
	}
	
	// In case two triangles were generated the flip was successful so we can add them to the list
	if (TrianglesMade == 2)
	{
		AddTriangle(NewTriangles[0], false);
		AddTriangle(NewTriangles[1], false);
	}

	return TrianglesMade == 2;
}

void FDelaunayTriangleGenerator::AddTriangle(FTriangle& newTriangle, bool bCheckHalfEdge/*=true*/)
{
	// see if it's same vertices
	for (int32 I=0;I<TriangleList.Num(); ++I)
	{
		if (newTriangle == *TriangleList[I])
		{
			return;
		}

		if (bCheckHalfEdge && newTriangle.HasSameHalfEdge(TriangleList[I]))
		{
			return;
		}
	}

	TriangleList.Add(new FTriangle(newTriangle));
}

int32 FDelaunayTriangleGenerator::GenerateTriangles(TArray<FPoint>& PointList, const int32 TotalNum)
{
	if (TotalNum == BLENDSPACE_MINSAMPLE)
	{
		if (IsEligibleForTriangulation(&PointList[0], &PointList[1], &PointList[2]))
		{
			FTriangle Triangle(&PointList[0], &PointList[1], &PointList[2]);
			AddTriangle(Triangle);
		}
	}
	else if (TriangleList.Num() == 0)
	{
		FPoint * TestPoint = &PointList[TotalNum-1];

		// so far no triangle is made, try to make it with new points that are just entered
		for (int32 I=0; I<TotalNum-2; ++I)
		{
			if (IsEligibleForTriangulation(&PointList[I], &PointList[I+1], TestPoint))
			{
				FTriangle NewTriangle (&PointList[I], &PointList[I+1], TestPoint);
				AddTriangle(NewTriangle);
			}
		}
	}
	else
	{
		// get the last addition
		FPoint * TestPoint = &PointList[TotalNum-1];
		int32 TriangleNum = TriangleList.Num();
	
		for (int32 I=0; I<TriangleList.Num(); ++I)
		{
			FTriangle * Triangle = TriangleList[I];
			if (IsEligibleForTriangulation(Triangle->Vertices[0], Triangle->Vertices[1], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[0], Triangle->Vertices[1], TestPoint);
				AddTriangle(NewTriangle);
			}

			if (IsEligibleForTriangulation(Triangle->Vertices[0], Triangle->Vertices[2], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[0], Triangle->Vertices[2], TestPoint);
				AddTriangle(NewTriangle);
			}

			if (IsEligibleForTriangulation(Triangle->Vertices[1], Triangle->Vertices[2], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[1], Triangle->Vertices[2], TestPoint);
				AddTriangle(NewTriangle);
			}
		}

		// this is locally optimization part
		// we need to make sure all triangles are locally optimized. If not optimize it. 
		for (int32 I=0; I<TriangleList.Num(); ++I)
		{
			FTriangle * A = TriangleList[I];
			for (int32 J=I+1; J<TriangleList.Num(); ++J)
			{
				FTriangle * B = TriangleList[J];

				// does share same edge
				if (A->DoesShareSameEdge(B))
				{
					// then test to see if locally optimized
					if (FlipTriangles(I, J))
					{
						// if this flips, remove current triangle
						delete TriangleList[I];
						delete TriangleList[J];
						//I need to remove J first because other wise, 
						//  index J isn't valid anymore
						TriangleList.RemoveAt(J);
						TriangleList.RemoveAt(I);
						// start over since we modified triangle
						// once we don't have any more to flip, we're good to go!
						I=-1;
						break;
					}
				}
			}
		}
	}

	return TriangleList.Num();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Find Triangle this TestPoint is within
* 
* @param	TestPoint				Point to test
* @param	OutBaryCentricCoords	Output BaryCentricCoords2D of the point in the triangle // for now it's only 2D
* @param	OutTriangle				The triangle that this point is within or lie
* @param	TriangleList			TriangleList to test
* 
* @return	true if successfully found the triangle this point is within
*/
bool FBlendSpaceGrid::FindTriangleThisPointBelongsTo(const FVector& TestPoint, FVector& OutBaryCentricCoords, FTriangle*& OutTriangle, const TArray<FTriangle*>& TriangleList) const
{
	// Calculate distance from point to triangle and sort the triangle list accordingly
	TArray<FSortByDistance> SortedTriangles;
	SortedTriangles.AddUninitialized(TriangleList.Num());
	for (int32 TriangleIndex=0; TriangleIndex<TriangleList.Num(); ++TriangleIndex)
	{
		SortedTriangles[TriangleIndex].Index = TriangleIndex;
		SortedTriangles[TriangleIndex].Distance = TriangleList[TriangleIndex]->GetDistance(TestPoint);
	}
	SortedTriangles.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });

	// Now loop over the sorted triangles and test the barycentric coordinates with the point
	for (const FSortByDistance& SortedTriangle : SortedTriangles)
	{
		FTriangle* Triangle = TriangleList[SortedTriangle.Index];

		FVector Coords = FMath::GetBaryCentric2D(TestPoint, Triangle->Vertices[0]->Position, Triangle->Vertices[1]->Position, Triangle->Vertices[2]->Position);

		// Z coords often has precision error because it's derived from 1-A-B, so do more precise check
		if (FMath::Abs(Coords.Z) < KINDA_SMALL_NUMBER)
		{
			Coords.Z = 0.f;
		}

		// Is the point inside of the triangle, or on it's edge (Z coordinate should always match since the blend samples are set in 2D)
		if ( 0.f <= Coords.X && Coords.X <= 1.0 && 0.f <= Coords.Y && Coords.Y <= 1.0 && 0.f <= Coords.Z && Coords.Z <= 1.0 )
		{
			OutBaryCentricCoords = Coords;
			OutTriangle = Triangle;
			return true;
		}
	}

	return false;
}

/** 
* Fill up Grid Elements using TriangleList input
* 
* @param	SamplePoints		: Sample Point List
* @param	TriangleList		: List of triangles
*/
void FBlendSpaceGrid::GenerateGridElements(const TArray<FPoint>& SamplePoints, const TArray<FTriangle*>& TriangleList)
{
	check (NumGridDivisions.X > 0 && NumGridDivisions.Y > 0 );
	check ( GridDimensions.IsValid );

	const int32 TotalNumGridPoints = NumGridPointsForAxis.X * NumGridPointsForAxis.Y;

	GridPoints.Empty(TotalNumGridPoints);

	if (SamplePoints.Num() == 0 || TriangleList.Num() == 0)
	{
		return;
	}

	GridPoints.AddDefaulted(TotalNumGridPoints);

	FVector GridPointPosition;		
	for (int32 GridPositionX = 0; GridPositionX < NumGridPointsForAxis.X; ++GridPositionX)
	{
		for (int32 GridPositionY = 0; GridPositionY < NumGridPointsForAxis.Y; ++GridPositionY)
		{
			FTriangle * SelectedTriangle = NULL;
			FEditorElement& GridPoint = GridPoints[ GridPositionX*NumGridPointsForAxis.Y + GridPositionY ];

			GridPointPosition = GetPosFromIndex(GridPositionX, GridPositionY);

			FVector Weights;
			if ( FindTriangleThisPointBelongsTo(GridPointPosition, Weights, SelectedTriangle, TriangleList) )
			{
				// found it
				GridPoint.Weights[0] = Weights.X;
				GridPoint.Weights[1] = Weights.Y;
				GridPoint.Weights[2] = Weights.Z;
				// need to find sample point index
				// @todo fix this with better solution
				// lazy me
				GridPoint.Indices[0] = SamplePoints.Find(*SelectedTriangle->Vertices[0]);
				GridPoint.Indices[1] = SamplePoints.Find(*SelectedTriangle->Vertices[1]);
				GridPoint.Indices[2] = SamplePoints.Find(*SelectedTriangle->Vertices[2]);

				check(GridPoint.Indices[0] != INDEX_NONE);
				check(GridPoint.Indices[1] != INDEX_NONE);
				check(GridPoint.Indices[2] != INDEX_NONE);
			}
			else
			{
				TArray<FSortByDistance> SortedTriangles;
				for (int32 TriangleIndex = 0; TriangleIndex < TriangleList.Num(); ++TriangleIndex )
				{
					// Check if points are collinear
					const FTriangle* Triangle = TriangleList[TriangleIndex];
					const FVector EdgeA = Triangle->Vertices[1]->Position - Triangle->Vertices[0]->Position;
					const FVector EdgeB = Triangle->Vertices[2]->Position - Triangle->Vertices[0]->Position;
					const float Result = EdgeA.X * EdgeB.Y - EdgeA.Y * EdgeB.X;
					// Only add valid triangles
					if (Result > 0.0f)
					{ 
						SortedTriangles.Add(FSortByDistance(TriangleIndex, Triangle->GetDistance(GridPointPosition)));
					}					
				}

				if (SortedTriangles.Num())
				{
					SortedTriangles.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });
					FTriangle* ClosestTriangle = TriangleList[SortedTriangles[0].Index];

					// For the closest triangle, determine which of its edges is closest to the grid point
					TArray<FSortByDistance> Edges;
					TArray<FVector> PointsOnEdges;
					for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
					{
						const FVector ClosestPoint = FMath::ClosestPointOnLine(ClosestTriangle->Edges[EdgeIndex].Vertices[0]->Position, ClosestTriangle->Edges[EdgeIndex].Vertices[1]->Position, GridPointPosition);
						Edges.Add(FSortByDistance(EdgeIndex, (ClosestPoint - GridPointPosition).SizeSquared()));
						PointsOnEdges.Add(ClosestPoint);
					}
					Edges.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });
					
					// Calculate weighting using the closest edge points and the clamped grid position on the line
					const FVector GridWeights = FMath::GetBaryCentric2D(PointsOnEdges[Edges[0].Index], ClosestTriangle->Vertices[0]->Position, ClosestTriangle->Vertices[1]->Position, ClosestTriangle->Vertices[2]->Position);
					
					for (int32 Index = 0; Index < 3; ++Index)
					{
						GridPoint.Weights[Index] = GridWeights[Index];
						GridPoint.Indices[Index] = SamplePoints.Find(*ClosestTriangle->Vertices[Index]);
					}
				}
				else
				{
					// This means that there is either one point, two points or collinear triangles on the grid
					if (SamplePoints.Num() == 1)
					{
						// Just one, fill all grid points to the single sample
						GridPoint.Weights[0] = 1.0f;
						GridPoint.Indices[0] = 0;
					}
					else
					{
						// Two points or co-linear triangles, first find the two closest samples
						TArray<FSortByDistance> SampleDistances;
						for (int32 PointIndex = 0; PointIndex < SamplePoints.Num(); ++PointIndex)
						{
							const float DistanceFromSampleToPoint= (SamplePoints[PointIndex].Position - GridPointPosition).SizeSquared2D();
							SampleDistances.Add(FSortByDistance(PointIndex, DistanceFromSampleToPoint));
						}
						SampleDistances.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });

						// Find closest point on line between the two samples (clamping the grid position to the line, just like clamping to the triangle edges)
						const FPoint Samples[2] = { SamplePoints[SampleDistances[0].Index], SamplePoints[SampleDistances[1].Index] };
						const FVector ClosestPointOnLine = FMath::ClosestPointOnLine(Samples[0].Position, Samples[1].Position, GridPointPosition);
						const float LineLength = (Samples[0].Position - Samples[1].Position).SizeSquared2D();

						// Weight the samples according to the distance from the grid point on the line to the samples
						for (int32 SampleIndex = 0; SampleIndex < 2; ++SampleIndex)
						{
							GridPoint.Weights[SampleIndex] = (LineLength - (Samples[SampleIndex].Position - ClosestPointOnLine).SizeSquared2D()) / LineLength;
							GridPoint.Indices[SampleIndex] = SamplePoints.Find(Samples[SampleIndex]);
						}	
					}
				}				
			}
		}
	}
}

/** 
* Convert grid index (GridX, GridY) to triangle coords and returns FVector
*/
const FVector FBlendSpaceGrid::GetPosFromIndex(const int32 GridX, const int32 GridY) const
{
	// grid X starts from 0 -> N when N == GridSizeX
	// grid Y starts from 0 -> N when N == GridSizeY
	// LeftBottom will map to Grid 0, 0
	// RightTop will map to Grid N, N

	FVector2D CoordDim (GridDimensions.GetSize());
	FVector2D EachGridSize = CoordDim / NumGridDivisions;

	// for now only 2D
	return FVector(GridX*EachGridSize.X+GridDimensions.Min.X, GridY*EachGridSize.Y+GridDimensions.Min.Y, 0.f);
}

const FEditorElement& FBlendSpaceGrid::GetElement(const int32 GridX, const int32 GridY) const
{
	check (NumGridPointsForAxis.X >= GridX);
	check (NumGridPointsForAxis.Y >= GridY);

	check (GridPoints.Num() > 0 );
	return GridPoints[GridX * NumGridPointsForAxis.Y + GridY];
}

#undef LOCTEXT_NAMESPACE
