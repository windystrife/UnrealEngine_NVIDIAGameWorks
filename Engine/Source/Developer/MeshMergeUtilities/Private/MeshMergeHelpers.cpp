// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeHelpers.h"

#include "Engine/MapBuildDataRegistry.h"
#include "Engine/MeshMerging.h"

#include "MaterialOptions.h"
#include "RawMesh.h"

#include "Misc/PackageName.h"
#include "MaterialUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"

#include "SkeletalMeshTypes.h"
#include "SkeletalRenderPublic.h"

#include "UObject/UObjectBaseUtility.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "MeshMergeData.h"
#include "IHierarchicalLODUtilities.h"
#include "Engine/MeshMergeCullingVolume.h"

#include "Landscape.h"
#include "LandscapeProxy.h"

#include "Editor.h"

#include "Engine/StaticMesh.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshUtilities.h"
#include "ImageUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "IMeshReductionManagerModule.h"
#include "LayoutUV.h"

//DECLARE_LOG_CATEGORY_CLASS(LogMeshMerging, Verbose, All);

void FMeshMergeHelpers::ExtractSections(const UStaticMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	TArray<FName> MaterialSlotNames = Component->GetMaterialSlotNames();

	const UStaticMesh* StaticMesh = Component->GetStaticMesh();
	for (const FStaticMeshSection& MeshSection : StaticMesh->RenderData->LODResources[LODIndex].Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = Component->GetMaterial(MeshSection.MaterialIndex);

		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		// Populate section data
		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialIndex = MeshSection.MaterialIndex;
		SectionInfo.MaterialSlotName = MaterialSlotNames.IsValidIndex(MeshSection.MaterialIndex) ? MaterialSlotNames[MeshSection.MaterialIndex] : NAME_None;
		SectionInfo.StartIndex = MeshSection.FirstIndex / 3;
		SectionInfo.EndIndex = SectionInfo.StartIndex + MeshSection.NumTriangles;

		if (MeshSection.bEnableCollision)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bEnableCollision));
		}

		if (MeshSection.bCastShadow && Component->CastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bCastShadow));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::ExtractSections(const USkeletalMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FSkeletalMeshResource* Resource = Component->GetSkeletalMeshResource();

	checkf(Resource->LODModels.IsValidIndex(LODIndex), TEXT("Invalid LOD Index"));

	TArray<FName> MaterialSlotNames = Component->GetMaterialSlotNames();

	const FStaticLODModel& Model = Resource->LODModels[LODIndex];
	for (const FSkelMeshSection& MeshSection : Model.Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = Component->GetMaterial(MeshSection.MaterialIndex);
		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialSlotName = MaterialSlotNames.IsValidIndex(MeshSection.MaterialIndex) ? MaterialSlotNames[MeshSection.MaterialIndex] : NAME_None;

		if (MeshSection.bCastShadow && Component->CastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FSkelMeshSection, bCastShadow));
		}

		if (MeshSection.bDisabled)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FSkelMeshSection, bDisabled));
		}

		if (MeshSection.bRecomputeTangent)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FSkelMeshSection, bRecomputeTangent));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::ExtractSections(const UStaticMesh* StaticMesh, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	for (const FStaticMeshSection& MeshSection : StaticMesh->RenderData->LODResources[LODIndex].Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = StaticMesh->GetMaterial(MeshSection.MaterialIndex);

		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		// Populate section data
		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialIndex = MeshSection.MaterialIndex;
		SectionInfo.MaterialSlotName = StaticMesh->StaticMaterials.IsValidIndex(MeshSection.MaterialIndex) ? StaticMesh->StaticMaterials[MeshSection.MaterialIndex].MaterialSlotName : NAME_None;

		if (MeshSection.bEnableCollision)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bEnableCollision));
		}

		if (MeshSection.bCastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bCastShadow));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::RetrieveMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh, bool bPropagateVertexColours)
{
	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->SourceModels[LODIndex];

	const bool bIsSplineMeshComponent = StaticMeshComponent->IsA<USplineMeshComponent>();

	// Imported meshes will have a filled RawMeshBulkData set
	const bool bImportedMesh = !StaticMeshModel.RawMeshBulkData->IsEmpty();
		
	// Export the raw mesh data using static mesh render data
	ExportStaticMeshLOD(StaticMesh->RenderData->LODResources[LODIndex], RawMesh);	

	// Make sure the raw mesh is not irreparably malformed.
	if (!RawMesh.IsValid())
	{
		return;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->SourceModels[0].BuildSettings;

	// Transform raw mesh to world space
	FTransform ComponentToWorldTransform = StaticMeshComponent->GetComponentTransform();

	// Handle spline mesh deformation
	if (bIsSplineMeshComponent)
	{
		const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(StaticMeshComponent);
		// Deform raw mesh data according to the Spline Mesh Component's data
		PropagateSplineDeformationToRawMesh(SplineMeshComponent, RawMesh);
	}

	// If specified propagate painted vertex colors into our raw mesh
	if (bPropagateVertexColours)
	{
		PropagatePaintedColorsToRawMesh(StaticMeshComponent, LODIndex, RawMesh);
	}

	// Transform raw mesh vertex data by the Static Mesh Component's component to world transformation	
	TransformRawMeshVertexData(ComponentToWorldTransform, RawMesh);

	if (!RawMesh.IsValid())
	{
		return;
	}

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals	
	const bool bRecomputeNormals = RawMesh.WedgeTangentZ.Num() == 0;
	const bool bRecomputeTangents = RawMesh.WedgeTangentX.Num() == 0 || RawMesh.WedgeTangentY.Num() == 0;

	if (bRecomputeNormals || bRecomputeTangents)
	{
		IMeshUtilities& Utilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		Utilities.RecomputeTangentsAndNormalsForRawMesh(bRecomputeTangents, bRecomputeNormals, BuildSettings, RawMesh);
	}
}

void FMeshMergeHelpers::RetrieveMesh(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex, FRawMesh& RawMesh, bool bPropagateVertexColours)
{
	FSkeletalMeshResource* Resource = SkeletalMeshComponent->SkeletalMesh->GetResourceForRendering();
	if (Resource->LODModels.IsValidIndex(LODIndex))
	{
		FSkeletalMeshLODInfo& SrcLODInfo = SkeletalMeshComponent->SkeletalMesh->LODInfo[LODIndex];

		// Get the CPU skinned verts for this LOD
		TArray<FFinalSkinVertex> FinalVertices;
		SkeletalMeshComponent->GetCPUSkinnedVertices(FinalVertices, LODIndex);

		FSkeletalMeshResource& SkeletalMeshResource = SkeletalMeshComponent->MeshObject->GetSkeletalMeshResource();
		FStaticLODModel& StaticLODModel = SkeletalMeshResource.LODModels[LODIndex];

		// Copy skinned vertex positions
		for (int32 VertIndex = 0; VertIndex < FinalVertices.Num(); ++VertIndex)
		{
			RawMesh.VertexPositions.Add(FinalVertices[VertIndex].Position);
		}

		const uint32 NumTexCoords = FMath::Min(StaticLODModel.VertexBufferGPUSkin.GetNumTexCoords(), (uint32)MAX_MESH_TEXTURE_COORDS);
		const int32 NumSections = StaticLODModel.Sections.Num();
		FRawStaticIndexBuffer16or32Interface& IndexBuffer = *StaticLODModel.MultiSizeIndexContainer.GetIndexBuffer();

		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			const FSkelMeshSection& SkelMeshSection = StaticLODModel.Sections[SectionIndex];
			if (!SkelMeshSection.bDisabled)
			{
				// Build 'wedge' info
				const int32 NumWedges = SkelMeshSection.NumTriangles * 3;
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
				{
					const int32 VertexIndexForWedge = IndexBuffer.Get(SkelMeshSection.BaseIndex + WedgeIndex);

					RawMesh.WedgeIndices.Add(VertexIndexForWedge);

					const FFinalSkinVertex& SkinnedVertex = FinalVertices[VertexIndexForWedge];
					const FVector TangentX = SkinnedVertex.TangentX;
					const FVector TangentZ = SkinnedVertex.TangentZ;
					const FVector4 UnpackedTangentZ = SkinnedVertex.TangentZ;
					const FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal() * UnpackedTangentZ.W;

					RawMesh.WedgeTangentX.Add(TangentX);
					RawMesh.WedgeTangentY.Add(TangentY);
					RawMesh.WedgeTangentZ.Add(TangentZ);

					for (uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++)
					{
						if (TexCoordIndex >= NumTexCoords)
						{
							RawMesh.WedgeTexCoords[TexCoordIndex].AddDefaulted();
						}
						else
						{
							RawMesh.WedgeTexCoords[TexCoordIndex].Add(StaticLODModel.VertexBufferGPUSkin.GetVertexUV(VertexIndexForWedge, TexCoordIndex));
						}
					}

					if (StaticLODModel.ColorVertexBuffer.IsInitialized() && bPropagateVertexColours)
					{
						RawMesh.WedgeColors.Add(StaticLODModel.ColorVertexBuffer.VertexColor(VertexIndexForWedge));
					}
					else
					{
						RawMesh.WedgeColors.Add(FColor::White);
					}
				}

				int32 MaterialIndex = SkelMeshSection.MaterialIndex;
				// use the remapping of material indices for all LODs besides the base LOD 
				if (LODIndex > 0 && SrcLODInfo.LODMaterialMap.IsValidIndex(SkelMeshSection.MaterialIndex))
				{
					MaterialIndex = FMath::Clamp<int32>(SrcLODInfo.LODMaterialMap[SkelMeshSection.MaterialIndex], 0, SkeletalMeshComponent->SkeletalMesh->Materials.Num());
				}

				// copy face info
				for (uint32 TriIndex = 0; TriIndex < SkelMeshSection.NumTriangles; TriIndex++)
				{
					RawMesh.FaceMaterialIndices.Add(MaterialIndex);
					RawMesh.FaceSmoothingMasks.Add(0); // Assume this is ignored as bRecomputeNormals is false
				}
			}
		}
	}
}

