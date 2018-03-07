// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMeshMerge.cpp: Unreal skeletal mesh merging implementation.
=============================================================================*/

#include "SkeletalMeshMerge.h"
#include "GPUSkinPublicDefs.h"
#include "SkeletalMeshTypes.h"
#include "RawIndexBuffer.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"

/*-----------------------------------------------------------------------------
	FSkeletalMeshMerge
-----------------------------------------------------------------------------*/

/**
* Constructor
* @param InMergeMesh - destination mesh to merge to
* @param InSrcMeshList - array of source meshes to merge
* @param InForceSectionMapping - optional array to map sections from the source meshes to merged section entries
*/
FSkeletalMeshMerge::FSkeletalMeshMerge(USkeletalMesh* InMergeMesh, 
									   const TArray<USkeletalMesh*>& InSrcMeshList, 
									   const TArray<FSkelMeshMergeSectionMapping>& InForceSectionMapping,
									   int32 InStripTopLODs,
                                       EMeshBufferAccess InMeshBufferAccess,
									   FSkelMeshMergeUVTransforms* InSectionUVTransforms)
:	MergeMesh(InMergeMesh)
,	SrcMeshList(InSrcMeshList)
,	StripTopLODs(InStripTopLODs)
,   MeshBufferAccess(InMeshBufferAccess)
,	ForceSectionMapping(InForceSectionMapping)
,	SectionUVTransforms(InSectionUVTransforms)
{
	check(MergeMesh);
}

/** Helper macro to call GenerateLODModel which requires compile time vertex type. */
#define GENERATE_LOD_MODEL( VertexType, NumUVs, bHasExtraBoneInfluences ) \
{\
	switch( NumUVs )\
	{\
	case 1:\
		GenerateLODModel< VertexType<1>, TSkinWeightInfo<bHasExtraBoneInfluences> >( LODIdx + StripTopLODs );\
		break;\
	case 2:\
		GenerateLODModel< VertexType<2>, TSkinWeightInfo<bHasExtraBoneInfluences> >( LODIdx + StripTopLODs );\
		break;\
	case 3:\
		GenerateLODModel< VertexType<3>, TSkinWeightInfo<bHasExtraBoneInfluences> >( LODIdx + StripTopLODs );\
		break;\
	case 4:\
		GenerateLODModel< VertexType<4>, TSkinWeightInfo<bHasExtraBoneInfluences> >( LODIdx + StripTopLODs );\
		break;\
	default:\
		checkf(false, TEXT("Invalid number of UV sets.  Must be between 0 and 4") );\
		break;\
	}\
}\

/**
* Merge/Composite the list of source meshes onto the merge one
* The MergeMesh is reinitialized 
* @return true if succeeded
*/
bool FSkeletalMeshMerge::DoMerge(TArray<FRefPoseOverride>* RefPoseOverrides /* = nullptr */)
{
	MergeSkeleton(RefPoseOverrides);

	return FinalizeMesh();
}

void FSkeletalMeshMerge::MergeSkeleton(const TArray<FRefPoseOverride>* RefPoseOverrides /* = nullptr */)
{
	// Release the rendering resources.

	MergeMesh->ReleaseResources();
	MergeMesh->ReleaseResourcesFence.Wait();

	// Build the reference skeleton & sockets.

	BuildReferenceSkeleton(SrcMeshList, NewRefSkeleton, MergeMesh->Skeleton);
	BuildSockets(SrcMeshList);

	// Override the reference bone poses & sockets, if specified.

	if (RefPoseOverrides)
	{
		OverrideReferenceSkeletonPose(*RefPoseOverrides, NewRefSkeleton, MergeMesh->Skeleton);
		OverrideMergedSockets(*RefPoseOverrides);
	}

	// Assign new referencer skeleton.

	MergeMesh->RefSkeleton = NewRefSkeleton;

	// Rebuild inverse ref pose matrices here as some access patterns 
	// may need to access these matrices before FinalizeMesh is called
	// (which would *normally* rebuild the inv ref matrices).
	MergeMesh->RefBasesInvMatrix.Empty();
	MergeMesh->CalculateInvRefMatrices();
}

