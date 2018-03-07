// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Assets/ClothingAsset.h"
#include "PhysXPublic.h"

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "MeshUtilities.h"
#endif

#include "Components/SkeletalMeshComponent.h"

#include "PhysicsPublic.h"
#include "UObjectIterator.h"
#include "ModuleManager.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ComponentReregisterContext.h"
#include "AnimPhysObjectVersion.h"
#include "ClothingMeshUtils.h"

DEFINE_LOG_CATEGORY(LogClothingAsset)

#define LOCTEXT_NAMESPACE "ClothingAsset"

UClothingAsset::UClothingAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReferenceBoneIndex(0)
	, CustomData(nullptr)
{

}

void UClothingAsset::RefreshBoneMapping(USkeletalMesh* InSkelMesh)
{
	// No mesh, can't remap
	if(!InSkelMesh)
	{
		return;
	}

	if(UsedBoneNames.Num() != UsedBoneIndices.Num())
	{
		UsedBoneIndices.Reset();
		UsedBoneIndices.AddDefaulted(UsedBoneNames.Num());
	}

	// Repopulate the used indices.
	for(int32 BoneNameIndex = 0; BoneNameIndex < UsedBoneNames.Num(); ++BoneNameIndex)
	{
		UsedBoneIndices[BoneNameIndex] = InSkelMesh->RefSkeleton.FindBoneIndex(UsedBoneNames[BoneNameIndex]);
	}
}

#if WITH_EDITOR

void LogAndToastClothingInfo(const FText& Error)
{
	FNotificationInfo Info(Error);
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogClothingAsset, Warning, TEXT("%s"), *Error.ToString());
}