void FMeshMergeHelpers::RetrieveMesh(const UStaticMesh* StaticMesh, int32 LODIndex, FRawMesh& RawMesh)
{
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->SourceModels[LODIndex];

	// Imported meshes will have a filled RawMeshBulkData set
	const bool bImportedMesh = !StaticMeshModel.RawMeshBulkData->IsEmpty();
	// Check whether or not this mesh has been reduced in-engine
	const bool bReducedMesh = (StaticMeshModel.ReductionSettings.PercentTriangles < 1.0f);
	// Trying to retrieve rawmesh from SourceStaticMeshModel was giving issues, which causes a mismatch			
	const bool bRenderDataMismatch = (LODIndex > 0) || StaticMeshModel.BuildSettings.bGenerateLightmapUVs;

	if (bImportedMesh && !bReducedMesh && !bRenderDataMismatch)
	{
		StaticMeshModel.RawMeshBulkData->LoadRawMesh(RawMesh);
	}
	else
	{
		ExportStaticMeshLOD(StaticMesh->RenderData->LODResources[LODIndex], RawMesh);
	}

	// Make sure the raw mesh is not irreparably malformed.
	if (!RawMesh.IsValid())
	{
		// wrong
		bool check = true;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->SourceModels[0].BuildSettings;

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals
	const bool bRecomputeNormals = (bImportedMesh && BuildSettings.bRecomputeNormals) || RawMesh.WedgeTangentZ.Num() == 0;
	const bool bRecomputeTangents = (bImportedMesh && BuildSettings.bRecomputeTangents) || RawMesh.WedgeTangentX.Num() == 0 || RawMesh.WedgeTangentY.Num() == 0;

	if (bRecomputeNormals || bRecomputeTangents)
	{
		IMeshUtilities& Utilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		Utilities.RecomputeTangentsAndNormalsForRawMesh(bRecomputeTangents, bRecomputeNormals, BuildSettings, RawMesh);
	}
}

void FMeshMergeHelpers::ExportStaticMeshLOD(const FStaticMeshLODResources& StaticMeshLOD, FRawMesh& OutRawMesh)
{
	const int32 NumWedges = StaticMeshLOD.IndexBuffer.GetNumIndices();
	const int32 NumVertexPositions = StaticMeshLOD.PositionVertexBuffer.GetNumVertices();
	const int32 NumFaces = NumWedges / 3;

	// Indices
	StaticMeshLOD.IndexBuffer.GetCopy(OutRawMesh.WedgeIndices);

	// Vertex positions
	if (NumVertexPositions > 0)
	{
		OutRawMesh.VertexPositions.Empty(NumVertexPositions);
		for (int32 PosIdx = 0; PosIdx < NumVertexPositions; ++PosIdx)
		{
			FVector Pos = StaticMeshLOD.PositionVertexBuffer.VertexPosition(PosIdx);
			OutRawMesh.VertexPositions.Add(Pos);
		}
	}

	// Vertex data
	if (StaticMeshLOD.VertexBuffer.GetNumVertices() > 0)
	{
		OutRawMesh.WedgeTangentX.Empty(NumWedges);
		OutRawMesh.WedgeTangentY.Empty(NumWedges);
		OutRawMesh.WedgeTangentZ.Empty(NumWedges);

		const int32 NumTexCoords = StaticMeshLOD.VertexBuffer.GetNumTexCoords();
		for (int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx)
		{
			OutRawMesh.WedgeTexCoords[TexCoodIdx].Empty(NumWedges);
		}

		for (int32 WedgeIndex : OutRawMesh.WedgeIndices)
		{
			FVector WedgeTangentX = StaticMeshLOD.VertexBuffer.VertexTangentX(WedgeIndex);
			FVector WedgeTangentY = StaticMeshLOD.VertexBuffer.VertexTangentY(WedgeIndex);
			FVector WedgeTangentZ = StaticMeshLOD.VertexBuffer.VertexTangentZ(WedgeIndex);
			OutRawMesh.WedgeTangentX.Add(WedgeTangentX);
			OutRawMesh.WedgeTangentY.Add(WedgeTangentY);
			OutRawMesh.WedgeTangentZ.Add(WedgeTangentZ);

			for (int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx)
			{
				FVector2D WedgeTexCoord = StaticMeshLOD.VertexBuffer.GetVertexUV(WedgeIndex, TexCoodIdx);
				OutRawMesh.WedgeTexCoords[TexCoodIdx].Add(WedgeTexCoord);
			}
		}
	}

	// Vertex colors
	if (StaticMeshLOD.ColorVertexBuffer.GetNumVertices() > 0)
	{
		OutRawMesh.WedgeColors.Empty(NumWedges);
		for (int32 WedgeIndex : OutRawMesh.WedgeIndices)
		{
			FColor VertexColor = StaticMeshLOD.ColorVertexBuffer.VertexColor(WedgeIndex);
			OutRawMesh.WedgeColors.Add(VertexColor);
		}
	}

	// Materials
	{
		OutRawMesh.FaceMaterialIndices.Empty(NumFaces);
		OutRawMesh.FaceMaterialIndices.SetNumZeroed(NumFaces);

		for (const FStaticMeshSection& Section : StaticMeshLOD.Sections)
		{
			uint32 FirstTriangle = Section.FirstIndex / 3;
			for (uint32 TriangleIndex = 0; TriangleIndex < Section.NumTriangles; ++TriangleIndex)
			{
				OutRawMesh.FaceMaterialIndices[FirstTriangle + TriangleIndex] = Section.MaterialIndex;
			}
		}
	}

	// Smoothing masks
	{
		OutRawMesh.FaceSmoothingMasks.Empty(NumFaces);
		OutRawMesh.FaceSmoothingMasks.SetNumUninitialized(NumFaces);

		for (auto& SmoothingMask : OutRawMesh.FaceSmoothingMasks)
		{
			SmoothingMask = 1;
		}
	}
}

bool FMeshMergeHelpers::CheckWrappingUVs(const TArray<FVector2D>& UVs)
{	
	bool bResult = false;

	FVector2D Min(FLT_MAX, FLT_MAX);
	FVector2D Max(-FLT_MAX, -FLT_MAX);
	for (const FVector2D& Coordinate : UVs)
	{
		if ((FMath::IsNegativeFloat(Coordinate.X) || FMath::IsNegativeFloat(Coordinate.Y)) || (Coordinate.X > (1.0f + KINDA_SMALL_NUMBER) || Coordinate.Y > (1.0f + KINDA_SMALL_NUMBER)))
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

void FMeshMergeHelpers::CullTrianglesFromVolumesAndUnderLandscapes(const UWorld* World, const FBoxSphereBounds& Bounds, FRawMesh& InOutRawMesh)
{
	TArray<ALandscapeProxy*> Landscapes;
	TArray<AMeshMergeCullingVolume*> CullVolumes;

	FBox BoxBounds = Bounds.GetBox();

	for (ULevel* Level : World->GetLevels())
	{
		for (AActor* Actor : Level->Actors)
		{
			ALandscape* Proxy = Cast<ALandscape>(Actor);
			if (Proxy && Proxy->bUseLandscapeForCullingInvisibleHLODVertices)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);

				// Ignore Z axis for 2d bounds check
				if (LandscapeBox.IntersectXY(BoxBounds))
				{
					Landscapes.Add(Proxy->GetLandscapeActor());
				}
			}

			// Check for culling volumes
			AMeshMergeCullingVolume* Volume = Cast<AMeshMergeCullingVolume>(Actor);
			if (Volume)
			{
				// If the mesh's bounds intersect with the volume there is a possibility of culling
				const bool bIntersecting = Volume->EncompassesPoint(Bounds.Origin, Bounds.SphereRadius, nullptr);
				if (bIntersecting)
				{
					CullVolumes.Add(Volume);
				}
			}
		}
	}

	TArray<bool> VertexVisible;
	VertexVisible.AddZeroed(InOutRawMesh.VertexPositions.Num());
	int32 Index = 0;

	for (const FVector& Position : InOutRawMesh.VertexPositions)
	{
		// Start with setting visibility to true on all vertices
		VertexVisible[Index] = true;

		// Check if this vertex is culled due to being underneath a landscape
		if (Landscapes.Num() > 0)
		{
			bool bVertexWithinLandscapeBounds = false;

			for (ALandscapeProxy* Proxy : Landscapes)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);
				bVertexWithinLandscapeBounds |= LandscapeBox.IsInsideXY(Position);
			}

			if (bVertexWithinLandscapeBounds)
			{
				const FVector Start = Position;
				FVector End = Position - (WORLD_MAX * FVector::UpVector);
				FVector OutHit;
				const bool IsAboveLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				End = Position + (WORLD_MAX * FVector::UpVector);
				const bool IsUnderneathLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				// Vertex is visible when above landscape (with actual landscape underneath) or if there is no landscape beneath or above the vertex (falls outside of landscape bounds)
				VertexVisible[Index] = (IsAboveLandscape && !IsUnderneathLandscape);// || (!IsAboveLandscape && !IsUnderneathLandscape);
			}
		}

		// Volume culling	
		for (AMeshMergeCullingVolume* Volume : CullVolumes)
		{
			const bool bVertexIsInsideVolume = Volume->EncompassesPoint(Position, 0.0f, nullptr);
			if (bVertexIsInsideVolume)
			{
				// Inside a culling volume so invisible
				VertexVisible[Index] = false;
			}
		}

		Index++;
	}


	// We now know which vertices are below the landscape
	TArray<bool> TriangleVisible;
	int32 NumTriangles = InOutRawMesh.WedgeIndices.Num() / 3;
	TriangleVisible.AddZeroed(NumTriangles);

	bool bCreateNewMesh = false;

	// Determine which triangles of the mesh are visible
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
	{
		bool AboveLandscape = false;

		for (int32 WedgeIndex = 0; WedgeIndex < 3; ++WedgeIndex)
		{
			AboveLandscape |= VertexVisible[InOutRawMesh.WedgeIndices[(TriangleIndex * 3) + WedgeIndex]];
		}
		TriangleVisible[TriangleIndex] = AboveLandscape;
		bCreateNewMesh |= !AboveLandscape;

	}

	// Check whether or not we have to create a new mesh
	if (bCreateNewMesh)
	{
		FRawMesh NewRawMesh;
		TMap<int32, int32> VertexRemapping;

		// Fill new mesh with data only from visible triangles
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			if (!TriangleVisible[TriangleIndex])
				continue;

			for (int32 WedgeIndex = 0; WedgeIndex < 3; ++WedgeIndex)
			{
				int32 OldIndex = InOutRawMesh.WedgeIndices[(TriangleIndex * 3) + WedgeIndex];

				int32 NewIndex;

				int32* RemappedIndex = VertexRemapping.Find(Index);
				if (RemappedIndex)
				{
					NewIndex = *RemappedIndex;
				}
				else
				{
					NewIndex = NewRawMesh.VertexPositions.Add(InOutRawMesh.VertexPositions[OldIndex]);
					VertexRemapping.Add(OldIndex, NewIndex);
				}

				NewRawMesh.WedgeIndices.Add(NewIndex);
				if (InOutRawMesh.WedgeColors.Num()) NewRawMesh.WedgeColors.Add(InOutRawMesh.WedgeColors[(TriangleIndex * 3) + WedgeIndex]);
				if (InOutRawMesh.WedgeTangentX.Num()) NewRawMesh.WedgeTangentX.Add(InOutRawMesh.WedgeTangentX[(TriangleIndex * 3) + WedgeIndex]);
				if (InOutRawMesh.WedgeTangentY.Num()) NewRawMesh.WedgeTangentY.Add(InOutRawMesh.WedgeTangentY[(TriangleIndex * 3) + WedgeIndex]);
				if (InOutRawMesh.WedgeTangentZ.Num()) NewRawMesh.WedgeTangentZ.Add(InOutRawMesh.WedgeTangentZ[(TriangleIndex * 3) + WedgeIndex]);

				for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; ++UVIndex)
				{
					if (InOutRawMesh.WedgeTexCoords[UVIndex].Num())
					{
						NewRawMesh.WedgeTexCoords[UVIndex].Add(InOutRawMesh.WedgeTexCoords[UVIndex][(TriangleIndex * 3) + WedgeIndex]);
					}
				}
			}

			NewRawMesh.FaceMaterialIndices.Add(InOutRawMesh.FaceMaterialIndices[TriangleIndex]);
			NewRawMesh.FaceSmoothingMasks.Add(InOutRawMesh.FaceSmoothingMasks[TriangleIndex]);
		}

		InOutRawMesh = NewRawMesh;
	}
}