bool FSkeletalMeshMerge::FinalizeMesh()
{
	bool Result = true;

	// Find the common maximum number of LODs available in the list of source meshes.

	int32 MaxNumLODs = CalculateLodCount(SrcMeshList);

	if (MaxNumLODs == -1)
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("FSkeletalMeshMerge: Invalid source mesh list"));
		return false;
	}

	ReleaseResources(MaxNumLODs);

	// Create a mapping from each input mesh bone to bones in the merged mesh.

	SrcMeshInfo.Empty();
	SrcMeshInfo.AddZeroed(SrcMeshList.Num());

	for (int32 MeshIdx = 0; MeshIdx < SrcMeshList.Num(); MeshIdx++)
	{
		USkeletalMesh* SrcMesh = SrcMeshList[MeshIdx];
		if (SrcMesh)
		{
			if (SrcMesh->bHasVertexColors)
			{
				MergeMesh->bHasVertexColors = true;
			}

			FMergeMeshInfo& MeshInfo = SrcMeshInfo[MeshIdx];
			MeshInfo.SrcToDestRefSkeletonMap.AddUninitialized(SrcMesh->RefSkeleton.GetRawBoneNum());

			for (int32 i = 0; i < SrcMesh->RefSkeleton.GetRawBoneNum(); i++)
			{
				FName SrcBoneName = SrcMesh->RefSkeleton.GetBoneName(i);
				int32 DestBoneIndex = NewRefSkeleton.FindBoneIndex(SrcBoneName);

				if (DestBoneIndex == INDEX_NONE)
				{
					// Missing bones shouldn't be possible, but can happen with invalid meshes;
					// map any bone we are missing to the 'root'.

					DestBoneIndex = 0;
				}

				MeshInfo.SrcToDestRefSkeletonMap[i] = DestBoneIndex;
			}
		}
	}

	// If things are going ok so far...
	if (Result)
	{
		FSkeletalMeshResource* SkelMeshResource = MergeMesh->GetImportedResource();

		// force 16 bit UVs if supported on hardware
		MergeMesh->bUseFullPrecisionUVs = GVertexElementTypeSupport.IsSupported(VET_Half2) ? false : true;

		// Array of per-lod number of UV sets
		TArray<uint32> PerLODNumUVSets;
		TArray<bool> PerLODExtraBoneInfluences;
		PerLODNumUVSets.AddZeroed(MaxNumLODs);
		PerLODExtraBoneInfluences.AddZeroed(MaxNumLODs);

		// Get the number of UV sets for each LOD.
		for (int32 MeshIdx = 0; MeshIdx < SrcMeshList.Num(); MeshIdx++)
		{
			USkeletalMesh* SrcSkelMesh = SrcMeshList[MeshIdx];
			FSkeletalMeshResource* SrcResource = SrcSkelMesh->GetImportedResource();

			for (int32 LODIdx = 0; LODIdx < MaxNumLODs; LODIdx++)
			{
				if (SrcResource->LODModels.IsValidIndex(LODIdx))
				{
					uint32& NumUVSets = PerLODNumUVSets[LODIdx];
					NumUVSets = FMath::Max(NumUVSets, SrcResource->LODModels[LODIdx].NumTexCoords);

					PerLODExtraBoneInfluences[LODIdx] |= SrcResource->LODModels[LODIdx].DoSectionsNeedExtraBoneInfluences();
				}
			}
		}

		// process each LOD for the new merged mesh
		for (int32 LODIdx = 0; LODIdx < MaxNumLODs; LODIdx++)
		{
			if (!MergeMesh->bUseFullPrecisionUVs)
			{
				if (PerLODExtraBoneInfluences[LODIdx])
				{
					GENERATE_LOD_MODEL(TGPUSkinVertexFloat16Uvs, PerLODNumUVSets[LODIdx], true);
				}
				else
				{
					GENERATE_LOD_MODEL(TGPUSkinVertexFloat16Uvs, PerLODNumUVSets[LODIdx], false);
				}
			}
			else
			{
				if (PerLODExtraBoneInfluences[LODIdx])
				{
					GENERATE_LOD_MODEL(TGPUSkinVertexFloat32Uvs, PerLODNumUVSets[LODIdx], true);
				}
				else
				{
					GENERATE_LOD_MODEL(TGPUSkinVertexFloat32Uvs, PerLODNumUVSets[LODIdx], false);
				}
			}
		}
		// update the merge skel mesh entries
		if (!ProcessMergeMesh())
		{
			Result = false;
		}

		// Reinitialize the mesh's render resources.
		MergeMesh->InitResources();
	}

	return Result;
}

/**
* Merge a bonemap with an existing bonemap and keep track of remapping
* (a bonemap is a list of indices of bones in the USkeletalMesh::RefSkeleton array)
* @param MergedBoneMap - out merged bonemap
* @param BoneMapToMergedBoneMap - out of mapping from original bonemap to new merged bonemap 
* @param BoneMap - input bonemap to merge
*/
void FSkeletalMeshMerge::MergeBoneMap( TArray<FBoneIndexType>& MergedBoneMap, TArray<FBoneIndexType>& BoneMapToMergedBoneMap, const TArray<FBoneIndexType>& BoneMap )
{
	BoneMapToMergedBoneMap.AddUninitialized( BoneMap.Num() );
	for( int32 IdxB=0; IdxB < BoneMap.Num(); IdxB++ )
	{
		BoneMapToMergedBoneMap[IdxB] = MergedBoneMap.AddUnique( BoneMap[IdxB] );
	}
}

static void BoneMapToNewRefSkel(const TArray<FBoneIndexType>& InBoneMap, const TArray<int32>& SrcToDestRefSkeletonMap, TArray<FBoneIndexType>& OutBoneMap)
{
	OutBoneMap.Empty();
	OutBoneMap.AddUninitialized(InBoneMap.Num());

	for(int32 i=0; i<InBoneMap.Num(); i++)
	{
		check(InBoneMap[i] < SrcToDestRefSkeletonMap.Num());
		OutBoneMap[i] = SrcToDestRefSkeletonMap[InBoneMap[i]];
	}
}