bool UClothingAsset::BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex)
{
	// If we've been added to the wrong mesh
	if(InSkelMesh != GetOuter())
	{
		FText Error = FText::Format(LOCTEXT("Error_WrongMesh", "Failed to bind clothing asset {0} as the provided mesh is not the owner of this asset."), FText::FromString(GetName()));
		LogAndToastClothingInfo(Error);

		return false;
	}

	// If we don't have clothing data
	if(!LodData.IsValidIndex(InAssetLodIndex))
	{
		FText Error = FText::Format(LOCTEXT("Error_NoClothingLod", "Failed to bind clothing asset {0} LOD{1} as LOD{2} does not exist."), FText::FromString(GetName()), FText::AsNumber(InAssetLodIndex), FText::AsNumber(InAssetLodIndex));
		LogAndToastClothingInfo(Error);

		return false;
	}

	// If we don't have a mesh
	if(!InSkelMesh)
	{
		FText Error = FText::Format(LOCTEXT("Error_NoMesh", "Failed to bind clothing asset {0} as provided skel mesh does not exist."), FText::FromString(GetName()));
		LogAndToastClothingInfo(Error);

		return false;
	}

	// If the mesh LOD index is invalid
	if(!InSkelMesh->GetImportedResource()->LODModels.IsValidIndex(InMeshLodIndex))
	{
		FText Error = FText::Format(LOCTEXT("Error_InvalidMeshLOD", "Failed to bind clothing asset {0} as mesh LOD{1} does not exist."), FText::FromString(GetName()), FText::AsNumber(InMeshLodIndex));
		LogAndToastClothingInfo(Error);

		return false;
	}

	const int32 NumMapEntries = LodMap.Num();
	for(int MapIndex = 0; MapIndex < NumMapEntries; ++MapIndex)
	{
		const int32& MappedLod = LodMap[MapIndex];

		if(MappedLod == InAssetLodIndex)
		{
			FText Error = FText::Format(LOCTEXT("Error_LodMapped", "Failed to bind clothing asset {0} LOD{1} as LOD{2} is already mapped to mesh LOD{3}."), FText::FromString(GetName()), FText::AsNumber(InAssetLodIndex), FText::AsNumber(InAssetLodIndex), FText::AsNumber(MapIndex));
			LogAndToastClothingInfo(Error);

			return false;
		}
	}

	if(LodMap.IsValidIndex(InMeshLodIndex) && LodMap[InMeshLodIndex] != INDEX_NONE)
	{
		// Already mapped
		return false;
	}

	BuildSelfCollisionData();
	CalculateReferenceBoneIndex();

	// Grab the clothing and skel lod data
	FClothLODData& ClothLodData = LodData[InAssetLodIndex];
	FStaticLODModel& SkelLod = InSkelMesh->GetImportedResource()->LODModels[InMeshLodIndex];

	FSkelMeshSection* OriginalSection = &SkelLod.Sections[InSectionIndex];
	FSkelMeshSection NewClothSection;

	// Data for mesh to mesh binding
	TArray<FMeshToMeshVertData> MeshToMeshData;
	TArray<FVector> RenderPositions;
	TArray<FVector> RenderNormals;
	TArray<FVector> RenderTangents;
	TArray<uint32> RenderIndices;

	RenderPositions.Reserve(OriginalSection->SoftVertices.Num());
	RenderNormals.Reserve(OriginalSection->SoftVertices.Num());
	RenderTangents.Reserve(OriginalSection->SoftVertices.Num());

	// Original data to weight to the clothing simulation mesh
	for(FSoftSkinVertex& UnrealVert : OriginalSection->SoftVertices)
	{
		RenderPositions.Add(UnrealVert.Position);
		RenderNormals.Add(UnrealVert.TangentZ);
		RenderTangents.Add(UnrealVert.TangentX);
	}

	SkelLod.MultiSizeIndexContainer.GetIndexBuffer(RenderIndices);
	TArrayView<uint32> IndexView(RenderIndices);
	IndexView.Slice(OriginalSection->BaseIndex, OriginalSection->NumTriangles * 3);

	ClothingMeshUtils::ClothMeshDesc TargetMesh(RenderPositions, RenderNormals, IndexView);
	ClothingMeshUtils::ClothMeshDesc SourceMesh(ClothLodData.PhysicalMeshData.Vertices, ClothLodData.PhysicalMeshData.Normals, ClothLodData.PhysicalMeshData.Indices);

	ClothingMeshUtils::GenerateMeshToMeshSkinningData(MeshToMeshData, TargetMesh, &RenderTangents, SourceMesh);

	if(MeshToMeshData.Num() == 0)
	{
		// Failed to generate skinning data, the function above will have notified
		// with the cause of the failure, so just exit
		return false;
	}

	// Calculate fixed verts
	for(FMeshToMeshVertData& VertData : MeshToMeshData)
	{
		float TriangleDistanceMax = 0.0f;
		TriangleDistanceMax += ClothLodData.PhysicalMeshData.MaxDistances[VertData.SourceMeshVertIndices[0]];
		TriangleDistanceMax += ClothLodData.PhysicalMeshData.MaxDistances[VertData.SourceMeshVertIndices[1]];
		TriangleDistanceMax += ClothLodData.PhysicalMeshData.MaxDistances[VertData.SourceMeshVertIndices[2]];

		if(TriangleDistanceMax == 0.0f)
		{
			VertData.SourceMeshVertIndices[3] = 0xFFFF;
		}
	}

	NewClothSection.SoftVertices = OriginalSection->SoftVertices;
	NewClothSection.NumVertices = NewClothSection.SoftVertices.Num();
	NewClothSection.BoneMap = OriginalSection->BoneMap;
	NewClothSection.MaxBoneInfluences = OriginalSection->MaxBoneInfluences;

	for(FName& BoneName : UsedBoneNames)
	{
		const int32 BoneIndex = InSkelMesh->RefSkeleton.FindBoneIndex(BoneName);

		if(BoneIndex != INDEX_NONE)
		{
			NewClothSection.BoneMap.AddUnique(BoneIndex);
		}
	}

	// Array of re-import contexts for components using this mesh
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* Component = *It;
		if (Component && !Component->IsTemplate() && Component->SkeletalMesh == InSkelMesh)
		{
			new(ComponentContexts) FComponentReregisterContext(Component);
		}
	}

	// Ready to apply the changes
	InSkelMesh->PreEditChange(nullptr);

	// calculate LOD verts before adding our new section
	uint32 NumLodVertices = 0;
	for(const FSkelMeshSection& CurSection : SkelLod.Sections)
	{
		NumLodVertices += CurSection.GetNumVertices();
	}

	int32 NewSectionIdx = SkelLod.Sections.AddDefaulted();

	SkelLod.Sections[NewSectionIdx] = SkelLod.Sections[InSectionIndex];

	// Re set original here, may have just realloced
	OriginalSection = &SkelLod.Sections[InSectionIndex];
	FSkelMeshSection& ClothSection = SkelLod.Sections[NewSectionIdx];
	
	ClothSection.bDisabled = false;
	ClothSection.CorrespondClothSectionIndex = InSectionIndex;

	OriginalSection->bDisabled = true;
	OriginalSection->CorrespondClothSectionIndex = NewSectionIdx;

	// Geo properties
	ClothSection.SoftVertices = NewClothSection.SoftVertices;
	ClothSection.NumVertices = NewClothSection.NumVertices;
	ClothSection.BoneMap = NewClothSection.BoneMap;
	ClothSection.MaxBoneInfluences = NewClothSection.MaxBoneInfluences;

	// Set the asset index, used during rendering to pick the correct sim mesh buffer
	int32 AssetIndex = INDEX_NONE;
	check(InSkelMesh->MeshClothingAssets.Find(this, AssetIndex));
	ClothSection.CorrespondClothAssetIndex = AssetIndex;
	
	ClothSection.BaseVertexIndex = NumLodVertices;

	// Material properties
	ClothSection.MaterialIndex = OriginalSection->MaterialIndex;
	ClothSection.TriangleSorting = OriginalSection->TriangleSorting;
	ClothSection.bSelected = OriginalSection->bSelected;
	ClothSection.bCastShadow = OriginalSection->bCastShadow;
	ClothSection.bRecomputeTangent = OriginalSection->bRecomputeTangent;
	ClothSection.bEnableClothLOD_DEPRECATED = OriginalSection->bEnableClothLOD_DEPRECATED;

	// sim properties
	ClothSection.ClothMappingData = MeshToMeshData;
	ClothSection.PhysicalMeshVertices = ClothLodData.PhysicalMeshData.Vertices;
	ClothSection.PhysicalMeshNormals = ClothLodData.PhysicalMeshData.Normals;

	// Set asset links on both sections
	ClothSection.ClothingData.AssetLodIndex = InAssetLodIndex;
	ClothSection.ClothingData.AssetGuid = AssetGuid;

	OriginalSection->ClothingData.AssetGuid = AssetGuid;
	OriginalSection->ClothingData.AssetLodIndex = InAssetLodIndex;

	// Copy original indices over
	const int32 BaseIndexToCopy = OriginalSection->BaseIndex;
	const int32 NumIndicesToCopy = OriginalSection->NumTriangles * 3;
	const int32 BaseVertexOffset = ClothSection.BaseVertexIndex - OriginalSection->BaseVertexIndex;

	FMultiSizeIndexContainerData NewIndexData;
	SkelLod.MultiSizeIndexContainer.GetIndexBuffer(NewIndexData.Indices);

	ClothSection.BaseIndex = NewIndexData.Indices.Num();
	ClothSection.NumTriangles = OriginalSection->NumTriangles;

	for(int32 CurrIdx = 0; CurrIdx < NumIndicesToCopy; ++CurrIdx)
	{
		int32 SourceIndex = NewIndexData.Indices[BaseIndexToCopy + CurrIdx];
		NewIndexData.Indices.Add(SourceIndex + BaseVertexOffset);
	}

	SkelLod.NumVertices += ClothSection.GetNumVertices();

	if(SkelLod.NumVertices > MAX_uint16)
	{
		NewIndexData.DataTypeSize = sizeof(uint32);
	}
	else
	{
		NewIndexData.DataTypeSize = sizeof(uint16);
	}

	SkelLod.MultiSizeIndexContainer.RebuildIndexBuffer(NewIndexData);
	
	// Build adjacency info (Only need soft verts for cloth)
	TArray<FSoftSkinVertex> TempVerts;
	FMultiSizeIndexContainerData AdjacencyData;
	SkelLod.GetVertices(TempVerts);
	IMeshUtilities& MeshUtils = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtils.BuildSkeletalAdjacencyIndexBuffer(TempVerts, SkelLod.NumTexCoords, NewIndexData.Indices, AdjacencyData.Indices);
	SkelLod.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(AdjacencyData);

	bool bRequireBoneChange = false;
	for(FBoneIndexType& BoneIndex : ClothSection.BoneMap)
	{
		if(!SkelLod.RequiredBones.Contains(BoneIndex))
		{
			bRequireBoneChange = true;
			if(InSkelMesh->RefSkeleton.IsValidIndex(BoneIndex))
			{
				SkelLod.RequiredBones.Add(BoneIndex);
				SkelLod.ActiveBoneIndices.AddUnique(BoneIndex);
			}
		}
	}

	if(bRequireBoneChange)
	{
		SkelLod.RequiredBones.Sort();
		InSkelMesh->RefSkeleton.EnsureParentsExistAndSort(SkelLod.ActiveBoneIndices);		
	}

	if(CustomData)
	{
		CustomData->BindToSkeletalMesh(InSkelMesh, InMeshLodIndex, InSectionIndex, InAssetLodIndex);
	}

	// Make sure the LOD map is always big enough for the asset to use.
	// This shouldn't grow to an unwieldy size but maybe consider compacting later.
	while(LodMap.Num() - 1 < InMeshLodIndex)
	{
		LodMap.Add(INDEX_NONE);
	}

	LodMap[InMeshLodIndex] = InAssetLodIndex;

	InSkelMesh->PostEditChange();

	return true;

	// ComponentContexts goes out of scope, causing components to be re-registered
}