void FMeshMergeHelpers::PropagateSplineDeformationToRawMesh(const USplineMeshComponent* InSplineMeshComponent, struct FRawMesh &OutRawMesh)
{
	// Apply spline deformation for each vertex's tangents
	for (int32 iVert = 0; iVert < OutRawMesh.WedgeIndices.Num(); ++iVert)
	{
		uint32 Index = OutRawMesh.WedgeIndices[iVert];
		float& AxisValue = USplineMeshComponent::GetAxisValue(OutRawMesh.VertexPositions[Index], InSplineMeshComponent->ForwardAxis);
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);

		// Transform tangents first
		if (OutRawMesh.WedgeTangentX.Num())
		{
			OutRawMesh.WedgeTangentX[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentX[iVert]);
		}

		if (OutRawMesh.WedgeTangentY.Num())
		{
			OutRawMesh.WedgeTangentY[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentY[iVert]);
		}

		if (OutRawMesh.WedgeTangentZ.Num())
		{
			OutRawMesh.WedgeTangentZ[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentZ[iVert]);
		}
	}

	// Apply spline deformation for each vertex position
	for (int32 iVert = 0; iVert < OutRawMesh.VertexPositions.Num(); ++iVert)
	{
		float& AxisValue = USplineMeshComponent::GetAxisValue(OutRawMesh.VertexPositions[iVert], InSplineMeshComponent->ForwardAxis);
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);
		AxisValue = 0.0f;
		OutRawMesh.VertexPositions[iVert] = SliceTransform.TransformPosition(OutRawMesh.VertexPositions[iVert]);
	}
}

