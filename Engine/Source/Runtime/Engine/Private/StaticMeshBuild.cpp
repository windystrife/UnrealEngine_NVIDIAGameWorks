// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshBuild.cpp: Static mesh building.
=============================================================================*/

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "Components/StaticMeshComponent.h"
#include "GenericOctreePublic.h"
#include "GenericOctree.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "MeshUtilities.h"
#include "Misc/FeedbackContext.h"
#include "Misc/App.h"
#endif // #if WITH_EDITOR

#define LOCTEXT_NAMESPACE "StaticMeshEditor"

#if WITH_EDITOR
/**
 * Check the render data for the provided mesh and return true if the mesh
 * contains degenerate tangent bases.
 */
static bool HasBadNTB(UStaticMesh* Mesh, bool &bZeroNormals, bool &bZeroTangents, bool &bZeroBinormals)
{
	bZeroTangents = false;
	bZeroNormals = false;
	bZeroBinormals = false;
	bool bBadTangents = false;
	if (Mesh && Mesh->RenderData)
	{
		int32 NumLODs = Mesh->GetNumLODs();
		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			FStaticMeshLODResources& LOD = Mesh->RenderData->LODResources[LODIndex];
			int32 NumVerts = LOD.VertexBuffer.GetNumVertices();
			for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
			{
				const FVector TangentX = LOD.VertexBuffer.VertexTangentX(VertIndex);
				const FVector TangentY = LOD.VertexBuffer.VertexTangentY(VertIndex);
				const FVector TangentZ = LOD.VertexBuffer.VertexTangentZ(VertIndex);
				
				if (TangentX.IsNearlyZero(KINDA_SMALL_NUMBER))
				{
					bZeroTangents = true;
				}
				if (TangentY.IsNearlyZero(KINDA_SMALL_NUMBER))
				{
					bZeroBinormals = true;
				}
				if (TangentZ.IsNearlyZero(KINDA_SMALL_NUMBER))
				{
					bZeroNormals = true;
				}
				if ((TangentX - TangentZ).IsNearlyZero(1.0f / 255.0f))
				{
					bBadTangents = true;
				}
			}
		}
	}
	return bBadTangents;
}
#endif // #if WITH_EDITOR