/**
* Generate the list of sections that need to be created along with info needed to merge sections
* @param NewSectionArray - out array to populate
* @param LODIdx - current LOD to process
*/
void FSkeletalMeshMerge::GenerateNewSectionArray( TArray<FNewSectionInfo>& NewSectionArray, int32 LODIdx )
{
	const int32 MaxGPUSkinBones = GetFeatureLevelMaxNumberOfBones(GMaxRHIFeatureLevel);

	NewSectionArray.Empty();
	for( int32 MeshIdx=0; MeshIdx < SrcMeshList.Num(); MeshIdx++ )
	{
		// source mesh
		USkeletalMesh* SrcMesh = SrcMeshList[MeshIdx];

		if( SrcMesh )
		{
			FSkeletalMeshResource* SrcResource = SrcMesh->GetImportedResource();
			int32 SourceLODIdx = FMath::Min(LODIdx, SrcResource->LODModels.Num()-1);
			FStaticLODModel& SrcLODModel = SrcResource->LODModels[SourceLODIdx];
			FSkeletalMeshLODInfo& SrcLODInfo = SrcMesh->LODInfo[SourceLODIdx];

			// iterate over each section of this LOD
			for( int32 SectionIdx=0; SectionIdx < SrcLODModel.Sections.Num(); SectionIdx++ )
			{
				int32 MaterialId = -1;
				// check for the optional list of material ids corresponding to the list of src meshes
				// if the id is valid (not -1) it is used to find an existing section entry to merge with
				if( ForceSectionMapping.Num() == SrcMeshList.Num() &&
					ForceSectionMapping.IsValidIndex(MeshIdx) &&
					ForceSectionMapping[MeshIdx].SectionIDs.IsValidIndex(SectionIdx) )
				{
					MaterialId = ForceSectionMapping[MeshIdx].SectionIDs[SectionIdx];
				}

				FSkelMeshSection& Section = SrcLODModel.Sections[SectionIdx];

				// Convert Chunk.BoneMap from src to dest bone indices
				TArray<FBoneIndexType> DestChunkBoneMap;
				BoneMapToNewRefSkel(Section.BoneMap, SrcMeshInfo[MeshIdx].SrcToDestRefSkeletonMap, DestChunkBoneMap);


				// get the material for this section
				int32 MaterialIndex = Section.MaterialIndex;
				// use the remapping of material indices for all LODs besides the base LOD 
				if( LODIdx > 0 && 
					SrcLODInfo.LODMaterialMap.IsValidIndex(Section.MaterialIndex) )
				{
					MaterialIndex = FMath::Clamp<int32>( SrcLODInfo.LODMaterialMap[Section.MaterialIndex], 0, SrcMesh->Materials.Num() );
				}
				UMaterialInterface* Material = SrcMesh->Materials[MaterialIndex].MaterialInterface;

				// see if there is an existing entry in the array of new sections that matches its material
				// if there is a match then the source section can be added to its list of sections to merge 
				int32 FoundIdx = INDEX_NONE;
				for( int32 Idx=0; Idx < NewSectionArray.Num(); Idx++ )
				{
					FNewSectionInfo& NewSectionInfo = NewSectionArray[Idx];
					// check for a matching material or a matching material index id if it is valid
					if( (MaterialId == -1 && Material == NewSectionInfo.Material) ||
						(MaterialId != -1 && MaterialId == NewSectionInfo.MaterialId) )
					{
						check(NewSectionInfo.MergeSections.Num());

						// merge the bonemap from the source section with the existing merged bonemap
						TArray<FBoneIndexType> TempMergedBoneMap(NewSectionInfo.MergedBoneMap);
						TArray<FBoneIndexType> TempBoneMapToMergedBoneMap;									
						MergeBoneMap(TempMergedBoneMap,TempBoneMapToMergedBoneMap,DestChunkBoneMap);

						// check to see if the newly merged bonemap is still within the bone limit for GPU skinning
						if( TempMergedBoneMap.Num() <= MaxGPUSkinBones )
						{
							TArray<FTransform> SrcUVTransform;
							if (SectionUVTransforms != nullptr && MeshIdx < SectionUVTransforms->UVTransformsPerMesh.Num())
							{
								SrcUVTransform = SectionUVTransforms->UVTransformsPerMesh[MeshIdx];
							}

							// add the source section as a new merge entry
							FMergeSectionInfo& MergeSectionInfo = *new(NewSectionInfo.MergeSections) FMergeSectionInfo(
								SrcMesh,
								&SrcLODModel.Sections[SectionIdx],
								SrcUVTransform
								);
							// keep track of remapping for the existing chunk's bonemap 
							// so that the bone matrix indices can be updated for the vertices
							MergeSectionInfo.BoneMapToMergedBoneMap = TempBoneMapToMergedBoneMap;

							// use the updated bonemap for this new section
							NewSectionInfo.MergedBoneMap = TempMergedBoneMap;

							// keep track of the entry that was found
							FoundIdx = Idx;
							break;
						}
					}
				}

				// new section entries will be created if the material for the source section was not found 
				// or merging it with an existing entry would go over the bone limit for GPU skinning
				if( FoundIdx == INDEX_NONE )
				{
					// create a new section entry
					const FMeshUVChannelInfo& UVChannelData = SrcMesh->Materials[MaterialIndex].UVChannelData;
					FNewSectionInfo& NewSectionInfo = *new(NewSectionArray) FNewSectionInfo(Material,MaterialId,UVChannelData);
					// initialize the merged bonemap to simply use the original chunk bonemap
					NewSectionInfo.MergedBoneMap = DestChunkBoneMap;

					TArray<FTransform> SrcUVTransform;
					if (SectionUVTransforms != nullptr && MeshIdx < SectionUVTransforms->UVTransformsPerMesh.Num())
					{
						SrcUVTransform = SectionUVTransforms->UVTransformsPerMesh[MeshIdx];
					}
					// add a new merge section entry
					FMergeSectionInfo& MergeSectionInfo = *new(NewSectionInfo.MergeSections) FMergeSectionInfo(
						SrcMesh,
						&SrcLODModel.Sections[SectionIdx],
						SrcUVTransform);
					// since merged bonemap == chunk.bonemap then remapping is just pass-through
					MergeSectionInfo.BoneMapToMergedBoneMap.Empty( DestChunkBoneMap.Num() );
					for( int32 i=0; i < DestChunkBoneMap.Num(); i++ )
					{
						MergeSectionInfo.BoneMapToMergedBoneMap.Add((FBoneIndexType)i);
					}
				}
			}
		}
	}
}