void FMeshMergeHelpers::PropagateSplineDeformationToPhysicsGeometry(USplineMeshComponent* SplineMeshComponent, FKAggregateGeom& InOutPhysicsGeometry)
{
	const FVector Mask = USplineMeshComponent::GetAxisMask(SplineMeshComponent->GetForwardAxis());

	for (FKConvexElem& Elem : InOutPhysicsGeometry.ConvexElems)
	{
		for (FVector& Position : Elem.VertexData)
		{
			const float& AxisValue = USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis);
			FTransform SliceTransform = SplineMeshComponent->CalcSliceTransform(AxisValue);
			Position = SliceTransform.TransformPosition(Position * Mask);
		}

		Elem.UpdateElemBox();
	}

	for (FKSphereElem& Elem : InOutPhysicsGeometry.SphereElems)
	{
		const FVector WorldSpaceCenter = Elem.GetTransform().TransformPosition(Elem.Center);
		Elem.Center = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(WorldSpaceCenter, SplineMeshComponent->ForwardAxis)).TransformPosition(Elem.Center * Mask);
	}

	for (FKSphylElem& Elem : InOutPhysicsGeometry.SphylElems)
	{
		const FVector WorldSpaceCenter = Elem.GetTransform().TransformPosition(Elem.Center);
		Elem.Center = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(WorldSpaceCenter, SplineMeshComponent->ForwardAxis)).TransformPosition(Elem.Center * Mask);
	}
}