void UStaticMesh::Build(bool bSilent, TArray<FText>* OutErrors)
{
#if WITH_EDITOR
	if (IsTemplate())
		return;

	if (SourceModels.Num() <= 0)
	{
		UE_LOG(LogStaticMesh,Warning,TEXT("Static mesh has no source models: %s"),*GetPathName());
		return;
	}

	if (SourceModels.Num() > MAX_STATIC_MESH_LODS)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Cannot build LOD %d.  The maximum allowed is %d.  Skipping."), SourceModels.Num(), MAX_STATIC_MESH_LODS);
		return;
	}

	if(!bSilent)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("Path"), FText::FromString( GetPathName() ) );
		const FText StatusUpdate = FText::Format( LOCTEXT("BeginStaticMeshBuildingTask", "({Path}) Building"), Args );
		GWarn->BeginSlowTask( StatusUpdate, true );	
	}

	PreMeshBuild.Broadcast(this);

	// Detach all instances of this static mesh from the scene.
	FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(this,false);

	// Release the static mesh's resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	ReleaseResourcesFence.Wait();

	// Remember the derived data key of our current render data if any.
	FString ExistingDerivedDataKey = RenderData ? RenderData->DerivedDataKey : TEXT("");

	// Regenerating UVs for lightmaps, use the latest version
	LightmapUVVersion = (int32)ELightmapUVVersion::Latest;

	// Free existing render data and recache.
	CacheDerivedData();

	// Note: meshes can be built during automated importing.  We should not create resources in that case
	// as they will never be released when this object is deleted
	if(FApp::CanEverRender())
	{
		// Reinitialize the static mesh's resources.
		InitResources();
	}

	// Ensure we have a bodysetup.
	CreateBodySetup();
	check(BodySetup != NULL);

	if( SourceModels.Num() )
	{
		// Rescale simple collision if the user changed the mesh build scale
		BodySetup->RescaleSimpleCollision( SourceModels[0].BuildSettings.BuildScale3D );
	}

	// Invalidate physics data if this has changed.
	// TODO_STATICMESH: Not necessary any longer?
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();

	// Compare the derived data keys to see if renderable mesh data has actually changed.
	check(RenderData);
	bool bHasRenderDataChanged = RenderData->DerivedDataKey != ExistingDerivedDataKey;

	if (bHasRenderDataChanged)
	{
		// Warn the user if the new mesh has degenerate tangent bases.
		bool bZeroNormals, bZeroTangents, bZeroBinormals;
		if (HasBadNTB(this, bZeroNormals, bZeroTangents, bZeroBinormals))
		{
			bool bIsUsingMikktSpace = SourceModels[0].BuildSettings.bUseMikkTSpace && (SourceModels[0].BuildSettings.bRecomputeTangents || SourceModels[0].BuildSettings.bRecomputeNormals);
			// Only suggest Recompute Tangents if the import hasn't already tried it
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Meshname"), FText::FromString(GetName()));
			Arguments.Add(TEXT("Options"), SourceModels[0].BuildSettings.bRecomputeTangents ? FText::GetEmpty() : LOCTEXT("MeshRecomputeTangents", "Consider enabling Recompute Tangents in the mesh's Build Settings."));
			Arguments.Add(TEXT("MikkTSpace"), bIsUsingMikktSpace ? LOCTEXT("MeshUseMikkTSpace", "MikkTSpace relies on tangent bases and may result in mesh corruption, consider disabling this option.") : FText::GetEmpty());
			const FText WarningMsg = FText::Format(LOCTEXT("MeshHasDegenerateTangents", "{Meshname} has degenerate tangent bases which will result in incorrect shading. {Options} {MikkTSpace}"), Arguments);
			UE_LOG(LogStaticMesh, Warning, TEXT("%s"), *WarningMsg.ToString());
			if (!bSilent && OutErrors)
			{
				OutErrors->Add(WarningMsg);
			}
		}
		
		FText ToleranceArgument = FText::FromString(TEXT("1E-4"));
		if (bZeroNormals)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Meshname"), FText::FromString(GetName()));
			Arguments.Add(TEXT("Tolerance"), ToleranceArgument);
			const FText WarningMsg = FText::Format(LOCTEXT("MeshHasSomeZeroNormals", "{Meshname} has some nearly zero normals which can create some issues. (Tolerance of {Tolerance})"), Arguments);
			UE_LOG(LogStaticMesh, Warning, TEXT("%s"), *WarningMsg.ToString());
			if (!bSilent && OutErrors)
			{
				OutErrors->Add(WarningMsg);
			}
		}

		if (bZeroTangents)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Meshname"), FText::FromString(GetName()));
			Arguments.Add(TEXT("Tolerance"), ToleranceArgument);
			const FText WarningMsg = FText::Format(LOCTEXT("MeshHasSomeZeroTangents", "{Meshname} has some nearly zero tangents which can create some issues. (Tolerance of {Tolerance})"), Arguments);
			UE_LOG(LogStaticMesh, Warning, TEXT("%s"), *WarningMsg.ToString());
			if (!bSilent && OutErrors)
			{
				OutErrors->Add(WarningMsg);
			}
		}

		if (bZeroBinormals)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Meshname"), FText::FromString(GetName()));
			Arguments.Add(TEXT("Tolerance"), ToleranceArgument);
			const FText WarningMsg = FText::Format(LOCTEXT("MeshHasSomeZeroBiNormals", "{Meshname} has some nearly zero bi-normals which can create some issues. (Tolerance of {Tolerance})"), Arguments);
			UE_LOG(LogStaticMesh, Warning, TEXT("%s"), *WarningMsg.ToString());
			if (!bSilent && OutErrors)
			{
				OutErrors->Add(WarningMsg);
			}
		}

		// Force the static mesh to re-export next time lighting is built
		SetLightingGuid();

		// Find any static mesh components that use this mesh and fixup their override colors if necessary.
		// Also invalidate lighting. *** WARNING components may be reattached here! ***
		const uint32 NumLODs = RenderData->LODResources.Num();
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if ( It->GetStaticMesh() == this )
			{
				It->FixupOverrideColorsIfNecessary(true);
				It->InvalidateLightingCache();
			}
		}
	}

	// Calculate extended bounds
	CalculateExtendedBounds();

	// Update nav collision 
	CreateNavCollision(/*bIsUpdate=*/true);

	PostMeshBuild.Broadcast(this);

	if(!bSilent)
	{
		GWarn->EndSlowTask();
	}