void UClothingAsset::UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh)
{
	if(FSkeletalMeshResource* Mesh = InSkelMesh->GetImportedResource())
	{
		const int32 NumLods = Mesh->LODModels.Num();

		for(int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
		{
			UnbindFromSkeletalMesh(InSkelMesh, LodIndex);
		}
	}
}

void UClothingAsset::UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex)
{
	bool bChangedMesh = false;

	// Find the chunk(s) we created
	if(FSkeletalMeshResource* Mesh = InSkelMesh->GetImportedResource())
	{
		if(!Mesh->LODModels.IsValidIndex(InMeshLodIndex))
		{
			FText Error = FText::Format(LOCTEXT("Error_UnbindNoMeshLod", "Failed to remove clothing asset {0} from mesh LOD{1} as that LOD doesn't exist."), FText::FromString(GetName()), FText::AsNumber(InMeshLodIndex));
			LogAndToastClothingInfo(Error);

			return;
		}

		FStaticLODModel& LodModel = Mesh->LODModels[InMeshLodIndex];

		{
			for(int32 SectionIdx = LodModel.Sections.Num() - 1; SectionIdx >= 0; --SectionIdx)
			{
				FSkelMeshSection& ClothSection = LodModel.Sections[SectionIdx];

				if(ClothSection.ClothingData.IsValid() && ClothSection.ClothingData.AssetGuid == AssetGuid)
				{
					// Clear selection if we're going to remove that section
					if(InSkelMesh->SelectedEditorSection == SectionIdx)
					{
						InSkelMesh->SelectedEditorSection = INDEX_NONE;
					}

					// We made this one.
					int32 OriginalSectionIdx = INDEX_NONE;

					// Find the original section
					for(int32 SearchIdx = 0; SearchIdx < LodModel.Sections.Num(); ++SearchIdx)
					{
						FSkelMeshSection& PossibleOriginalSection = LodModel.Sections[SearchIdx];

						if(PossibleOriginalSection.CorrespondClothSectionIndex == SectionIdx)
						{
							OriginalSectionIdx = SearchIdx;
							break;
						}
					}

					check(OriginalSectionIdx != INDEX_NONE);

					InSkelMesh->PreEditChange(nullptr);

					FSkelMeshSection& OriginalSection = LodModel.Sections[OriginalSectionIdx];

					FMultiSizeIndexContainerData NewIndexData;
					LodModel.MultiSizeIndexContainer.GetIndexBuffer(NewIndexData.Indices);

					uint32 NumIndicesToRemove = ClothSection.NumTriangles * 3;
					uint32 BaseIndexToRemove = ClothSection.BaseIndex;

					uint32 NumVertsToRemove = ClothSection.GetNumVertices();
					uint32 BaseVertToRemove = ClothSection.BaseVertexIndex;

					// Remove from index buffer
					NewIndexData.Indices.RemoveAt(BaseIndexToRemove, NumIndicesToRemove);

					// Fix up remaining indices (can this start at baseindextoremove?)
					for(uint32& Index : NewIndexData.Indices)
					{
						if(Index >= BaseVertToRemove)
						{
							Index -= NumVertsToRemove;
						}
					}

					// Count resulting vertices
					int32 NumVertsAfter = 0;
					for(int32 ResultSectionsIndex = 0; ResultSectionsIndex < SectionIdx; ++ResultSectionsIndex)
					{
						NumVertsAfter += LodModel.Sections[ResultSectionsIndex].NumVertices;
					}

					if(NumVertsAfter > MAX_uint16)
					{
						NewIndexData.DataTypeSize = sizeof(uint16);
					}
					else
					{
						NewIndexData.DataTypeSize = sizeof(uint32);
					}

					// Push back to buffer
					LodModel.MultiSizeIndexContainer.RebuildIndexBuffer(NewIndexData);

					LodModel.Sections.RemoveAt(SectionIdx);

					LodModel.NumVertices -= NumVertsToRemove;

					// Fix up other data
					for(FSkelMeshSection& Section : LodModel.Sections)
					{
						if(Section.CorrespondClothSectionIndex > OriginalSection.CorrespondClothSectionIndex)
						{
							// We removed one here, so knock back
							Section.CorrespondClothSectionIndex -= 1;
						}

						if(Section.BaseIndex > BaseIndexToRemove)
						{
							Section.BaseIndex -= NumIndicesToRemove;
						}

						if(Section.BaseVertexIndex > BaseVertToRemove)
						{
							Section.BaseVertexIndex -= NumVertsToRemove;
						}
					}

					// Enable rendering for section
					OriginalSection.bDisabled = false;

					// Clear cloth link data
					OriginalSection.CorrespondClothSectionIndex = INDEX_NONE;
					OriginalSection.ClothingData.AssetLodIndex = INDEX_NONE;
					OriginalSection.ClothingData.AssetGuid = FGuid();

					// Track the mesh change so we can re-register later
					bChangedMesh = true;

					// Set the LOD map so nothing is mapped for this asset
					LodMap[InMeshLodIndex] = INDEX_NONE;

					InSkelMesh->PostEditChange();
				}
			}
		}
	}

	// If the mesh changed we need to re-register any components that use it to reflect the changes
	if(bChangedMesh)
	{
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* MeshComponent = *It;
			if(MeshComponent &&
			   !MeshComponent->IsTemplate() &&
			   MeshComponent->SkeletalMesh == InSkelMesh)
			{
				MeshComponent->ReregisterComponent();
			}
		}
	}
}