void FMeshMergeHelpers::TransformRawMeshVertexData(const FTransform& InTransform, FRawMesh &OutRawMesh)
{
	for (FVector& Vertex : OutRawMesh.VertexPositions)
	{
		Vertex = InTransform.TransformPosition(Vertex);
	}

	for (FVector& TangentX : OutRawMesh.WedgeTangentX)
	{
		TangentX = InTransform.TransformVector(TangentX).GetSafeNormal();
	}

	for (FVector& TangentY : OutRawMesh.WedgeTangentY)
	{
		TangentY = InTransform.TransformVector(TangentY).GetSafeNormal();
	}

	for (FVector& TangentZ : OutRawMesh.WedgeTangentZ)
	{
		TangentZ = InTransform.TransformVector(TangentZ).GetSafeNormal();
	}

	const bool bIsMirrored = InTransform.GetDeterminant() < 0.f;
	if (bIsMirrored)
	{
		Algo::Reverse(OutRawMesh.WedgeIndices);
		Algo::Reverse(OutRawMesh.WedgeTangentX);
		Algo::Reverse(OutRawMesh.WedgeTangentY);
		Algo::Reverse(OutRawMesh.WedgeTangentZ);
		for (uint32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; ++UVIndex)
		{
			Algo::Reverse(OutRawMesh.WedgeTexCoords[UVIndex]);
		}
		Algo::Reverse(OutRawMesh.FaceMaterialIndices);
		Algo::Reverse(OutRawMesh.FaceSmoothingMasks);
		Algo::Reverse(OutRawMesh.WedgeColors);
	}
}