#else
	UE_LOG(LogStaticMesh,Fatal,TEXT("UStaticMesh::Build should not be called on non-editor builds."));
#endif
}

/*------------------------------------------------------------------------------
	Remapping of painted vertex colors.
------------------------------------------------------------------------------*/

#if WITH_EDITOR
/** Helper struct for the mesh component vert position octree */
struct FStaticMeshComponentVertPosOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	/**
	 * Get the bounding box of the provided octree element. In this case, the box
	 * is merely the point specified by the element.
	 *
	 * @param	Element	Octree element to get the bounding box for
	 *
	 * @return	Bounding box of the provided octree element
	 */
	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox( const FPaintedVertex& Element )
	{
		return FBoxCenterAndExtent( Element.Position, FVector::ZeroVector );
	}

	/**
	 * Determine if two octree elements are equal
	 *
	 * @param	A	First octree element to check
	 * @param	B	Second octree element to check
	 *
	 * @return	true if both octree elements are equal, false if they are not
	 */
	FORCEINLINE static bool AreElementsEqual( const FPaintedVertex& A, const FPaintedVertex& B )
	{
		return ( A.Position == B.Position && A.Normal == B.Normal && A.Color == B.Color );
	}

	/** Ignored for this implementation */
	FORCEINLINE static void SetElementId( const FPaintedVertex& Element, FOctreeElementId Id )
	{
	}
};
typedef TOctree<FPaintedVertex, FStaticMeshComponentVertPosOctreeSemantics> TSMCVertPosOctree;