void UClothingAsset::InvalidateCachedData()
{
	for(FClothLODData& CurrentLodData : LodData)
	{
		// Recalculate inverse masses for the physical mesh particles
		FClothPhysicalMeshData& PhysMesh = CurrentLodData.PhysicalMeshData;

		check(PhysMesh.Indices.Num() % 3 == 0);

		TArray<float>& InvMasses = PhysMesh.InverseMasses;

		const int32 NumVerts = PhysMesh.Vertices.Num();
		InvMasses.Empty(NumVerts);
		InvMasses.AddZeroed(NumVerts);

		for(int32 TriBaseIndex = 0; TriBaseIndex < PhysMesh.Indices.Num(); TriBaseIndex += 3)
		{
			const int32 Index0 = PhysMesh.Indices[TriBaseIndex];
			const int32 Index1 = PhysMesh.Indices[TriBaseIndex + 1];
			const int32 Index2 = PhysMesh.Indices[TriBaseIndex + 2];

			const FVector AB = PhysMesh.Vertices[Index1] - PhysMesh.Vertices[Index0];
			const FVector AC = PhysMesh.Vertices[Index2] - PhysMesh.Vertices[Index0];
			const float TriArea = FVector::CrossProduct(AB, AC).Size();

			InvMasses[Index0] += TriArea;
			InvMasses[Index1] += TriArea;
			InvMasses[Index2] += TriArea;
		}

		PhysMesh.NumFixedVerts = 0;
		float MassSum = 0.0f;
		for(int32 CurrVertIndex = 0; CurrVertIndex < NumVerts; ++CurrVertIndex)
		{
			float& InvMass = InvMasses[CurrVertIndex];
			const float& MaxDistance = PhysMesh.MaxDistances[CurrVertIndex];

			if(MaxDistance < SMALL_NUMBER)
			{
				InvMass = 0.0f;
				++PhysMesh.NumFixedVerts;
			}
			else
			{
				MassSum += InvMass;
			}
		}

		if(MassSum > 0.0f)
		{
			const float MassScale = (float)(NumVerts - PhysMesh.NumFixedVerts) / MassSum;	

			for(float& InvMass : InvMasses)
			{
				if(InvMass != 0.0f)
				{
					InvMass *= MassScale;
					InvMass = 1.0f / InvMass;
				}
			}
		}

		// Calculate number of influences per vertex
		for(int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
		{
			FClothVertBoneData& BoneData = PhysMesh.BoneData[VertIndex];
			const uint16* BoneIndices = BoneData.BoneIndices;
			const float* BoneWeights = BoneData.BoneWeights;

			BoneData.NumInfluences = MAX_TOTAL_INFLUENCES;

			int32 NumInfluences = 0;
			for(int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
			{
				if(BoneWeights[InfluenceIndex] == 0.0f || BoneIndices[InfluenceIndex] == INDEX_NONE)
				{
					BoneData.NumInfluences = NumInfluences;
					break;
				}
				++NumInfluences;
			}
		}
	}
}

void UClothingAsset::BuildLodTransitionData()
{
	const int32 NumLods = LodData.Num();
	for(int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
	{
		const bool bHasPrevLod = LodIndex > 0;
		const bool bHasNextLod = LodIndex < NumLods - 1;

		FClothLODData& CurrentLod = LodData[LodIndex];
		FClothPhysicalMeshData& CurrentPhysMesh = CurrentLod.PhysicalMeshData;

		FClothLODData* PrevLod = bHasPrevLod ? &LodData[LodIndex - 1] : nullptr;
		FClothLODData* NextLod = bHasNextLod ? &LodData[LodIndex + 1] : nullptr;

		const int32 CurrentLodNumVerts = CurrentPhysMesh.Vertices.Num();

		ClothingMeshUtils::ClothMeshDesc CurrentMeshDesc(CurrentPhysMesh.Vertices, CurrentPhysMesh.Normals, CurrentPhysMesh.Indices);

		if(PrevLod)
		{
			FClothPhysicalMeshData& PrevPhysMesh = PrevLod->PhysicalMeshData;

			CurrentLod.TransitionUpSkinData.Empty(CurrentLodNumVerts);

			ClothingMeshUtils::ClothMeshDesc PrevMeshDesc(PrevPhysMesh.Vertices, PrevPhysMesh.Normals, PrevPhysMesh.Indices);

			ClothingMeshUtils::GenerateMeshToMeshSkinningData(CurrentLod.TransitionUpSkinData, CurrentMeshDesc, nullptr, PrevMeshDesc);
		}

		if(NextLod)
		{
			FClothPhysicalMeshData& NextPhysMesh = NextLod->PhysicalMeshData;

			CurrentLod.TransitionDownSkinData.Empty(CurrentLodNumVerts);

			ClothingMeshUtils::ClothMeshDesc NextMeshDesc(NextPhysMesh.Vertices, NextPhysMesh.Normals, NextPhysMesh.Indices);

			ClothingMeshUtils::GenerateMeshToMeshSkinningData(CurrentLod.TransitionDownSkinData, CurrentMeshDesc, nullptr, NextMeshDesc);
		}
	}
}

void UClothingAsset::ApplyParameterMasks()
{
	for(FClothLODData& Lod : LodData)
	{
		// First zero out the parameters, otherwise disabled masks might hang around
		Lod.PhysicalMeshData.ClearParticleParameters();

		for(FClothParameterMask_PhysMesh& Mask : Lod.ParameterMasks)
		{
			// Only apply enabled masks
			if(!Mask.bEnabled)
			{
				continue;
			}

			TArray<float>* TargetArray = nullptr;

			switch(Mask.CurrentTarget)
			{
				case MaskTarget_PhysMesh::BackstopDistance:
					TargetArray = &Lod.PhysicalMeshData.BackstopDistances;
					break;
				case MaskTarget_PhysMesh::BackstopRadius:
					TargetArray = &Lod.PhysicalMeshData.BackstopRadiuses;
					break;
				case MaskTarget_PhysMesh::MaxDistance:
					TargetArray = &Lod.PhysicalMeshData.MaxDistances;
					break;
				default:
					break;
			}

			if(TargetArray)
			{
				*TargetArray = Mask.GetValueArray();
			}
		}
	}

	InvalidateCachedData();
}

bool UClothingAsset::IsValidLod(int32 InLodIndex)
{
	return LodData.IsValidIndex(InLodIndex);
}

int32 UClothingAsset::GetNumLods()
{
	return LodData.Num();
}

#endif

void UClothingAsset::BuildSelfCollisionData()
{
	if(!ClothConfig.HasSelfCollision())
	{
		// No self collision, can't generate data
		return;
	}

	// can't pass through the network of other spheres.
	const float SCRadius = ClothConfig.SelfCollisionRadius * ClothConfig.SelfCollisionCullScale;
	const float SCRadiusSq = SCRadius * SCRadius;

	for(FClothLODData& Lod : LodData)
	{
		FClothPhysicalMeshData& PhysMesh = Lod.PhysicalMeshData;
		
		// Start with the full set
		const int32 NumVerts = PhysMesh.Vertices.Num();
		PhysMesh.SelfCollisionIndices.Reset();
		for(int32 Index = 0; Index < NumVerts; ++Index)
		{
			PhysMesh.SelfCollisionIndices.Add(Index);
		}

		// Strip any verts that are fixed from the beginning of the array so we always
		// start with a valid movable vertex
		int32 PrepassIndex = PhysMesh.SelfCollisionIndices[0];
		while(PhysMesh.MaxDistances[PrepassIndex] < SMALL_NUMBER)
		{
			PhysMesh.SelfCollisionIndices.RemoveAt(0);

			if(PhysMesh.SelfCollisionIndices.Num() > 0)
			{
				// Take index again
				PrepassIndex = PhysMesh.SelfCollisionIndices[0];
			}
			else
			{
				// We've cleared out the array, bail as we have no movable verts
				break;
			}
		}

		// Now start aggresively culling verts that are near others that we have accepted
		for(int32 Vert0Itr = 0; Vert0Itr < PhysMesh.SelfCollisionIndices.Num(); ++Vert0Itr)
		{
			uint32 V0Index = PhysMesh.SelfCollisionIndices[Vert0Itr];
			const FVector& V0Pos = PhysMesh.Vertices[V0Index];
			float MaxDist0 = PhysMesh.MaxDistances[V0Index];

			// Start one after our current V0, we've done the other checks
			for(int32 Vert1Itr = Vert0Itr + 1; Vert1Itr < PhysMesh.SelfCollisionIndices.Num(); )
			{
				uint32 V1Index = PhysMesh.SelfCollisionIndices[Vert1Itr];
				const FVector& V1Pos = PhysMesh.Vertices[V1Index];

				float V0ToV1DistSq = (V1Pos - V0Pos).SizeSquared();
				float MaxDist1 = PhysMesh.MaxDistances[V1Index];

				if(V0ToV1DistSq < SCRadiusSq || MaxDist1 < SMALL_NUMBER)
				{
					// Too close, remove it
					PhysMesh.SelfCollisionIndices.RemoveAt(Vert1Itr);
					continue;
				}
				else
				{
					// Move to next if we didn't remove
					++Vert1Itr;
				}
			}
		}
	}
}

void UClothingAsset::PostLoad()
{
	Super::PostLoad();

	BuildSelfCollisionData();

#if WITH_EDITORONLY_DATA
	CalculateReferenceBoneIndex();
#endif

	int32 CustomVersion = GetLinkerCustomVersion(FAnimPhysObjectVersion::GUID);

	if(CustomVersion < FAnimPhysObjectVersion::AddedClothingMaskWorkflow)
	{
#if WITH_EDITORONLY_DATA
		// Convert current parameters to masks
		for(FClothLODData& Lod : LodData)
		{
			FClothPhysicalMeshData& PhysMesh = Lod.PhysicalMeshData;

			// Didn't do anything previously - clear out incase there's something in there
			// so we can use it correctly now.
			Lod.ParameterMasks.Reset(3);

			// Max distances (Always present)
			Lod.ParameterMasks.AddDefaulted();
			FClothParameterMask_PhysMesh& MaxDistanceMask = Lod.ParameterMasks.Last();
			MaxDistanceMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::MaxDistance);
			MaxDistanceMask.bEnabled = true;

			// Following params are only added if necessary, if we don't have any backstop
			// radii then there's no backstops.
			if(PhysMesh.BackstopRadiuses.FindByPredicate([](const float& A) {return A != 0.0f; }))
			{
				// Backstop radii
				Lod.ParameterMasks.AddDefaulted();
				FClothParameterMask_PhysMesh& BackstopRadiusMask = Lod.ParameterMasks.Last();
				BackstopRadiusMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::BackstopRadius);
				BackstopRadiusMask.bEnabled = true;

				// Backstop distances
				Lod.ParameterMasks.AddDefaulted();
				FClothParameterMask_PhysMesh& BackstopDistanceMask = Lod.ParameterMasks.Last();
				BackstopDistanceMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::BackstopDistance);
				BackstopDistanceMask.bEnabled = true;
			}
			
		}
#endif

		// Make sure we're transactional
		SetFlags(RF_Transactional);
	}

#if WITH_EDITOR
	if(CustomVersion < FAnimPhysObjectVersion::CacheClothMeshInfluences)
	{
		// Rebuild data cache
		InvalidateCachedData();
	}
#endif

}