void FMeshMergeHelpers::RetrieveCullingLandscapeAndVolumes(UWorld* InWorld, const FBoxSphereBounds& EstimatedMeshProxyBounds, const TEnumAsByte<ELandscapeCullingPrecision::Type> PrecisionType, TArray<FRawMesh*>& CullingRawMeshes)
{
	// Extract landscape proxies and cull volumes from the world
	TArray<ALandscapeProxy*> LandscapeActors;
	TArray<AMeshMergeCullingVolume*> CullVolumes;

	uint32 MaxLandscapeExportLOD = 0;
	if (InWorld->IsValidLowLevel())
	{
		for (FConstLevelIterator Iterator = InWorld->GetLevelIterator(); Iterator; ++Iterator)
		{
			for (AActor* Actor : (*Iterator)->Actors)
			{
				if (Actor)
				{
					ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
					if (LandscapeProxy && LandscapeProxy->bUseLandscapeForCullingInvisibleHLODVertices)
					{
						// Retrieve highest landscape LOD level possible
						MaxLandscapeExportLOD = FMath::Max(MaxLandscapeExportLOD, FMath::CeilLogTwo(LandscapeProxy->SubsectionSizeQuads + 1) - 1);
						LandscapeActors.Add(LandscapeProxy);
					}
					// Check for culling volumes
					AMeshMergeCullingVolume* Volume = Cast<AMeshMergeCullingVolume>(Actor);
					if (Volume)
					{
						// If the mesh's bounds intersect with the volume there is a possibility of culling
						const bool bIntersecting = Volume->EncompassesPoint(EstimatedMeshProxyBounds.Origin, EstimatedMeshProxyBounds.SphereRadius, nullptr);
						if (bIntersecting)
						{
							CullVolumes.Add(Volume);
						}
					}
				}
			}
		}
	}

	// Setting determines the precision at which we should export the landscape for culling (highest, half or lowest)
	const uint32 LandscapeExportLOD = ((float)MaxLandscapeExportLOD * (0.5f * (float)PrecisionType));
	for (ALandscapeProxy* Landscape : LandscapeActors)
	{
		// Export the landscape to raw mesh format
		FRawMesh* LandscapeRawMesh = new FRawMesh();
		FBoxSphereBounds LandscapeBounds = EstimatedMeshProxyBounds;
		Landscape->ExportToRawMesh(LandscapeExportLOD, *LandscapeRawMesh, LandscapeBounds);
		if (LandscapeRawMesh->VertexPositions.Num())
		{
			CullingRawMeshes.Add(LandscapeRawMesh);
		}
	}

	// Also add volume mesh data as culling meshes
	for (AMeshMergeCullingVolume* Volume : CullVolumes)
	{
		// Export the landscape to raw mesh format
		FRawMesh* VolumeMesh = new FRawMesh();

		TArray<FStaticMaterial>	VolumeMaterials;
		GetBrushMesh(Volume, Volume->Brush, *VolumeMesh, VolumeMaterials);

		// Offset vertices to correct world position;
		FVector VolumeLocation = Volume->GetActorLocation();
		for (FVector& Position : VolumeMesh->VertexPositions)
		{
			Position += VolumeLocation;
		}

		CullingRawMeshes.Add(VolumeMesh);
	}
}