void RemapPaintedVertexColors(const TArray<FPaintedVertex>& InPaintedVertices,
	const FColorVertexBuffer& InOverrideColors,
	const FPositionVertexBuffer& OldPositions,
	const FStaticMeshVertexBuffer& OldVertexBuffer,
	const FPositionVertexBuffer& NewPositions,
	const FStaticMeshVertexBuffer* OptionalVertexBuffer,
	TArray<FColor>& OutOverrideColors)
{
	// Find the extents formed by the cached vertex positions in order to optimize the octree used later
	FVector MinExtents(EForceInit::ForceInitToZero);
	FVector MaxExtents(EForceInit::ForceInitToZero);
	
	TArray<FPaintedVertex> PaintedVertices;
	FBox Bounds(EForceInit::ForceInitToZero);

	// Retrieve currently painted vertices
	if (InPaintedVertices.Num() > 0)
	{
		// In case we have retained the painted vertices we can just append
		PaintedVertices.Append(InPaintedVertices);
		
		for (const FPaintedVertex& Vertex : InPaintedVertices)
		{
			Bounds += Vertex.Position;
		}
	}
	else
	{
		// Otherwise we have to retrieve the data from the override color and vertex buffers
		TArray<FColor> Colors;
		InOverrideColors.GetVertexColors(Colors);

		PaintedVertices.Reset(Colors.Num());
		FPaintedVertex PaintedVertex;
		for (int32 Index = 0; Index < Colors.Num(); ++Index)
		{
			PaintedVertex.Color = Colors[Index];
			PaintedVertex.Normal = OldVertexBuffer.VertexTangentZ(Index);
			PaintedVertex.Position = OldPositions.VertexPosition(Index);
			Bounds += PaintedVertex.Position;

			PaintedVertices.Add(PaintedVertex);
		}
	}

	// Create an octree which spans the extreme extents of the old and new vertex positions in order to quickly query for the colors
	// of the new vertex positions
	for (int32 VertIndex = 0; VertIndex < (int32)NewPositions.GetNumVertices(); ++VertIndex)
	{
		Bounds += NewPositions.VertexPosition(VertIndex);
	}

	TSMCVertPosOctree VertPosOctree( Bounds.GetCenter(), Bounds.GetExtent().GetMax() );

	// Add each old vertex to the octree
	for ( int32 PaintedVertexIndex = 0; PaintedVertexIndex < PaintedVertices.Num(); ++PaintedVertexIndex )
	{
		VertPosOctree.AddElement( PaintedVertices[ PaintedVertexIndex ] );
	}

	// Iterate over each new vertex position, attempting to find the old vertex it is closest to, applying
	// the color of the old vertex to the new position if possible.
	OutOverrideColors.Empty(NewPositions.GetNumVertices());
	const float DistanceOverNormalThreshold = OptionalVertexBuffer ? KINDA_SMALL_NUMBER : 0.0f;
	for ( uint32 NewVertIndex = 0; NewVertIndex < NewPositions.GetNumVertices(); ++NewVertIndex )
	{
		TArray<FPaintedVertex> PointsToConsider;
		TSMCVertPosOctree::TConstIterator<> OctreeIter( VertPosOctree );
		const FVector& CurPosition = NewPositions.VertexPosition( NewVertIndex );
		FVector CurNormal = FVector::ZeroVector;
		if (OptionalVertexBuffer)
		{
			CurNormal = OptionalVertexBuffer->VertexTangentZ( NewVertIndex );
		}

		// Iterate through the octree attempting to find the vertices closest to the current new point
		while ( OctreeIter.HasPendingNodes() )
		{
			const TSMCVertPosOctree::FNode& CurNode = OctreeIter.GetCurrentNode();
			const FOctreeNodeContext& CurContext = OctreeIter.GetCurrentContext();

			// Find the child of the current node, if any, that contains the current new point
			FOctreeChildNodeRef ChildRef = CurContext.GetContainingChild( FBoxCenterAndExtent( CurPosition, FVector::ZeroVector ) );

			if ( !ChildRef.IsNULL() )
			{
				const TSMCVertPosOctree::FNode* ChildNode = CurNode.GetChild( ChildRef );

				// If the specified child node exists and contains any of the old vertices, push it to the iterator for future consideration
				if ( ChildNode && ChildNode->GetInclusiveElementCount() > 0 )
				{
					OctreeIter.PushChild( ChildRef );
				}
				// If the child node doesn't have any of the old vertices in it, it's not worth pursuing any further. In an attempt to find
				// anything to match vs. the new point, add all of the children of the current octree node that have old points in them to the
				// iterator for future consideration.
				else
				{
					FOREACH_OCTREE_CHILD_NODE( OctreeChildRef )
					{
						if( CurNode.HasChild( OctreeChildRef ) )
						{
							OctreeIter.PushChild( OctreeChildRef );
						}
					}
				}
			}

			// Add all of the elements in the current node to the list of points to consider for closest point calculations
			PointsToConsider.Append( CurNode.GetElements() );
			OctreeIter.Advance();
		}

		// If any points to consider were found, iterate over each and find which one is the closest to the new point 
		if ( PointsToConsider.Num() > 0 )
		{
			FPaintedVertex BestVertex = PointsToConsider[0];
			FVector BestVertexNormal = BestVertex.Normal;

			float BestDistanceSquared = ( BestVertex.Position - CurPosition ).SizeSquared();
			float BestNormalDot = BestVertexNormal | CurNormal;

			for ( int32 ConsiderationIndex = 1; ConsiderationIndex < PointsToConsider.Num(); ++ConsiderationIndex )
			{
				FPaintedVertex& Vertex = PointsToConsider[ ConsiderationIndex ];
				FVector VertexNormal = Vertex.Normal;

				const float DistSqrd = ( Vertex.Position - CurPosition ).SizeSquared();
				const float NormalDot = VertexNormal | CurNormal;
				if ( DistSqrd < BestDistanceSquared - DistanceOverNormalThreshold )
				{
					BestVertex = Vertex;
					BestDistanceSquared = DistSqrd;
					BestNormalDot = NormalDot;
				}
				else if ( OptionalVertexBuffer && DistSqrd < BestDistanceSquared + DistanceOverNormalThreshold && NormalDot > BestNormalDot )
				{
					BestVertex = Vertex;
					BestDistanceSquared = DistSqrd;
					BestNormalDot = NormalDot;
				}
			}

			OutOverrideColors.Add(BestVertex.Color);
		}
	}
}
#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Conversion of legacy source data.
------------------------------------------------------------------------------*/