template<typename VertexDataType>
void FSkeletalMeshMerge::CopyVertexFromSource(VertexDataType& DestVert, const FStaticLODModel& SrcLODModel, int32 SourceVertIdx, const FMergeSectionInfo& MergeSectionInfo)
{
	const TGPUSkinVertexBase* SrcBaseVert = SrcLODModel.VertexBufferGPUSkin.GetVertexPtr(SourceVertIdx);

	DestVert.Position = SrcLODModel.VertexBufferGPUSkin.GetVertexPositionFast(SrcBaseVert);
	DestVert.TangentX = SrcBaseVert->TangentX;
	DestVert.TangentZ = SrcBaseVert->TangentZ;

	// Copy all UVs that are available
	uint32 LODNumTexCoords = SrcLODModel.VertexBufferGPUSkin.GetNumTexCoords();
	for (uint32 UVIndex = 0; UVIndex < LODNumTexCoords && UVIndex < MAX_TEXCOORDS; ++UVIndex)
	{
		FVector2D UVs = SrcLODModel.VertexBufferGPUSkin.GetVertexUVFast(SourceVertIdx, UVIndex);
		if (UVIndex < (uint32)MergeSectionInfo.UVTransforms.Num())
		{
			FVector Transformed = MergeSectionInfo.UVTransforms[UVIndex].TransformPosition(FVector(UVs, 1.f));
			UVs = FVector2D(Transformed.X, Transformed.Y);
		}
		DestVert.UVs[UVIndex] = UVs;
	}
}

template<typename SkinWeightType, bool bHasExtraBoneInfluences>
void FSkeletalMeshMerge::CopyWeightFromSource(SkinWeightType& DestWeight, const FStaticLODModel& SrcLODModel, int32 SourceVertIdx, const FMergeSectionInfo& MergeSectionInfo)
{
	const TSkinWeightInfo<bHasExtraBoneInfluences>* SrcSkinWeights = SrcLODModel.SkinWeightVertexBuffer.GetSkinWeightPtr<bHasExtraBoneInfluences>(SourceVertIdx);

	// if source doesn't have extra influence, we have to clear the buffer
	FMemory::Memzero(DestWeight.InfluenceBones);
	FMemory::Memzero(DestWeight.InfluenceWeights);

	FMemory::Memcpy(DestWeight.InfluenceBones, SrcSkinWeights->InfluenceBones, sizeof(SrcSkinWeights->InfluenceBones));
	FMemory::Memcpy(DestWeight.InfluenceWeights, SrcSkinWeights->InfluenceWeights, sizeof(SrcSkinWeights->InfluenceWeights));
}