void FMeshMergeHelpers::TransformPhysicsGeometry(const FTransform& InTransform, struct FKAggregateGeom& AggGeom)
{
	FTransform NoScaleInTransform = InTransform;
	NoScaleInTransform.SetScale3D(FVector(1, 1, 1));

	// Pre-scale all non-convex geometry		
	const FVector Scale3D = InTransform.GetScale3D();
	if (!Scale3D.Equals(FVector(1.f)))
	{
		const float MinPrimSize = KINDA_SMALL_NUMBER;

		for (FKSphereElem& Elem : AggGeom.SphereElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}

		for (FKBoxElem& Elem : AggGeom.BoxElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}

		for (FKSphylElem& Elem : AggGeom.SphylElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}
	}
	
	// Multiply out merge transform (excluding scale) with original transforms for non-convex geometry
	for (FKSphereElem& Elem : AggGeom.SphereElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKBoxElem& Elem : AggGeom.BoxElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKSphylElem& Elem : AggGeom.SphylElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKConvexElem& Elem : AggGeom.ConvexElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*InTransform);
	}
}

void FMeshMergeHelpers::ExtractPhysicsGeometry(UBodySetup* InBodySetup, const FTransform& ComponentToWorld, struct FKAggregateGeom& OutAggGeom)
{
	if (InBodySetup == nullptr)
	{
		return;
	}


	OutAggGeom = InBodySetup->AggGeom;

	// Convert boxes to convex, so they can be sheared 
	for (int32 BoxIdx = 0; BoxIdx < OutAggGeom.BoxElems.Num(); BoxIdx++)
	{
		FKConvexElem* NewConvexColl = new(OutAggGeom.ConvexElems) FKConvexElem();
		NewConvexColl->ConvexFromBoxElem(OutAggGeom.BoxElems[BoxIdx]);
	}
	OutAggGeom.BoxElems.Empty();

	// we are not owner of this stuff
	OutAggGeom.RenderInfo = nullptr;
	for (FKConvexElem& Elem : OutAggGeom.ConvexElems)
	{
		Elem.SetConvexMesh(nullptr);
		Elem.SetMirroredConvexMesh(nullptr);
	}

	// Transform geometry to world space
	TransformPhysicsGeometry(ComponentToWorld, OutAggGeom);
}

FVector2D FMeshMergeHelpers::GetValidUV(const FVector2D& UV)
{
	FVector2D NewUV = UV;
	// first make sure they're positive
	if (UV.X < 0.0f)
	{
		NewUV.X = UV.X + FMath::CeilToInt(FMath::Abs(UV.X));
	}

	if (UV.Y < 0.0f)
	{
		NewUV.Y = UV.Y + FMath::CeilToInt(FMath::Abs(UV.Y));
	}

	// now make sure they're within [0, 1]
	if (UV.X > 1.0f)
	{
		NewUV.X = FMath::Fmod(NewUV.X, 1.0f);
	}

	if (UV.Y > 1.0f)
	{
		NewUV.Y = FMath::Fmod(NewUV.Y, 1.0f);
	}

	return NewUV;
}