void UClothingAsset::CalculateReferenceBoneIndex()
{
	// Starts at root
	ReferenceBoneIndex = 0;

	// Find the root bone for this clothing asset (common bone for all used bones)
	typedef TArray<int32> BoneIndexArray;

	// List of valid paths to the root bone from each weighted bone
	TArray<BoneIndexArray> PathsToRoot;
	
	USkeletalMesh* OwnerMesh = Cast<USkeletalMesh>(GetOuter());

	if(OwnerMesh)
	{
		FReferenceSkeleton& RefSkel = OwnerMesh->RefSkeleton;
		// First build a list per used bone for it's path to root
		const int32 NumUsedBones = UsedBoneIndices.Num();

		// List of actually weighted (not just used) bones
		TArray<int32> WeightedBones;

		for(FClothLODData& CurLod : LodData)
		{
			FClothPhysicalMeshData& MeshData = CurLod.PhysicalMeshData;

			for(FClothVertBoneData& VertBoneData : MeshData.BoneData)
			{
				for(int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
				{
					if(VertBoneData.BoneWeights[InfluenceIndex] > SMALL_NUMBER)
					{
						WeightedBones.AddUnique(VertBoneData.BoneIndices[InfluenceIndex]);
					}
					else
					{
						// Hit the last weight (they're sorted)
						break;
					}
				}
			}
		}

		const int32 NumWeightedBones = WeightedBones.Num();
		PathsToRoot.Reserve(NumWeightedBones);
		
		// Compute paths to the root bone
		for(int32 WeightedBoneIndex = 0; WeightedBoneIndex < NumWeightedBones; ++WeightedBoneIndex)
		{
			PathsToRoot.AddDefaulted();
			BoneIndexArray& Path = PathsToRoot.Last();
			
			int32 CurrentBone = WeightedBones[WeightedBoneIndex];
			Path.Add(CurrentBone);
			
			while(CurrentBone != 0 && CurrentBone != INDEX_NONE)
			{
				CurrentBone = RefSkel.GetParentIndex(CurrentBone);
				Path.Add(CurrentBone);
			}
		}

		// Paths are from leaf->root, we want the other way
		for(BoneIndexArray& Path : PathsToRoot)
		{
			Algo::Reverse(Path);
		}

		// Verify the last common bone in all paths as the root of the sim space
		const int32 NumPaths = PathsToRoot.Num();
		if(NumPaths > 0)
		{
			BoneIndexArray& FirstPath = PathsToRoot[0];
		
			const int32 FirstPathSize = FirstPath.Num();
			for(int32 PathEntryIndex = 0; PathEntryIndex < FirstPathSize; ++PathEntryIndex)
			{
				const int32 CurrentQueryIndex = FirstPath[PathEntryIndex];
				bool bValidRoot = true;

				for(int32 PathIndex = 1; PathIndex < NumPaths; ++PathIndex)
				{
					if(!PathsToRoot[PathIndex].Contains(CurrentQueryIndex))
					{
						bValidRoot = false;
						break;
					}
				}

				if(bValidRoot)
				{
					ReferenceBoneIndex = CurrentQueryIndex;
				}
				else
				{
					// Once we fail to find a valid root we're done.
					break;
				}
			}
		}
		else
		{
			// Just use root
			ReferenceBoneIndex = 0;
		}
	}
}

#if WITH_EDITOR

void UClothingAsset::PostEditChangeChainProperty(FPropertyChangedChainEvent& InEvent)
{
	if(InEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if(InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UClothingAsset, PhysicsAsset))
		{
			HandlePhysicsAssetChange();
		}

		if(InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FClothConfig, SelfCollisionRadius) ||
			InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FClothConfig, SelfCollisionCullScale))
		{
			BuildSelfCollisionData();
		}
	}
}