#if WITH_EDITOR

struct FStaticMeshTriangle
{
	FVector		Vertices[3];
	FVector2D	UVs[3][8];
	FColor		Colors[3];
	int32			MaterialIndex;
	int32			FragmentIndex;
	uint32		SmoothingMask;
	int32			NumUVs;

	FVector		TangentX[3]; // Tangent, U-direction
	FVector		TangentY[3]; // Binormal, V-direction
	FVector		TangentZ[3]; // Normal

	uint32		bOverrideTangentBasis;
	uint32		bExplicitNormals;
};

struct FStaticMeshTriangleBulkData : public FUntypedBulkData
{
	virtual int32 GetElementSize() const
	{
		return sizeof(FStaticMeshTriangle);
	}

	virtual void SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
	{
		FStaticMeshTriangle& StaticMeshTriangle = *((FStaticMeshTriangle*)Data + ElementIndex);
		Ar << StaticMeshTriangle.Vertices[0];
		Ar << StaticMeshTriangle.Vertices[1];
		Ar << StaticMeshTriangle.Vertices[2];
		for( int32 VertexIndex=0; VertexIndex<3; VertexIndex++ )
		{
			for( int32 UVIndex=0; UVIndex<8; UVIndex++ )
			{
				Ar << StaticMeshTriangle.UVs[VertexIndex][UVIndex];
			}
        }
		Ar << StaticMeshTriangle.Colors[0];
		Ar << StaticMeshTriangle.Colors[1];
		Ar << StaticMeshTriangle.Colors[2];
		Ar << StaticMeshTriangle.MaterialIndex;
		Ar << StaticMeshTriangle.FragmentIndex;
		Ar << StaticMeshTriangle.SmoothingMask;
		Ar << StaticMeshTriangle.NumUVs;
		Ar << StaticMeshTriangle.TangentX[0];
		Ar << StaticMeshTriangle.TangentX[1];
		Ar << StaticMeshTriangle.TangentX[2];
		Ar << StaticMeshTriangle.TangentY[0];
		Ar << StaticMeshTriangle.TangentY[1];
		Ar << StaticMeshTriangle.TangentY[2];
		Ar << StaticMeshTriangle.TangentZ[0];
		Ar << StaticMeshTriangle.TangentZ[1];
		Ar << StaticMeshTriangle.TangentZ[2];
		Ar << StaticMeshTriangle.bOverrideTangentBasis;
		Ar << StaticMeshTriangle.bExplicitNormals;
	}

	virtual bool RequiresSingleElementSerialization( FArchive& Ar )
	{
		return false;
	}
};

struct FFragmentRange
{
	int32 BaseIndex;
	int32 NumPrimitives;

	friend FArchive& operator<<(FArchive& Ar,FFragmentRange& FragmentRange)
	{
		Ar << FragmentRange.BaseIndex << FragmentRange.NumPrimitives;
		return Ar;
	}
};

