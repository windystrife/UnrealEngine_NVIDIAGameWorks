// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
SkeletalMeshSorting.cpp: Static sorting for skeletal mesh triangles
=============================================================================*/

#include "SkeletalMeshSorting.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "MeshUtilities.h"

void CacheOptimizeSortStrip(uint32* Indices, int32 NumIndices)
{
	TArray<uint32> TempIndices;
	TempIndices.AddUninitialized(NumIndices);
	FMemory::Memcpy(TempIndices.GetData(), Indices, NumIndices * sizeof(uint32));
	IMeshUtilities& MeshUtilities = FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.CacheOptimizeIndexBuffer(TempIndices);
	check(TempIndices.Num() == NumIndices);
	FMemory::Memcpy(Indices, TempIndices.GetData(), NumIndices * sizeof(uint32));
}

// struct to hold a strip, for sorting by key.
struct FTriStripSortInfo
{
	TArray<int32> Triangles;
	float SortKey;
};

/**
* Group triangles into connected strips 
*
* @param NumTriangles - The number of triangles to group
* @param Indices - pointer to the index buffer data
* @outparam OutTriSet - an array containing the set number for each triangle.
* @return the maximum set number of any triangle.
*/
int32 GetConnectedTriangleSets( int32 NumTriangles, const uint32* Indices, TArray<uint32>& OutTriSet )
{
	// 1. find connected strips of triangles
	union EdgeInfo
	{		
		uint32 Indices[2];
		uint64 QWData;
		EdgeInfo( uint64 InQWData )
			:	QWData(InQWData)
		{}
		EdgeInfo( uint32 Index1, uint32 Index2 )
		{
			Indices[0] = FMath::Min<uint32>(Index1,Index2);
			Indices[1] = FMath::Max<uint32>(Index1,Index2);
		}
	};

	// Map from edge to first triangle using that edge
	TMap<uint64, int32> EdgeTriMap;
	int32 MaxTriSet = 0;
	for( int32 TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
	{
		uint32 i1 = Indices[TriIndex*3+0];
		uint32 i2 = Indices[TriIndex*3+1];
		uint32 i3 = Indices[TriIndex*3+2];

		uint64 Edge1 = EdgeInfo(i1,i2).QWData;
		uint64 Edge2 = EdgeInfo(i2,i3).QWData;
		uint64 Edge3 = EdgeInfo(i3,i1).QWData;

		int32* Edge1TriPtr = EdgeTriMap.Find(Edge1);
		int32* Edge2TriPtr = EdgeTriMap.Find(Edge2);
		int32* Edge3TriPtr = EdgeTriMap.Find(Edge3);

		if( !Edge1TriPtr && !Edge2TriPtr && !Edge3TriPtr )
		{
			OutTriSet.Add(MaxTriSet++);
			// none of the edges have been found before.
			EdgeTriMap.Add(Edge1, TriIndex);
			EdgeTriMap.Add(Edge2, TriIndex);
			EdgeTriMap.Add(Edge3, TriIndex);
		}
		else
		if( Edge1TriPtr && !Edge2TriPtr && !Edge3TriPtr )
		{
			// triangle belongs to triangle Edge1's set.
			int32 Edge1Tri = *Edge1TriPtr;
			int32 Edge1TriSet = OutTriSet[Edge1Tri];
			OutTriSet.Add(Edge1TriSet);
			EdgeTriMap.Add(Edge2, Edge1Tri);
			EdgeTriMap.Add(Edge3, Edge1Tri);
		}
		else
		if( Edge2TriPtr && !Edge1TriPtr && !Edge3TriPtr )
		{
			// triangle belongs to triangle Edge2's set.
			int32 Edge2Tri = *Edge2TriPtr;
			int32 Edge2TriSet = OutTriSet[Edge2Tri];
			OutTriSet.Add(Edge2TriSet);
			EdgeTriMap.Add(Edge1, Edge2Tri);
			EdgeTriMap.Add(Edge3, Edge2Tri);
		}
		else
		if( Edge3TriPtr && !Edge1TriPtr && !Edge2TriPtr )
		{
			// triangle belongs to triangle Edge3's set.
			int32 Edge3Tri = *Edge3TriPtr;
			int32 Edge3TriSet = OutTriSet[Edge3Tri];
			OutTriSet.Add(Edge3TriSet);
			EdgeTriMap.Add(Edge1, Edge3Tri);
			EdgeTriMap.Add(Edge2, Edge3Tri);
		}
		else
		if( Edge1TriPtr && Edge2TriPtr && !Edge3TriPtr )
		{
			int32 Edge1TriSet = OutTriSet[*Edge1TriPtr];
			int32 Edge2TriSet = OutTriSet[*Edge2TriPtr];
			OutTriSet.Add(Edge1TriSet);
			EdgeTriMap.Add(Edge3, *Edge1TriPtr);

			// triangle belongs to triangle Edge1 and Edge2's set.
			if( Edge1TriSet != Edge2TriSet )
			{
				// merge sets for Edge1 and Edge2
				for( int32 TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet[TriSetIndex]==Edge2TriSet )
					{
						OutTriSet[TriSetIndex]=Edge1TriSet;
					}
				}
			}
		}
		else
		if( Edge1TriPtr && Edge3TriPtr && !Edge2TriPtr )
		{
			int32 Edge1TriSet = OutTriSet[*Edge1TriPtr];
			int32 Edge3TriSet = OutTriSet[*Edge3TriPtr];
			OutTriSet.Add(Edge1TriSet);
			EdgeTriMap.Add(Edge2, *Edge1TriPtr);

			// triangle belongs to triangle Edge1 and Edge3's set.
			if( Edge1TriSet != Edge3TriSet )
			{
				// merge sets for Edge1 and Edge3
				for( int32 TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet[TriSetIndex]==Edge3TriSet )
					{
						OutTriSet[TriSetIndex]=Edge1TriSet;
					}
				}
			}
		}
		else
		if( Edge2TriPtr && Edge3TriPtr && !Edge1TriPtr )
		{
			int32 Edge2TriSet = OutTriSet[*Edge2TriPtr];
			int32 Edge3TriSet = OutTriSet[*Edge3TriPtr];
			OutTriSet.Add(Edge2TriSet);
			EdgeTriMap.Add(Edge1, *Edge2TriPtr);

			// triangle belongs to triangle Edge2 and Edge3's set.
			if( Edge2TriSet != Edge3TriSet )
			{
				// merge sets for Edge2 and Edge3
				for( int32 TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet[TriSetIndex]==Edge3TriSet )
					{
						OutTriSet[TriSetIndex]=Edge2TriSet;
					}
				}
			}
		}
		else
		if( Edge1TriPtr && Edge2TriPtr && Edge3TriPtr )
		{
			int32 Edge1TriSet = OutTriSet[*Edge1TriPtr];
			int32 Edge2TriSet = OutTriSet[*Edge2TriPtr];
			int32 Edge3TriSet = OutTriSet[*Edge3TriPtr];

			// triangle belongs to triangle Edge1, Edge2 and Edge3's set.
			if( Edge1TriSet != Edge2TriSet )
			{
				// merge sets for Edge1 and Edge2
				for( int32 TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet[TriSetIndex]==Edge2TriSet )
					{
						OutTriSet[TriSetIndex]=Edge1TriSet;
					}
				}
			}
			if( Edge1TriSet != Edge3TriSet )
			{
				// merge sets for Edge1 and Edge3
				for( int32 TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet[TriSetIndex]==Edge3TriSet )
					{
						OutTriSet[TriSetIndex]=Edge1TriSet;
					}
				}
			}
			OutTriSet.Add(Edge1TriSet);
		}
	}

	return MaxTriSet;
}