/**
* Creates a new LOD model and adds the new merged sections to it. Modifies the MergedMesh.
* @param LODIdx - current LOD to process
*/
template<typename VertexDataType, typename SkinWeightType>
void FSkeletalMeshMerge::GenerateLODModel( int32 LODIdx )
{
	// add the new LOD model entry
	FSkeletalMeshResource* MergeResource = MergeMesh->GetImportedResource();
	FStaticLODModel& MergeLODModel = *new(MergeResource->LODModels) FStaticLODModel;
	// add the new LOD info entry
	FSkeletalMeshLODInfo& MergeLODInfo = *new(MergeMesh->LODInfo) FSkeletalMeshLODInfo;
	MergeLODInfo.ScreenSize = MergeLODInfo.LODHysteresis = MAX_FLT;

	// generate an array with info about new sections that need to be created
	TArray<FNewSectionInfo> NewSectionArray;
	GenerateNewSectionArray( NewSectionArray, LODIdx );

	uint32 MaxIndex = 0;

	// merged vertex buffer
	TArray< VertexDataType > MergedVertexBuffer;
	// merged skin weight buffer
	TArray< SkinWeightType > MergedSkinWeightBuffer;
	// merged vertex color buffer
	TArray< FColor > MergedColorBuffer;
	// merged index buffer
	TArray<uint32> MergedIndexBuffer;

	// The total number of UV sets for this LOD model
	uint32 TotalNumUVs = 0;

	// true if any extra bone influence exists
	bool bSourceHasExtraBoneInfluences = false;

	for( int32 CreateIdx=0; CreateIdx < NewSectionArray.Num(); CreateIdx++ )
	{
		FNewSectionInfo& NewSectionInfo = NewSectionArray[CreateIdx];

		// ActiveBoneIndices contains all the bones used by the verts from all the sections of this LOD model
		// Add the bones used by this new section
		for( int32 Idx=0; Idx < NewSectionInfo.MergedBoneMap.Num(); Idx++ )
		{
			MergeLODModel.ActiveBoneIndices.AddUnique( NewSectionInfo.MergedBoneMap[Idx] );
		}

		// add the new section entry
		FSkelMeshSection& Section = *new(MergeLODModel.Sections) FSkelMeshSection;

		// set the new bonemap from the merged sections
		// these are the bones that will be used by this new section
		Section.BoneMap = NewSectionInfo.MergedBoneMap;

		// init vert totals
		Section.NumVertices = 0;

		// keep track of the current base vertex for this section in the merged vertex buffer
		Section.BaseVertexIndex = MergedVertexBuffer.Num();


		// The current implementation of skeletal mesh merging can't do more than two sections.
		// Therefore, we cannot e.g. create new sections based on the source section shadow casting.
		MergeLODInfo.TriangleSortSettings.AddZeroed();

		// find existing material index
		check(MergeMesh->Materials.Num() == MaterialIds.Num());
		int32 MatIndex;
		if(NewSectionInfo.MaterialId == -1)
		{
			MatIndex = MergeMesh->Materials.Find(NewSectionInfo.Material);
		}
		else
		{
			MatIndex = MaterialIds.Find(NewSectionInfo.MaterialId);
		}

		// if it doesn't exist, make new entry
		if(MatIndex == INDEX_NONE)
		{
			FSkeletalMaterial SkeletalMaterial(NewSectionInfo.Material, true);
			SkeletalMaterial.UVChannelData = NewSectionInfo.UVChannelData;
			MergeMesh->Materials.Add(SkeletalMaterial);
			MaterialIds.Add(NewSectionInfo.MaterialId);
			Section.MaterialIndex = MergeMesh->Materials.Num()-1;
		}
		else
		{
			Section.MaterialIndex = MatIndex;
		}
		
		// init tri totals
		Section.NumTriangles = 0;
		// keep track of the current base index for this section in the merged index buffer
		Section.BaseIndex = MergedIndexBuffer.Num();

		FMeshUVChannelInfo& MergedUVData = MergeMesh->Materials[Section.MaterialIndex].UVChannelData;

		// iterate over all of the sections that need to be merged together
		for( int32 MergeIdx=0; MergeIdx < NewSectionInfo.MergeSections.Num(); MergeIdx++ )
		{
			FMergeSectionInfo& MergeSectionInfo = NewSectionInfo.MergeSections[MergeIdx];
			int32 SourceLODIdx = FMath::Min(LODIdx, MergeSectionInfo.SkelMesh->GetImportedResource()->LODModels.Num()-1);

			// Take the max UV density for each UVChannel between all sections that are being merged.
			{
				const int32 NewSectionMatId = MergeSectionInfo.Section->MaterialIndex;
				const FMeshUVChannelInfo& NewSectionUVData = MergeSectionInfo.SkelMesh->Materials[NewSectionMatId].UVChannelData;

				for (int32 i = 0; i < MAX_TEXCOORDS; i++)
				{
					const float NewSectionUVDensity = NewSectionUVData.LocalUVDensities[i];
					float& UVDensity = MergedUVData.LocalUVDensities[i];

					UVDensity = FMath::Max(UVDensity, NewSectionUVDensity);
				}
			}

			// get the source skel LOD info from this merge entry
			const FSkeletalMeshLODInfo& SrcLODInfo = MergeSectionInfo.SkelMesh->LODInfo[SourceLODIdx];

			// keep track of the lowest LOD displayfactor and hysterisis
			MergeLODInfo.ScreenSize = FMath::Min<float>(MergeLODInfo.ScreenSize, SrcLODInfo.ScreenSize);
			MergeLODInfo.LODHysteresis = FMath::Min<float>(MergeLODInfo.LODHysteresis,SrcLODInfo.LODHysteresis);

			// get the source skel LOD model from this merge entry
			const FStaticLODModel& SrcLODModel = MergeSectionInfo.SkelMesh->GetImportedResource()->LODModels[SourceLODIdx];

			// add required bones from this source model entry to the merge model entry
			for( int32 Idx=0; Idx < SrcLODModel.RequiredBones.Num(); Idx++ )
			{
				FName SrcLODBoneName = MergeSectionInfo.SkelMesh->RefSkeleton.GetBoneName( SrcLODModel.RequiredBones[Idx] );
				int32 MergeBoneIndex = NewRefSkeleton.FindBoneIndex(SrcLODBoneName);
				
				if (MergeBoneIndex != INDEX_NONE)
				{
					MergeLODModel.RequiredBones.AddUnique(MergeBoneIndex);
				}
			}

			// keep track of the max number of influences used by the vertices of the chunk
			Section.MaxBoneInfluences = FMath::Max<int32>(Section.MaxBoneInfluences, MergeSectionInfo.Section->MaxBoneInfluences );
			// update vert total
			Section.NumVertices += MergeSectionInfo.Section->GetNumVertices();

			// update total number of vertices 
			int32 NumTotalVertices = MergeSectionInfo.Section->GetNumVertices();
			MergeLODModel.NumVertices += NumTotalVertices;					

			// add the vertices from the original source mesh to the merged vertex buffer					
			int32 MaxVertIdx = FMath::Min<int32>( 
				MergeSectionInfo.Section->BaseVertexIndex + NumTotalVertices,
				SrcLODModel.VertexBufferGPUSkin.GetNumVertices() 
				);

			int32 MaxColorIdx = SrcLODModel.ColorVertexBuffer.GetNumVertices();

			// keep track of the current base vertex index before adding any new vertices
			// this will be needed to remap the index buffer values to the new range
			int32 CurrentBaseVertexIndex = MergedVertexBuffer.Num();
			const bool bSourceExtraBoneInfluence = SrcLODModel.SkinWeightVertexBuffer.HasExtraBoneInfluences();
			for( int32 VertIdx=MergeSectionInfo.Section->BaseVertexIndex; VertIdx < MaxVertIdx; VertIdx++ )
			{
				// add the new vertex
				VertexDataType& DestVert = MergedVertexBuffer[MergedVertexBuffer.AddUninitialized()];
				SkinWeightType& DestWeight = MergedSkinWeightBuffer[MergedSkinWeightBuffer.AddUninitialized()];

				CopyVertexFromSource<VertexDataType>(DestVert, SrcLODModel, VertIdx, MergeSectionInfo);

				bSourceHasExtraBoneInfluences |= bSourceExtraBoneInfluence;
				if (bSourceExtraBoneInfluence)
				{
					CopyWeightFromSource<SkinWeightType, true>(DestWeight, SrcLODModel, VertIdx, MergeSectionInfo);
				}
				else
				{
					CopyWeightFromSource<SkinWeightType, false>(DestWeight, SrcLODModel, VertIdx, MergeSectionInfo);
				}

				// if the mesh uses vertex colors, copy the source color if possible or default to white
				if( MergeMesh->bHasVertexColors )
				{
					if( VertIdx < MaxColorIdx )
					{
						const FColor& SrcColor = SrcLODModel.ColorVertexBuffer.VertexColor(VertIdx);
						MergedColorBuffer.Add(SrcColor);
					}
					else
					{
						const FColor ColorWhite(255, 255, 255);
						MergedColorBuffer.Add(ColorWhite);
					}
				}

				uint32 LODNumTexCoords = SrcLODModel.VertexBufferGPUSkin.GetNumTexCoords();
				if( TotalNumUVs < LODNumTexCoords )
				{
					TotalNumUVs = LODNumTexCoords;
				}

				// remap the bone index used by this vertex to match the mergedbonemap 
				for( int32 Idx=0; Idx < SkinWeightType::NumInfluences; Idx++ )
				{
					if (DestWeight.InfluenceWeights[Idx] > 0)
					{
						checkSlow(MergeSectionInfo.BoneMapToMergedBoneMap.IsValidIndex(DestWeight.InfluenceBones[Idx]));
						DestWeight.InfluenceBones[Idx] = (uint8)MergeSectionInfo.BoneMapToMergedBoneMap[DestWeight.InfluenceBones[Idx]];
					}
				}
			}

			// update total number of triangles
			Section.NumTriangles += MergeSectionInfo.Section->NumTriangles;

			// add the indices from the original source mesh to the merged index buffer					
			int32 MaxIndexIdx = FMath::Min<int32>( 
				MergeSectionInfo.Section->BaseIndex + MergeSectionInfo.Section->NumTriangles * 3, 
				SrcLODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num() 
				);
			for( int32 IndexIdx=MergeSectionInfo.Section->BaseIndex; IndexIdx < MaxIndexIdx; IndexIdx++ )
			{
				uint32 SrcIndex = SrcLODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(IndexIdx);

				// add offset to each index to match the new entries in the merged vertex buffer
				checkSlow(SrcIndex >= MergeSectionInfo.Section->BaseVertexIndex);
				uint32 DstIndex = SrcIndex - MergeSectionInfo.Section->BaseVertexIndex + CurrentBaseVertexIndex;
				checkSlow(DstIndex < (uint32)MergedVertexBuffer.Num());

				// add the new index to the merged vertex buffer
				MergedIndexBuffer.Add(DstIndex);
				if( MaxIndex < DstIndex )
				{
					MaxIndex = DstIndex;
				}

			}
		}
	}

    const bool bNeedsCPUAccess = (MeshBufferAccess == EMeshBufferAccess::ForceCPUAndGPU) ||
                                    MergeResource->RequiresCPUSkinning(GMaxRHIFeatureLevel);

	// sort required bone array in strictly increasing order
	MergeLODModel.RequiredBones.Sort();
	MergeMesh->RefSkeleton.EnsureParentsExistAndSort(MergeLODModel.ActiveBoneIndices);	

	// copy the new vertices and indices to the vertex buffer for the new model
	MergeLODModel.VertexBufferGPUSkin.SetUseFullPrecisionUVs(MergeMesh->bUseFullPrecisionUVs);
	// set CPU skinning on vertex buffer since it affects the type of TResourceArray needed
	MergeLODModel.VertexBufferGPUSkin.SetNeedsCPUAccess(bNeedsCPUAccess);
	// Set the number of tex coords on this vertex buffer
	MergeLODModel.VertexBufferGPUSkin.SetNumTexCoords(TotalNumUVs);
	MergeLODModel.NumTexCoords = TotalNumUVs;

	MergeLODModel.SkinWeightVertexBuffer.SetHasExtraBoneInfluences(bSourceHasExtraBoneInfluences);
	MergeLODModel.SkinWeightVertexBuffer.SetNeedsCPUAccess(bNeedsCPUAccess);

	// copy vertex resource arrays
	MergeLODModel.VertexBufferGPUSkin = MergedVertexBuffer;
	MergeLODModel.SkinWeightVertexBuffer = MergedSkinWeightBuffer;

	if( MergeMesh->bHasVertexColors )
	{
		MergeLODModel.ColorVertexBuffer.InitFromColorArray(MergedColorBuffer);
	}

	FMultiSizeIndexContainerData IndexBufferData;
	IndexBufferData.DataTypeSize = (MaxIndex < MAX_uint16) ? sizeof(uint16) : sizeof(uint32);
	IndexBufferData.Indices = MergedIndexBuffer;
	MergeLODModel.MultiSizeIndexContainer.RebuildIndexBuffer( IndexBufferData );
}