void UStaticMesh::FixupZeroTriangleSections()
{
	if (RenderData->MaterialIndexToImportIndex.Num() > 0 && RenderData->LODResources.Num())
	{
		TArray<int32> MaterialMap;
		FMeshSectionInfoMap NewSectionInfoMap;

		// Iterate over all sections of all LODs and identify all material indices that need to be remapped.
		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++ LODIndex)
		{
			FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
			int32 NumSections = LOD.Sections.Num();

			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				FMeshSectionInfo DefaultSectionInfo(SectionIndex);
				if (RenderData->MaterialIndexToImportIndex.IsValidIndex(SectionIndex))
				{
					int32 ImportIndex = RenderData->MaterialIndexToImportIndex[SectionIndex];
					FMeshSectionInfo SectionInfo = SectionInfoMap.Get(LODIndex, ImportIndex);
					int32 OriginalMaterialIndex = SectionInfo.MaterialIndex;

					// If import index == material index, remap it.
					if (SectionInfo.MaterialIndex == ImportIndex)
					{
						SectionInfo.MaterialIndex = SectionIndex;
					}

					// Update the material mapping table.
					while (SectionInfo.MaterialIndex >= MaterialMap.Num())
					{
						MaterialMap.Add(INDEX_NONE);
					}
					if (SectionInfo.MaterialIndex >= 0)
					{
						MaterialMap[SectionInfo.MaterialIndex] = OriginalMaterialIndex;
					}

					// Update the new section info map if needed.
					if (SectionInfo != DefaultSectionInfo)
					{
						NewSectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);
					}
				}
			}
		}

		// Compact the materials array.
		for (int32 i = RenderData->LODResources[0].Sections.Num(); i < MaterialMap.Num(); ++i)
		{
			if (MaterialMap[i] == INDEX_NONE)
			{
				int32 NextValidIndex = i+1;
				for (; NextValidIndex < MaterialMap.Num(); ++NextValidIndex)
				{
					if (MaterialMap[NextValidIndex] != INDEX_NONE)
					{
						break;
					}
				}
				if (MaterialMap.IsValidIndex(NextValidIndex))
				{
					MaterialMap[i] = MaterialMap[NextValidIndex];
					for (TMap<uint32,FMeshSectionInfo>::TIterator It(NewSectionInfoMap.Map); It; ++It)
					{
						FMeshSectionInfo& SectionInfo = It.Value();
						if (SectionInfo.MaterialIndex == NextValidIndex)
						{
							SectionInfo.MaterialIndex = i;
						}
					}
				}
				MaterialMap.RemoveAt(i, NextValidIndex - i);
			}
		}

		SectionInfoMap.Clear();
		SectionInfoMap.CopyFrom(NewSectionInfoMap);

		// Check if we need to remap materials.
		bool bRemapMaterials = false;
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialMap.Num(); ++MaterialIndex)
		{
			if (MaterialMap[MaterialIndex] != MaterialIndex)
			{
				bRemapMaterials = true;
				break;
			}
		}

		// Remap the materials array if needed.
		if (bRemapMaterials)
		{
			TArray<FStaticMaterial> OldMaterials;
			Exchange(StaticMaterials,OldMaterials);
			StaticMaterials.Empty(MaterialMap.Num());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialMap.Num(); ++MaterialIndex)
			{
				FStaticMaterial StaticMaterial;
				int32 OldMaterialIndex = MaterialMap[MaterialIndex];
				if (OldMaterials.IsValidIndex(OldMaterialIndex))
				{
					StaticMaterial = OldMaterials[OldMaterialIndex];
				}
				StaticMaterials.Add(StaticMaterial);
			}
		}
	}
	else
	{
		int32 FoundMaxMaterialIndex = -1;
		TSet<int32> DiscoveredMaterialIndices;
		
		// Find the maximum material index that is used by the mesh
		// Also keep track of which materials are actually used in the array
		for(int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++LODIndex)
		{
			if (RenderData->LODResources.IsValidIndex(LODIndex))
			{
				FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
				int32 NumSections = LOD.Sections.Num();
				for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
				{
					FMeshSectionInfo Info = SectionInfoMap.Get(LODIndex, SectionIndex);
					if(Info.MaterialIndex > FoundMaxMaterialIndex)
					{
						FoundMaxMaterialIndex = Info.MaterialIndex;
					}

					DiscoveredMaterialIndices.Add(Info.MaterialIndex);
				}
			}
		}

		// NULL references to materials in indices that are not used by any LOD.
		// This is to fix up an import bug which caused more materials to be added to this array than needed.
		for ( int32 MaterialIdx = 0; MaterialIdx < StaticMaterials.Num(); ++MaterialIdx )
		{
			if ( !DiscoveredMaterialIndices.Contains(MaterialIdx) )
			{
				// Materials that are not used by any LOD resource should not be in this array.
				StaticMaterials[MaterialIdx].MaterialInterface = nullptr;
			}
		}

		// Remove entries at the end of the materials array.
		if (StaticMaterials.Num() > (FoundMaxMaterialIndex + 1))
		{
			StaticMaterials.RemoveAt(FoundMaxMaterialIndex+1, StaticMaterials.Num() - FoundMaxMaterialIndex - 1);
		}
	}
}

#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE
