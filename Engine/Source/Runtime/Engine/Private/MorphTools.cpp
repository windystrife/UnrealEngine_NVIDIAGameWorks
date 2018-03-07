// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphTools.cpp: Morph target creation helper classes.
=============================================================================*/ 

#include "CoreMinimal.h"
#include "RawIndexBuffer.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "SkeletalMeshTypes.h"

/** compare based on base mesh source vertex indices */
struct FCompareMorphTargetDeltas
{
	FORCEINLINE bool operator()( const FMorphTargetDelta& A, const FMorphTargetDelta& B ) const
	{
		return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0 ? true : false;
	}
};

FMorphTargetDelta* UMorphTarget::GetMorphTargetDelta(int32 LODIndex, int32& OutNumDeltas)
{
	if(LODIndex < MorphLODModels.Num())
	{
		FMorphTargetLODModel& MorphModel = MorphLODModels[LODIndex];
		OutNumDeltas = MorphModel.Vertices.Num();
		return MorphModel.Vertices.GetData();
	}

	return NULL;
}

bool UMorphTarget::HasDataForLOD(int32 LODIndex) 
{
	// If we have an entry for this LOD, and it has verts
	return (MorphLODModels.IsValidIndex(LODIndex) && MorphLODModels[LODIndex].Vertices.Num() > 0);
}