/**
* (Re)initialize and merge skeletal mesh info from the list of source meshes to the merge mesh
* @return true if succeeded
*/
bool FSkeletalMeshMerge::ProcessMergeMesh()
{
	bool Result=true;
	
	// copy settings and bone info from src meshes
	bool bNeedsInit=true;

	MergeMesh->SkelMirrorTable.Empty();

	for( int32 MeshIdx=0; MeshIdx < SrcMeshList.Num(); MeshIdx++ )
	{
		USkeletalMesh* SrcMesh = SrcMeshList[MeshIdx];
		if( SrcMesh )
		{
			if( bNeedsInit )
			{
				// initialize the merged mesh with the first src mesh entry used
				MergeMesh->SetImportedBounds(SrcMesh->GetImportedBounds());

				MergeMesh->SkelMirrorAxis = SrcMesh->SkelMirrorAxis;
				MergeMesh->SkelMirrorFlipAxis = SrcMesh->SkelMirrorFlipAxis;

				// only initialize once
				bNeedsInit = false;
			}
			else
			{
				// add bounds
				MergeMesh->SetImportedBounds(MergeMesh->GetImportedBounds() + SrcMesh->GetImportedBounds());
			}
		}
	}

	// Rebuild inverse ref pose matrices.
	MergeMesh->RefBasesInvMatrix.Empty();
	MergeMesh->CalculateInvRefMatrices();

	return Result;
}

int32 FSkeletalMeshMerge::CalculateLodCount(const TArray<USkeletalMesh*>& SourceMeshList) const
{
	int32 LodCount = INT_MAX;

	for (int32 i = 0, MeshCount = SourceMeshList.Num(); i < MeshCount; ++i)
	{
		USkeletalMesh* SourceMesh = SourceMeshList[i];

		if (SourceMesh)
		{
			LodCount = FMath::Min<int32>(LodCount, SourceMesh->LODInfo.Num());
		}
	}

	if (LodCount == INT_MAX)
	{
		return -1;
	}

	// Decrease the number of LODs we are going to make based on StripTopLODs.
	// But, make sure there is at least one.

	LodCount -= StripTopLODs;
	LodCount = FMath::Max(LodCount, 1);
	
	return LodCount;
}