void SortTriangles_None( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices )
{
	CacheOptimizeSortStrip( Indices, NumTriangles*3 );
}

void SortTriangles_CenterRadialDistance( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices )
{
	// find average location of entire model and use that as the sorting center
	FVector SortCenter(0,0,0);
	TSet<FVector> AllVertsSet;
	int32 AllVertsCount=0;

	for( int32 TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
	{
		uint32 i1 = Indices[TriIndex*3+0];
		uint32 i2 = Indices[TriIndex*3+1];
		uint32 i3 = Indices[TriIndex*3+2];

		bool bDuplicate=false;
		AllVertsSet.Add(Vertices[i1].Position,&bDuplicate);
		if( !bDuplicate )
		{
			SortCenter += Vertices[i1].Position;
			AllVertsCount++;
		}
		AllVertsSet.Add(Vertices[i2].Position,&bDuplicate);
		if( !bDuplicate )
		{
			SortCenter += Vertices[i2].Position;
			AllVertsCount++;
		}
		AllVertsSet.Add(Vertices[i3].Position,&bDuplicate);
		if( !bDuplicate )
		{
			SortCenter += Vertices[i3].Position;
			AllVertsCount++;
		}
	}

	// Calc center of all verts.
	SortCenter /= AllVertsCount;

	SortTriangles_CenterRadialDistance(SortCenter,NumTriangles,Vertices,Indices);
}

void SortTriangles_CenterRadialDistance( FVector SortCenter, int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices )
{
	// Get the set number for each triangle
	TArray<uint32> TriSet;
	int32 MaxTriSet = GetConnectedTriangleSets( NumTriangles, Indices, TriSet );

	TArray<FTriStripSortInfo> Strips;
	Strips.AddZeroed(MaxTriSet);
	for( int32 TriIndex=0;TriIndex<TriSet.Num();TriIndex++ )
	{
		Strips[TriSet[TriIndex]].Triangles.Add(TriIndex);
	}

	for( int32 s=0;s<Strips.Num();s++ )
	{
		if( Strips[s].Triangles.Num() == 0 )
		{
			Strips.RemoveAt(s);
			s--;
			continue;
		}
		
		FVector StripCenter(0,0,0);
		for( int32 TriIndex=0;TriIndex<Strips[s].Triangles.Num();TriIndex++ )
		{
			int32 tri = Strips[s].Triangles[TriIndex];
			uint32 i1 = Indices[tri*3+0];
			uint32 i2 = Indices[tri*3+1];
			uint32 i3 = Indices[tri*3+2];
			FVector TriCenter = (Vertices[i1].Position + Vertices[i2].Position + Vertices[i3].Position) / 3.f;
			StripCenter += TriCenter;
		}

		StripCenter /= Strips[s].Triangles.Num();
		Strips[s].SortKey = (StripCenter-SortCenter).SizeSquared();
	}

	struct FCompareSortKey
	{
		FORCEINLINE bool operator()( const FTriStripSortInfo& A, const FTriStripSortInfo& B ) const
		{
			return A.SortKey < B.SortKey;
		}
	};
	Strips.Sort( FCompareSortKey() );

	// export new draw order
	TArray<uint32> NewIndices;
	NewIndices.Empty(NumTriangles*3);
	for( int32 s=0;s<Strips.Num();s++ )
	{
		int32 StripStartIndex = NewIndices.Num();
		for( int32 TriIndex=0;TriIndex<Strips[s].Triangles.Num();TriIndex++ )
		{
			int32 tri = Strips[s].Triangles[TriIndex];
			NewIndices.Add(Indices[tri*3+0]);
			NewIndices.Add(Indices[tri*3+1]);
			NewIndices.Add(Indices[tri*3+2]);	
		}
		CacheOptimizeSortStrip( &NewIndices[StripStartIndex], Strips[s].Triangles.Num()*3 );
	}
	FMemory::Memcpy( Indices, NewIndices.GetData(), NewIndices.Num() * sizeof(uint32) );
}


void SortTriangles_Random( int32 NumTriangles, const FSoftSkinVertex* Vertices, uint32* Indices )
{
	TArray<int32> Triangles;
	for( int32 i=0;i<NumTriangles;i++ )
	{
		Triangles.Insert(i, i > 0 ? FMath::Rand() % i : 0);
	}

	// export new draw order
	TArray<uint32> NewIndices;
	NewIndices.Empty(NumTriangles*3);
	for( int TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
	{
		int32 tri = Triangles[TriIndex];
		NewIndices.Add(Indices[tri*3+0]);
		NewIndices.Add(Indices[tri*3+1]);
		NewIndices.Add(Indices[tri*3+2]);	
	}

	FMemory::Memcpy( Indices, NewIndices.GetData(), NewIndices.Num() * sizeof(uint32) );
}


void SortTriangles_MergeContiguous( int32 NumTriangles, int32 NumVertices, const FSoftSkinVertex* Vertices, uint32* Indices )
{
	// Build the list of triangle sets
	TArray<uint32> TriSet;
	GetConnectedTriangleSets( NumTriangles, Indices, TriSet );

	// Mapping from triangle set number to the array of indices that make up the contiguous strip.
	TMap<uint32, TArray<uint32> > Strips;

	int32 Index=0;
	for( int32 s=0;s<TriSet.Num();s++ )
	{
		// Store the indices for this triangle in the appropriate contiguous set.
		TArray<uint32>* ThisStrip = Strips.Find(TriSet[s]);
		if( !ThisStrip )
		{
			ThisStrip = &Strips.Add(TriSet[s],TArray<uint32>());
		}

		// Add the three indices for this triangle.
		ThisStrip->Add(Indices[Index++]);
		ThisStrip->Add(Indices[Index++]);
		ThisStrip->Add(Indices[Index++]);
	}

	// Export the indices in the same order.
	Index = 0;
	int32 PrevSet = INDEX_NONE;
	for( int32 s=0;s<TriSet.Num();s++ )
	{
		// The first time we see a triangle in a new set, export all the indices from that set.
		if( TriSet[s] != PrevSet )
		{
			TArray<uint32>* ThisStrip = Strips.Find(TriSet[s]);
			check(ThisStrip);

			if( ThisStrip->Num() > 0 )
			{
				check(Index < NumTriangles*3);
				FMemory::Memcpy( &Indices[Index], &(*ThisStrip)[0], ThisStrip->Num() * sizeof(uint32) );
				Index += ThisStrip->Num();

				// We want to export the whole strip contiguously, so we empty it so we don't export the
				// indices again when we see the same TriSet later.
				ThisStrip->Empty();
			}
		}
		PrevSet = TriSet[s];
	}
	check(Index == NumTriangles*3);
}

#endif // WITH_EDITOR