void FMeshMergeHelpers::CalculateTextureCoordinateBoundsForRawMesh(const FRawMesh& InRawMesh, TArray<FBox2D>& OutBounds)
{
	const int32 NumWedges = InRawMesh.WedgeIndices.Num();
	const int32 NumTris = NumWedges / 3;

	OutBounds.Empty();
	for (int32 TriIndex = 0; TriIndex < NumTris; TriIndex++)
	{
		int MaterialIndex = InRawMesh.FaceMaterialIndices[TriIndex];
		if (OutBounds.Num() <= MaterialIndex)
			OutBounds.SetNumZeroed(MaterialIndex + 1);
		{
			const int32 CachedWedgeIndex = TriIndex * 3;
			for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; ++UVIndex)
			{
				int32 WedgeIndex = CachedWedgeIndex;
				if (InRawMesh.WedgeTexCoords[UVIndex].Num())
				{
					for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++, WedgeIndex++)
					{
						OutBounds[MaterialIndex] += InRawMesh.WedgeTexCoords[UVIndex][WedgeIndex];
					}
				}
			}
		}
	}
}

bool FMeshMergeHelpers::PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh)
{
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

	if (StaticMesh->SourceModels.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData[LODIndex].OverrideVertexColors != nullptr)
	{
		FColorVertexBuffer& ColorVertexBuffer = *StaticMeshComponent->LODData[LODIndex].OverrideVertexColors;
		FStaticMeshLODResources& RenderModel = StaticMesh->RenderData->LODResources[LODIndex];

		if (ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
		{	
			const int32 NumWedges = RawMesh.WedgeIndices.Num();
			const int32 NumRenderWedges = RenderModel.IndexBuffer.GetNumIndices();
			const bool bUseRenderWedges = NumWedges == NumRenderWedges;
					
			if (bUseRenderWedges)
			{
				const int32 NumExistingColors = RawMesh.WedgeColors.Num();
				if (NumExistingColors < NumRenderWedges)
				{
					RawMesh.WedgeColors.AddUninitialized(NumRenderWedges - NumExistingColors);
				}

				const FIndexArrayView ArrayView = RenderModel.IndexBuffer.GetArrayView();
				for (int32 WedgeIndex = 0; WedgeIndex < NumRenderWedges; WedgeIndex++)
				{
					const int32 Index = ArrayView[WedgeIndex];
					FColor WedgeColor = FColor::White;
					if (Index != INDEX_NONE)
					{
						WedgeColor = ColorVertexBuffer.VertexColor(Index);
					}

					RawMesh.WedgeColors[WedgeIndex] = WedgeColor;
				}

				return true;				
			}
			// No wedge map (this can happen when we poly reduce the LOD for example)
			// Use index buffer directly
			else
			{
				RawMesh.WedgeColors.SetNumUninitialized(NumWedges);

				if (RawMesh.VertexPositions.Num() == ColorVertexBuffer.GetNumVertices())
				{
					for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
					{
						FColor WedgeColor = FColor::White;
						uint32 VertIndex = RawMesh.WedgeIndices[WedgeIndex];

						if (VertIndex < ColorVertexBuffer.GetNumVertices())
						{
							WedgeColor = ColorVertexBuffer.VertexColor(VertIndex);
						}
						RawMesh.WedgeColors[WedgeIndex] = WedgeColor;
					}

					return true;
				}
			}
		}
	}

	return false;
}

bool FMeshMergeHelpers::IsLandscapeHit(const FVector& RayOrigin, const FVector& RayEndPoint, const UWorld* World, const TArray<ALandscapeProxy*>& LandscapeProxies, FVector& OutHitLocation)
{
	TArray<FHitResult> Results;
	// Each landscape component has 2 collision shapes, 1 of them is specific to landscape editor
	// Trace only ECC_Visibility channel, so we do hit only Editor specific shape
	World->LineTraceMultiByObjectType(Results, RayOrigin, RayEndPoint, FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(LandscapeTrace), true));

	bool bHitLandscape = false;

	for (const FHitResult& HitResult : Results)
	{
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(HitResult.Component.Get());
		if (CollisionComponent)
		{
			ALandscapeProxy* HitLandscape = CollisionComponent->GetLandscapeProxy();
			if (HitLandscape && LandscapeProxies.Contains(HitLandscape))
			{
				// Could write a correct clipping algorithm, that clips the triangle to hit location
				OutHitLocation = HitLandscape->LandscapeActorToWorld().InverseTransformPosition(HitResult.Location);
				// Above landscape so visible
				bHitLandscape = true;
			}
		}
	}

	return bHitLandscape;
}