void FSkeletalMeshMerge::BuildReferenceSkeleton(const TArray<USkeletalMesh*>& SourceMeshList, FReferenceSkeleton& RefSkeleton, const USkeleton* SkeletonAsset)
{
	RefSkeleton.Empty();

	// Iterate through all the source mesh reference skeletons and compose the merged reference skeleton.

	FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, SkeletonAsset);

	for (int32 MeshIndex = 0; MeshIndex < SourceMeshList.Num(); ++MeshIndex)
	{
		USkeletalMesh* SourceMesh = SourceMeshList[MeshIndex];

		if (!SourceMesh)
		{
			continue;
		}

		// Initialise new RefSkeleton with first mesh.

		if (RefSkeleton.GetRawBoneNum() == 0)
		{
			RefSkeleton = SourceMesh->RefSkeleton;
			continue;
		}

		// For subsequent meshes, add any missing bones.

		for (int32 i = 1; i < SourceMesh->RefSkeleton.GetRawBoneNum(); ++i)
		{
			FName SourceBoneName = SourceMesh->RefSkeleton.GetBoneName(i);
			int32 TargetBoneIndex = RefSkeleton.FindRawBoneIndex(SourceBoneName);

			// If the source bone is present in the new RefSkeleton, we skip it.

			if (TargetBoneIndex != INDEX_NONE)
			{
				continue;
			}

			// Add the source bone to the RefSkeleton.

			int32 SourceParentIndex = SourceMesh->RefSkeleton.GetParentIndex(i);
			FName SourceParentName = SourceMesh->RefSkeleton.GetBoneName(SourceParentIndex);
			int32 TargetParentIndex = RefSkeleton.FindRawBoneIndex(SourceParentName);

			if (TargetParentIndex == INDEX_NONE)
			{
				continue;
			}

			FMeshBoneInfo MeshBoneInfo = SourceMesh->RefSkeleton.GetRefBoneInfo()[i];
			MeshBoneInfo.ParentIndex = TargetParentIndex;

			RefSkelModifier.Add(MeshBoneInfo, SourceMesh->RefSkeleton.GetRefBonePose()[i]);
		}
	}
}

void FSkeletalMeshMerge::OverrideReferenceSkeletonPose(const TArray<FRefPoseOverride>& PoseOverrides, FReferenceSkeleton& TargetSkeleton, const USkeleton* SkeletonAsset)
{
	for (int32 i = 0, PoseMax = PoseOverrides.Num(); i < PoseMax; ++i)
	{
		const FRefPoseOverride& PoseOverride = PoseOverrides[i];
		const FReferenceSkeleton& SourceSkeleton = PoseOverride.SkeletalMesh->RefSkeleton;

		FReferenceSkeletonModifier RefSkelModifier(TargetSkeleton, SkeletonAsset);

		for (int32 j = 0, BoneMax = PoseOverride.Overrides.Num(); j < BoneMax; ++j)
		{
			const FName& BoneName = PoseOverride.Overrides[j].BoneName;
			int32 SourceBoneIndex = SourceSkeleton.FindBoneIndex(BoneName);

			if (SourceBoneIndex != INDEX_NONE)
			{
				bool bOverrideBone = (PoseOverride.Overrides[j].OverrideMode == FRefPoseOverride::ChildrenOnly) ? false : true;

				if (bOverrideBone)
				{
					OverrideReferenceBonePose(SourceBoneIndex, SourceSkeleton, RefSkelModifier);
				}

				bool bOverrideChildren = (PoseOverride.Overrides[j].OverrideMode == FRefPoseOverride::BoneOnly) ? false : true;

				if (bOverrideChildren)
				{
					for (int32 ChildBoneIndex = SourceBoneIndex + 1; ChildBoneIndex < SourceSkeleton.GetRawBoneNum(); ++ChildBoneIndex)
					{
						if (SourceSkeleton.BoneIsChildOf(ChildBoneIndex, SourceBoneIndex))
						{
							OverrideReferenceBonePose(ChildBoneIndex, SourceSkeleton, RefSkelModifier);
						}
					}
				}
			}
		}
	}
}

bool FSkeletalMeshMerge::OverrideReferenceBonePose(int32 SourceBoneIndex, const FReferenceSkeleton& SourceSkeleton, FReferenceSkeletonModifier& TargetSkeleton)
{
	FName BoneName = SourceSkeleton.GetBoneName(SourceBoneIndex);
	int32 TargetBoneIndex = TargetSkeleton.GetReferenceSkeleton().FindBoneIndex(BoneName);

	if (TargetBoneIndex != INDEX_NONE)
	{
		const FTransform& SourceBoneTransform = SourceSkeleton.GetRefBonePose()[SourceBoneIndex];
		TargetSkeleton.UpdateRefPoseTransform(TargetBoneIndex, SourceBoneTransform);

		return true;
	}

	return false;
}

void FSkeletalMeshMerge::ReleaseResources(int32 Slack)
{
	FSkeletalMeshResource* Resource = MergeMesh->GetImportedResource();

	Resource->LODModels.Empty(Slack);
	MergeMesh->LODInfo.Empty(Slack);
	MergeMesh->Materials.Empty();
}