bool UMorphTarget::HasValidData() const
{
	for (const FMorphTargetLODModel& Model : MorphLODModels)
	{
		if (Model.Vertices.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void UMorphTarget::CreateMorphMeshStreams( const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex, bool bCompareNormal)
{
	check(BaseSource.IsValidTarget(TargetSource));

	// create the LOD entry if it doesn't already exist
	if( LODIndex >= MorphLODModels.Num() )
	{
		MorphLODModels.AddDefaulted(LODIndex - MorphLODModels.Num() + 1);
	}

	// morph mesh data to modify
	FMorphTargetLODModel& MorphModel = MorphLODModels[LODIndex];
	// copy the wedge point indices
	// for now just keep every thing 

	// set the original number of vertices
	MorphModel.NumBaseMeshVerts = BaseSource.Vertices.Num();

	// empty morph mesh vertices first
	MorphModel.Vertices.Empty();

	// array to mark processed base vertices
	TArray<bool> WasProcessed;
	WasProcessed.Empty(BaseSource.Vertices.Num());
	WasProcessed.AddZeroed(BaseSource.Vertices.Num());


	TMap<uint32,uint32> WedgePointToVertexIndexMap;
	// Build a mapping of wedge point indices to vertex indices for fast lookup later.
	for (int32 Idx = 0; Idx < TargetSource.WedgePointIndices.Num(); Idx++)
	{
		if (BaseSource.WedgePointIndices.IsValidIndex(Idx) && BaseSource.WedgePointIndices[Idx] == TargetSource.WedgePointIndices[Idx])
		{
			WedgePointToVertexIndexMap.Add(TargetSource.WedgePointIndices[Idx], Idx);
		}
	}

	// iterate over all the base mesh indices
	for( int32 Idx=0; Idx < BaseSource.Indices.Num(); Idx++ )
	{
		uint32 BaseVertIdx = BaseSource.Indices[Idx];

		// check for duplicate processing
		if( !WasProcessed[BaseVertIdx] )
		{
			// mark this base vertex as already processed
			WasProcessed[BaseVertIdx] = true;

			// get base mesh vertex using its index buffer
			const FMorphMeshVertexRaw& VBase = BaseSource.Vertices[BaseVertIdx];
			
			// clothing can add extra verts, and we won't have source point, so we ignore those
			if (BaseSource.WedgePointIndices.IsValidIndex(BaseVertIdx))
			{
				// get the base mesh's original wedge point index
				uint32 BasePointIdx = BaseSource.WedgePointIndices[BaseVertIdx];

				// find the matching target vertex by searching for one
				// that has the same wedge point index
				uint32* TargetVertIdx = WedgePointToVertexIndexMap.Find( BasePointIdx );

				// only add the vertex if the source point was found
				if( TargetVertIdx != NULL )
				{
					// get target mesh vertex using its index buffer
					const FMorphMeshVertexRaw& VTarget = TargetSource.Vertices[*TargetVertIdx];

					// change in position from base to target
					FVector PositionDelta( VTarget.Position - VBase.Position );
					FVector NormalDeltaZ (VTarget.TanZ - VBase.TanZ);

					// check if position actually changed much
					if( PositionDelta.SizeSquared() > FMath::Square(THRESH_POINTS_ARE_NEAR) || 
						// since we can't get imported morphtarget normal from FBX
						// we can't compare normal unless it's calculated
						// this is special flag to ignore normal diff
						( bCompareNormal && NormalDeltaZ.SizeSquared() > 0.01f) )
					{
						// create a new entry
						FMorphTargetDelta NewVertex;
						// position delta
						NewVertex.PositionDelta = PositionDelta;
						// normal delta
						NewVertex.TangentZDelta = NormalDeltaZ;
						// index of base mesh vert this entry is to modify
						NewVertex.SourceIdx = BaseVertIdx;

						// add it to the list of changed verts
						MorphModel.Vertices.Add( NewVertex );				
					}
				}	
			}
		}
	}

	// sort the array of vertices for this morph target based on the base mesh indices 
	// that each vertex is associated with. This allows us to sequentially traverse the list
	// when applying the morph blends to each vertex.
	MorphModel.Vertices.Sort(FCompareMorphTargetDeltas());

	// remove array slack
	MorphModel.Vertices.Shrink();
}

void UMorphTarget::PopulateDeltas(const TArray<FMorphTargetDelta>& Deltas, const int32 LODIndex, const bool bCompareNormal)
{
	// create the LOD entry if it doesn't already exist
	if (LODIndex >= MorphLODModels.Num())
	{
		MorphLODModels.AddDefaulted(LODIndex - MorphLODModels.Num() + 1);
	}

	// morph mesh data to modify
	FMorphTargetLODModel& MorphModel = MorphLODModels[LODIndex];
	// copy the wedge point indices
	// for now just keep every thing 

	// set the original number of vertices
	MorphModel.NumBaseMeshVerts = Deltas.Num();

	// empty morph mesh vertices first
	MorphModel.Vertices.Empty(Deltas.Num());

	// Still keep this (could remove in long term due to incoming data)
	for (const FMorphTargetDelta& Delta : Deltas)
	{
		if (Delta.PositionDelta.SizeSquared() > FMath::Square(THRESH_POINTS_ARE_NEAR) || 
			( bCompareNormal && Delta.TangentZDelta.SizeSquared() > 0.01f))
		{
			MorphModel.Vertices.Add(Delta);
		}
	}

	// sort the array of vertices for this morph target based on the base mesh indices
	// that each vertex is associated with. This allows us to sequentially traverse the list
	// when applying the morph blends to each vertex.
	MorphModel.Vertices.Sort(FCompareMorphTargetDeltas());

	// remove array slack
	MorphModel.Vertices.Shrink();
}


void UMorphTarget::RemapVertexIndices( USkeletalMesh* InBaseMesh, const TArray< TArray<uint32> > & BasedWedgePointIndices )
{
	// make sure base wedge point indices have more than what this morph target has
	// any morph target import needs base mesh (correct LOD index if it belongs to LOD)
	check ( BasedWedgePointIndices.Num() >= MorphLODModels.Num() );
	check ( InBaseMesh );

	// for each LOD
	FSkeletalMeshResource* ImportedResource = InBaseMesh->GetImportedResource();
	for ( int32 LODIndex=0; LODIndex<MorphLODModels.Num(); ++LODIndex )
	{
		FStaticLODModel & BaseLODModel = ImportedResource->LODModels[LODIndex];
		FMorphTargetLODModel& MorphLODModel = MorphLODModels[LODIndex];
		const TArray<uint32> & LODWedgePointIndices = BasedWedgePointIndices[LODIndex];
		TArray<uint32> NewWedgePointIndices;

		// If the LOD has been simplified, don't remap vertex indices else the data will be useless if the mesh is unsimplified.
		check( LODIndex < InBaseMesh->LODInfo.Num() );
		if ( InBaseMesh->LODInfo[ LODIndex ].bHasBeenSimplified  )
		{
			continue;
		}

		// copy the wedge point indices - it makes easy to find
		if( BaseLODModel.RawPointIndices.GetBulkDataSize() )
		{
			NewWedgePointIndices.Empty( BaseLODModel.RawPointIndices.GetElementCount() );
			NewWedgePointIndices.AddUninitialized( BaseLODModel.RawPointIndices.GetElementCount() );
			FMemory::Memcpy( NewWedgePointIndices.GetData(), BaseLODModel.RawPointIndices.Lock(LOCK_READ_ONLY), BaseLODModel.RawPointIndices.GetBulkDataSize() );
			BaseLODModel.RawPointIndices.Unlock();
		
			// Source Indices used : Save it so that you don't use it twice
			TArray<uint32> SourceIndicesUsed;
			SourceIndicesUsed.Empty(MorphLODModel.Vertices.Num());

			// go through all vertices
			for ( int32 VertIdx=0; VertIdx<MorphLODModel.Vertices.Num(); ++VertIdx )
			{	
				// Get Old Base Vertex ID
				uint32 OldVertIdx = MorphLODModel.Vertices[VertIdx].SourceIdx;
				// find PointIndices from the old list
				uint32 BasePointIndex = LODWedgePointIndices[OldVertIdx];

				// Find the PointIndices from new list
				int32 NewVertIdx = NewWedgePointIndices.Find(BasePointIndex);
				// See if it's already used
				if ( SourceIndicesUsed.Find(NewVertIdx) != INDEX_NONE )
				{
					// if used look for next right vertex index
					for ( int32 Iter = NewVertIdx + 1; Iter<NewWedgePointIndices.Num(); ++Iter )
					{
						// found one
						if (NewWedgePointIndices[Iter] == BasePointIndex)
						{
							// see if used again
							if (SourceIndicesUsed.Find(Iter) == INDEX_NONE)
							{
								// if not, this slot is available 
								// update new value
								MorphLODModel.Vertices[VertIdx].SourceIdx = Iter;
								SourceIndicesUsed.Add(Iter);									
								break;
							}
						}
					}
				}
				else
				{
					// update new value
					MorphLODModel.Vertices[VertIdx].SourceIdx = NewVertIdx;
					SourceIndicesUsed.Add(NewVertIdx);
				}
			}

			MorphLODModel.Vertices.Sort(FCompareMorphTargetDeltas());
		}
	}
}
/**
* Constructor. 
* Converts a skeletal mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source skeletal mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( USkeletalMesh* SrcMesh, int32 LODIndex ) 
{
	check(SrcMesh);
	FSkeletalMeshResource* ImportedResource = SrcMesh->GetImportedResource();
	check(ImportedResource->LODModels.IsValidIndex(LODIndex));

	// get the mesh data for the given lod
	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	Initialize(LODModel);
}

FMorphMeshRawSource::FMorphMeshRawSource(FStaticLODModel& LODModel)
{
	Initialize(LODModel);
}

void FMorphMeshRawSource::Initialize(FStaticLODModel& LODModel)
{
	// vertices are packed to stay consistent with the indexing used by the FStaticLODModel vertex buffer
	//
	//	Chunk0
	//		Soft0
	//		Soft1
	//	Chunk1
	//		Soft0
	//		Soft1

	// iterate over the chunks for the skeletal mesh
	for (int32 SectionIdx = 0; SectionIdx < LODModel.Sections.Num(); SectionIdx++)
	{
		const FSkelMeshSection& Section = LODModel.Sections[SectionIdx];
		for (int32 VertexIdx = 0; VertexIdx < Section.SoftVertices.Num(); VertexIdx++)
		{
			const FSoftSkinVertex& SourceVertex = Section.SoftVertices[VertexIdx];
			FMorphMeshVertexRaw RawVertex =
			{
				SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.Add(RawVertex);
		}
	}

	// Copy the indices manually, since the LODModel's index buffer may have a different alignment.
	Indices.Empty(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
	for (int32 Index = 0; Index < LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num(); Index++)
	{
		Indices.Add(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(Index));
	}

	// copy the wedge point indices
	if (LODModel.RawPointIndices.GetBulkDataSize())
	{
		WedgePointIndices.Empty(LODModel.RawPointIndices.GetElementCount());
		WedgePointIndices.AddUninitialized(LODModel.RawPointIndices.GetElementCount());
		FMemory::Memcpy(WedgePointIndices.GetData(), LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize());
		LODModel.RawPointIndices.Unlock();
	}
}

/**
* Constructor. 
* Converts a static mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source static mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( UStaticMesh* SrcMesh, int32 LODIndex ) 
{
	// @todo - not implemented
	// not sure if we will support static mesh morphing yet
}

/**
* Return true if current vertex data can be morphed to the target vertex data
* 
*/
bool FMorphMeshRawSource::IsValidTarget( const FMorphMeshRawSource& Target ) const
{
	//@todo sz -
	// heuristic is to check for the same number of original points
	//return( WedgePointIndices.Num() == Target.WedgePointIndices.Num() );
	return true;
}