void UClothingAsset::HandlePhysicsAssetChange()
{
	if(USkeletalMesh* OwnerMesh = Cast<USkeletalMesh>(GetOuter()))
	{
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if(USkeletalMeshComponent* Component = *It)
			{
				if(Component->SkeletalMesh == OwnerMesh)
				{
					FComponentReregisterContext Context(Component);
				}
			}
		}
	}
}

#endif

void UClothingAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);
}

void ClothingAssetUtils::GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings)
{
	OutBindings.Empty();

	if(!InSkelMesh)
	{
		return;
	}

	if(FSkeletalMeshResource* Resource = InSkelMesh->GetImportedResource())
	{
		const int32 NumLods = Resource->LODModels.Num();

		for(int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
		{
			TArray<FClothingAssetMeshBinding> LodBindings;
			GetMeshClothingAssetBindings(InSkelMesh, LodBindings, LodIndex);

			OutBindings.Append(LodBindings);
		}
	}
}

void ClothingAssetUtils::GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings, int32 InLodIndex)
{
	OutBindings.Empty();

	if(!InSkelMesh)
	{
		return;
	}

	if(FSkeletalMeshResource* Resource = InSkelMesh->GetImportedResource())
	{
		if(Resource->LODModels.IsValidIndex(InLodIndex))
		{
			FStaticLODModel& LodModel = Resource->LODModels[InLodIndex];

			const int32 NumSections = LodModel.Sections.Num();

			for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				FSkelMeshSection& Section = LodModel.Sections[SectionIndex];

				if(Section.bDisabled && Section.CorrespondClothSectionIndex != INDEX_NONE)
				{
					UClothingAsset* SectionAsset = Cast<UClothingAsset>(InSkelMesh->GetSectionClothingAsset(InLodIndex, SectionIndex));

					if(SectionAsset)
					{
						// This is the original section of a clothing section pair
						OutBindings.AddDefaulted();
						FClothingAssetMeshBinding& Binding = OutBindings.Last();

						Binding.Asset = SectionAsset;
						Binding.LODIndex = InLodIndex;
						Binding.SectionIndex = SectionIndex;
						Binding.AssetInternalLodIndex = Section.ClothingData.AssetLodIndex;
					}
				}
			}
		}
	}
}