bool FSkeletalMeshMerge::AddSocket(const USkeletalMeshSocket* NewSocket, bool bIsSkeletonSocket)
{
	TArray<USkeletalMeshSocket*>& MergeMeshSockets = MergeMesh->GetMeshOnlySocketList();

	// Verify the socket doesn't already exist in the current Mesh list.
	for (USkeletalMeshSocket const * const ExistingSocket : MergeMeshSockets)
	{
		if (ExistingSocket->SocketName == NewSocket->SocketName)
		{
			return false;
		}
	}

	// The Skeleton will only be valid in cases where the passed in mesh already had a skeleton
	// (i.e. an existing mesh was used, or a created mesh was explicitly assigned a skeleton).
	// In either case, we want to avoid adding sockets to the Skeleton (as it is shared), but we
	// still need to check against it to prevent duplication.
	if (bIsSkeletonSocket && MergeMesh->Skeleton)
	{
		for (USkeletalMeshSocket const * const ExistingSocket : MergeMesh->Skeleton->Sockets)
		{
			return false;
		}
	}

	USkeletalMeshSocket* NewSocketDuplicate = CastChecked<USkeletalMeshSocket>(StaticDuplicateObject(NewSocket, MergeMesh));
	MergeMeshSockets.Add(NewSocketDuplicate);

	return true;
}

void FSkeletalMeshMerge::AddSockets(const TArray<USkeletalMeshSocket*>& NewSockets, bool bAreSkeletonSockets)
{
	for (USkeletalMeshSocket* NewSocket : NewSockets)
	{
		AddSocket(NewSocket, bAreSkeletonSockets);
	}
}

void FSkeletalMeshMerge::BuildSockets(const TArray<USkeletalMesh*>& SourceMeshList)
{
	TArray<USkeletalMeshSocket*>& MeshSocketList = MergeMesh->GetMeshOnlySocketList();
	MeshSocketList.Empty();

	// Iterate through the all the source MESH sockets, only adding the new sockets.

	for (USkeletalMesh const * const SourceMesh : SourceMeshList)
	{
		if (SourceMesh)
		{
			const TArray<USkeletalMeshSocket*>& NewMeshSocketList = SourceMesh->GetMeshOnlySocketList();
			AddSockets(NewMeshSocketList, false);
		}
	}

	// Iterate through the all the source SKELETON sockets, only adding the new sockets.

	for (USkeletalMesh const * const SourceMesh : SourceMeshList)
	{
		if (SourceMesh)
		{
			const TArray<USkeletalMeshSocket*>& NewSkeletonSocketList = SourceMesh->Skeleton->Sockets;
			AddSockets(NewSkeletonSocketList, true);
		}
	}
}

void FSkeletalMeshMerge::OverrideSocket(const USkeletalMeshSocket* SourceSocket)
{
	TArray<USkeletalMeshSocket*>& SocketList = MergeMesh->GetMeshOnlySocketList();

	for (int32 i = 0, SocketCount = SocketList.Num(); i < SocketCount; ++i)
	{
		USkeletalMeshSocket* TargetSocket = SocketList[i];

		if (TargetSocket->SocketName == SourceSocket->SocketName)
		{
			TargetSocket->BoneName = SourceSocket->BoneName;
			TargetSocket->RelativeLocation = SourceSocket->RelativeLocation;
			TargetSocket->RelativeRotation = SourceSocket->RelativeRotation;
			TargetSocket->RelativeScale = SourceSocket->RelativeScale;
		}
	}
}

void FSkeletalMeshMerge::OverrideBoneSockets(const FName& BoneName, const TArray<USkeletalMeshSocket*>& SourceSocketList)
{
	for (int32 i = 0, SourceSocketCount = SourceSocketList.Num(); i < SourceSocketCount; ++i)
	{
		const USkeletalMeshSocket* SourceSocket = SourceSocketList[i];

		if (SourceSocket->BoneName == BoneName)
		{
			OverrideSocket(SourceSocket);
		}
	}
}

void FSkeletalMeshMerge::OverrideMergedSockets(const TArray<FRefPoseOverride>& PoseOverrides)
{
	for (int32 i = 0, PoseMax = PoseOverrides.Num(); i < PoseMax; ++i)
	{
		const FRefPoseOverride& PoseOverride = PoseOverrides[i];
		const FReferenceSkeleton& SourceSkeleton = PoseOverride.SkeletalMesh->RefSkeleton;

		const TArray<USkeletalMeshSocket*>& SkeletonSocketList = PoseOverride.SkeletalMesh->Skeleton->Sockets;
		const TArray<USkeletalMeshSocket*>& MeshSocketList = const_cast<USkeletalMesh*>(PoseOverride.SkeletalMesh)->GetMeshOnlySocketList();

		for (int32 j = 0, BoneMax = PoseOverride.Overrides.Num(); j < BoneMax; ++j)
		{
			const FName& BoneName = PoseOverride.Overrides[j].BoneName;
			int32 SourceBoneIndex = SourceSkeleton.FindBoneIndex(BoneName);

			if (SourceBoneIndex != INDEX_NONE)
			{
				bool bOverrideBone = (PoseOverride.Overrides[j].OverrideMode == FRefPoseOverride::ChildrenOnly) ? false : true;

				if (bOverrideBone)
				{
					OverrideBoneSockets(BoneName, SkeletonSocketList);
					OverrideBoneSockets(BoneName, MeshSocketList);
				}

				bool bOverrideChildren = (PoseOverride.Overrides[j].OverrideMode == FRefPoseOverride::BoneOnly) ? false : true;

				if (bOverrideChildren)
				{
					for (int32 ChildBoneIndex = SourceBoneIndex + 1; ChildBoneIndex < SourceSkeleton.GetRawBoneNum(); ++ChildBoneIndex)
					{
						if (SourceSkeleton.BoneIsChildOf(ChildBoneIndex, SourceBoneIndex))
						{
							FName ChildBoneName = SourceSkeleton.GetBoneName(ChildBoneIndex);

							OverrideBoneSockets(ChildBoneName, SkeletonSocketList);
							OverrideBoneSockets(ChildBoneName, MeshSocketList);
						}
					}
				}
			}
		}
	}
}