bool FClothConfig::HasSelfCollision() const
{
	return SelfCollisionRadius > 0.0f && SelfCollisionStiffness > 0.0f;
}

void FClothCollisionData::Reset()
{
	Spheres.Reset();
	SphereConnections.Reset();
	Convexes.Reset();
}

void FClothCollisionData::Append(const FClothCollisionData& InOther)
{
	const int32 NumSpheresBefore = Spheres.Num();
	const int32 NumSphereConnectionsBefore = SphereConnections.Num();

	Spheres.Append(InOther.Spheres);
	SphereConnections.Append(InOther.SphereConnections);

	const int32 NumSphereConnectionsAfter = SphereConnections.Num();

	if(NumSpheresBefore > 0)
	{
		// Each connection that was added needs to have its sphere indices increased to match the new spheres that were added
		for(int32 NewConnectionIndex = NumSphereConnectionsBefore; NewConnectionIndex < NumSphereConnectionsAfter; ++NewConnectionIndex)
		{
			FClothCollisionPrim_SphereConnection& Connection = SphereConnections[NewConnectionIndex];
			Connection.SphereIndices[0] += NumSpheresBefore;
			Connection.SphereIndices[1] += NumSpheresBefore;
		}
	}

	Convexes.Append(InOther.Convexes);
}

void FClothPhysicalMeshData::Reset(const int32 InNumVerts)
{
	Vertices.Reset();
	Normals.Reset();
	MaxDistances.Reset();
	BackstopDistances.Reset();
	BackstopRadiuses.Reset();
	InverseMasses.Reset();
	BoneData.Reset();

	Vertices.AddDefaulted(InNumVerts);
	Normals.AddDefaulted(InNumVerts);
	MaxDistances.AddDefaulted(InNumVerts);
	BackstopDistances.AddDefaulted(InNumVerts);
	BackstopRadiuses.AddDefaulted(InNumVerts);
	InverseMasses.AddDefaulted(InNumVerts);
	BoneData.AddDefaulted(InNumVerts);

	MaxBoneWeights = 0;
	NumFixedVerts = 0;
}

void FClothPhysicalMeshData::ClearParticleParameters()
{
	// All float parameter arrays can just be memzeroed
	FMemory::Memzero(MaxDistances.GetData(), MaxDistances.GetAllocatedSize());
	FMemory::Memzero(BackstopDistances.GetData(), BackstopDistances.GetAllocatedSize());
	FMemory::Memzero(BackstopRadiuses.GetData(), BackstopRadiuses.GetAllocatedSize());
}

void FClothParameterMask_PhysMesh::Initialize(const FClothPhysicalMeshData& InMeshData)
{
	const int32 NumVerts = InMeshData.Vertices.Num();

	// Set up value array
	Values.Reset(NumVerts);
	Values.AddZeroed(NumVerts);

	bEnabled = false;
}

void FClothParameterMask_PhysMesh::CopyFromPhysMesh(const FClothPhysicalMeshData& InMeshData, MaskTarget_PhysMesh InTarget)
{
	// Presize value arrays
	Initialize(InMeshData);

	// Set our target
	CurrentTarget = InTarget;

	// Copy the actual parameter data
	switch(InTarget)
	{
		case MaskTarget_PhysMesh::BackstopDistance:
			Values = InMeshData.BackstopDistances;
			break;
		case MaskTarget_PhysMesh::BackstopRadius:
			Values = InMeshData.BackstopRadiuses;
			break;
		case MaskTarget_PhysMesh::MaxDistance:
			Values = InMeshData.MaxDistances;
			break;
		default:
			break;
	}

	CalcRanges();
}

void FClothParameterMask_PhysMesh::SetValue(int32 InVertexIndex, float InValue)
{
	if(InVertexIndex < Values.Num())
	{
		Values[InVertexIndex] = InValue;

		CalcRanges();
	}
}

float FClothParameterMask_PhysMesh::GetValue(int32 InVertexIndex) const
{
	return InVertexIndex < Values.Num() ? Values[InVertexIndex] : 0.0f;
}

const TArray<float>& FClothParameterMask_PhysMesh::GetValueArray() const
{
	return Values;
}

void FClothParameterMask_PhysMesh::CalcRanges()
{
	MinValue = MAX_flt;
	MaxValue = -MinValue;

	for(const float& Value : Values)
	{
		MaxValue = FMath::Max(Value, MaxValue);
		MinValue = FMath::Min(Value, MinValue);
	}
}

#if WITH_EDITOR

FColor FClothParameterMask_PhysMesh::GetValueAsColor(int32 InVertexIndex) const
{
	if(InVertexIndex < Values.Num())
	{
		const float& Value = Values[InVertexIndex];

		if(Value == 0.0f)
		{
			return FColor::Magenta;
		}

		uint8 ScaledValue = uint8(((Value - MinValue) / (MaxValue - MinValue)) * 255.0f);
		return FColor(ScaledValue, ScaledValue, ScaledValue);
	}

	return FColor::Red;
}
#endif

void FClothParameterMask_PhysMesh::Apply(FClothPhysicalMeshData& InTargetMesh)
{
	if(CurrentTarget == MaskTarget_PhysMesh::None)
	{
		// Nothing to do here, just return
		return;
	}

	const int32 NumValues = Values.Num();
	const int32 NumTargetMeshVerts = InTargetMesh.Vertices.Num();

	if(NumTargetMeshVerts == NumValues)
	{
		TArray<float>* TargetArray = nullptr;
		switch(CurrentTarget)
		{
			case MaskTarget_PhysMesh::MaxDistance:
				TargetArray = &InTargetMesh.MaxDistances;
				break;
			case MaskTarget_PhysMesh::BackstopDistance:
				TargetArray = &InTargetMesh.BackstopDistances;
				break;
			case MaskTarget_PhysMesh::BackstopRadius:
				TargetArray = &InTargetMesh.BackstopRadiuses;
				break;
			default:
				break;
		}

		if(!TargetArray)
		{
			return;
		}

		check((*TargetArray).Num() == NumValues);

		for(int32 Index = 0; Index < NumTargetMeshVerts; ++Index)
		{
			(*TargetArray)[Index] = Values[Index];
		}
	}
	else
	{
		UE_LOG(LogClothingAsset, Warning, TEXT("Aborted applying mask to physical mesh at %p, value mismatch (NumValues: %d, NumVerts: %d)."), &InTargetMesh, NumValues, NumTargetMeshVerts);
	}
}

#if WITH_EDITORONLY_DATA

void FClothLODData::GetParameterMasksForTarget(const MaskTarget_PhysMesh& InTarget, TArray<FClothParameterMask_PhysMesh*>& OutMasks)
{
	for(FClothParameterMask_PhysMesh& Mask : ParameterMasks)
	{
		if(Mask.CurrentTarget == InTarget)
		{
			OutMasks.Add(&Mask);
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE
