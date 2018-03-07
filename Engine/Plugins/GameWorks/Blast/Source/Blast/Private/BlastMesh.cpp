#include "BlastMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysXPublic.h"
#if WITH_EDITOR
#include "RawMesh.h"
#include "RawIndexBuffer.h"
#endif

#define LOCTEXT_NAMESPACE "Blast"

UBlastMesh::UBlastMesh(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer),
	Mesh(nullptr),
	Skeleton(nullptr),
	PhysicsAsset(nullptr)
{

}

bool UBlastMesh::IsValidBlastMesh()
{
	return Mesh != nullptr && PhysicsAsset != nullptr && GetLoadedAsset() != nullptr;
}

#if WITH_EDITOR

static FName NAME_Mesh = GET_MEMBER_NAME_CHECKED(UBlastMesh, Mesh);

void UBlastMesh::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif


void UBlastMesh::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITORONLY_DATA
	//This is used by the reimport code to find the AssetImportData
	if (AssetImportData)
	{
		OutTags.Emplace(UObject::SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden);
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

void UBlastMesh::PostLoad()
{
	Super::PostLoad();

	//Make sure our instanced sub objects have run PostLoad so they are fully initialized before we use them
	if (Mesh)
	{
		Mesh->ConditionalPostLoad();
	}

	if (PhysicsAsset)
	{
		PhysicsAsset->ConditionalPostLoad();
	}

	if (Skeleton)
	{
		Skeleton->ConditionalPostLoad();
	}

	RebuildIndexToBoneNameMap();
#if WITH_EDITOR
	RebuildCookedBodySetupsIfRequired();

	if (Mesh)
	{
		// Old mesh which doesn't contain these
		if (Mesh->GetIndexBufferRanges().Num() == 0)
		{
			Mesh->RebuildIndexBufferRanges();
		}

		for (int32 I = 0; I < Mesh->Materials.Num(); I++)
		{
			//Fix up files where this is null from the old import/fracture code
			FSkeletalMaterial& Mat = Mesh->Materials[I];
			if (Mat.MaterialSlotName.IsNone() && Mat.ImportedMaterialSlotName.IsNone())
			{
				Mat.ImportedMaterialSlotName = FName("MaterialSlot", I);
			}

			if (Mat.MaterialSlotName.IsNone())
			{
				Mat.MaterialSlotName = Mat.ImportedMaterialSlotName;
			}
			else if (Mat.ImportedMaterialSlotName.IsNone())
			{
				Mat.ImportedMaterialSlotName = Mat.MaterialSlotName;
			}
		}
	}
#endif
}

void UBlastMesh::PreSave(const class ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	//Since can only do this in the editor, just make 100% sure this up to date if we are cooking
	RebuildCookedBodySetupsIfRequired();
#endif
	Super::PreSave(TargetPlatform);
}

void UBlastMesh::RebuildIndexToBoneNameMap()
{
	// Building chunk to bone maps
	if (Mesh != nullptr)
	{
		const uint32 ChunkCount = GetChunkCount();
		const auto& BoneNames = GetChunkIndexToBoneName();

		ChunkIndexToBoneIndex.SetNum(ChunkCount);
		for (uint32 i = 0; i < ChunkCount; i++)
		{
			FName boneName = BoneNames[i];
			int32 boneIndex = Mesh->RefSkeleton.FindBoneIndex(boneName);
			ChunkIndexToBoneIndex[i] = boneIndex;
		}
	}
	else
	{
		ChunkIndexToBoneIndex.Empty();
	}
}

#if WITH_EDITOR
void UBlastMesh::RebuildCookedBodySetupsIfRequired(bool bForceRebuild)
{
	int32 BoneCount = IsValidBlastMesh() ? Mesh->RefSkeleton.GetRawBoneNum() : 0;
	if (bForceRebuild || BoneCount != ComponentSpaceInitialBoneTransforms.Num())
	{
		Mesh->CalculateInvRefMatrices(); //will do nothing if already cached
		ComponentSpaceInitialBoneTransforms.SetNum(BoneCount);
		for (int32 B = 0; B < BoneCount; B++)
		{
			ComponentSpaceInitialBoneTransforms[B] = FTransform(Mesh->GetComposedRefPoseMatrix(B));
		}
	}

	int32 ChunkCount = IsValidBlastMesh() ? GetChunkCount() : 0;
	if (CookedChunkData.Num() != ChunkCount)
	{
		CookedChunkData.SetNum(ChunkCount);
	}
	
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
	{
		FBlastCookedChunkData& CurCookedChunkData = CookedChunkData[ChunkIndex];
		int32 BoneIndex = ChunkIndexToBoneIndex[ChunkIndex];
		//Would be nice to remove the Index -> Name -> Index lookup, but the PhysicsAsset seems to require it
		const uint32 BodySetupIndex = PhysicsAsset->FindBodyIndex(Mesh->RefSkeleton.GetBoneName(BoneIndex));

		if (PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodySetupIndex))
		{
			//Transform these ahead of time and cache since InitialBoneTransform is constant
			//Always make the initial actor at the component space origin, this allows the actor space to correspond to the at-rest position which Blast internally uses
			UBodySetup*  PhysicsAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodySetupIndex];
			//Whenever this setup is changed the guid is changed
			if (bForceRebuild || CurCookedChunkData.SourceBodySetupGUID != PhysicsAssetBodySetup->BodySetupGuid)
			{
				//rebuild this one
				UBodySetup* CookedTransformedBodySetup = NewObject<UBodySetup>(this);
				//Copy the settings, but not the actual colliders
				CookedTransformedBodySetup->CopyBodySetupProperty(PhysicsAssetBodySetup);
				//We are on the root bone now
				CookedTransformedBodySetup->BoneName = NAME_None;

				//Copy the bodies, transforming them into bone-space
				const FTransform InitialBoneTransform = GetComponentSpaceInitialBoneTransform(BoneIndex);
				const FKAggregateGeom& SrcAggGeom = PhysicsAssetBodySetup->AggGeom;
				FKAggregateGeom& DestAggGeom = CookedTransformedBodySetup->AggGeom;

				DestAggGeom.SphereElems.Reset(SrcAggGeom.SphereElems.Num());
				for (auto& E : SrcAggGeom.SphereElems)
				{
					DestAggGeom.SphereElems.Emplace(E.GetFinalScaled(FVector(1.0f), InitialBoneTransform));
				}
				DestAggGeom.BoxElems.Reset(SrcAggGeom.BoxElems.Num());
				for (auto& E : SrcAggGeom.BoxElems)
				{
					DestAggGeom.BoxElems.Emplace(E.GetFinalScaled(FVector(1.0f), InitialBoneTransform));
				}
				DestAggGeom.SphylElems.Reset(SrcAggGeom.SphylElems.Num());
				for (auto& E : SrcAggGeom.SphylElems)
				{
					DestAggGeom.SphylElems.Emplace(E.GetFinalScaled(FVector(1.0f), InitialBoneTransform));
				}

				const int32 ConvexCount = SrcAggGeom.ConvexElems.Num();
				DestAggGeom.ConvexElems.Reset(ConvexCount);
				for (int32 C = 0; C < ConvexCount; C++)
				{
					DestAggGeom.ConvexElems.Emplace(SrcAggGeom.ConvexElems[C]);
					auto& Last = DestAggGeom.ConvexElems.Last();

					Last.SetTransform(Last.GetTransform() * InitialBoneTransform);
					//This is not strictly required, but why not for simplicity?
					Last.BakeTransformToVerts();
				}

				CookedTransformedBodySetup->CreatePhysicsMeshes();

				CurCookedChunkData.CookedBodySetup = CookedTransformedBodySetup;
				CurCookedChunkData.SourceBodySetupGUID = PhysicsAssetBodySetup->BodySetupGuid;
			}
		}
		else
		{
			//Clear out this entry
			CurCookedChunkData.SourceBodySetupGUID = FGuid();
			CurCookedChunkData.CookedBodySetup = nullptr;
		}
	}
}

void UBlastMesh::GetRenderMesh(int32 LODIndex, TArray<FRawMesh>& RawMeshes)
{
	FSkeletalMeshResource* Resource = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!Resource || !Resource->LODModels.IsValidIndex(LODIndex))
	{
		return;
	}

	const int32 ChunkCount = GetChunkCount();
	TArray<uint32> TempBuffer;
	TArray<int32> BoneIndexToChunkIndex;
	TMap<int32, int32> newIndicesMap;
	BoneIndexToChunkIndex.SetNum(Mesh->RefSkeleton.GetNum());
	for (int32 I = 0; I < BoneIndexToChunkIndex.Num(); I++)
	{
		BoneIndexToChunkIndex[I] = INDEX_NONE;
	}
	for (int32 I = 0; I < ChunkCount; I++)
	{
		BoneIndexToChunkIndex[ChunkIndexToBoneIndex[I]] = I;
	}


	const FSkeletalMeshLODInfo& SrcLODInfo = Mesh->LODInfo[LODIndex];
	const FStaticLODModel& StaticLODModel = Resource->LODModels[LODIndex];

	if (!StaticLODModel.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		return;
	}

	TArray<FSoftSkinVertex> MeshVerts;
	StaticLODModel.GetVertices(MeshVerts);

	const uint32 NumTexCoords = FMath::Min(StaticLODModel.VertexBufferGPUSkin.GetNumTexCoords(), (uint32)MAX_MESH_TEXTURE_COORDS);
	const int32 NumSections = StaticLODModel.Sections.Num();
	const FRawStaticIndexBuffer16or32Interface& IndexBuffer = *StaticLODModel.MultiSizeIndexContainer.GetIndexBuffer();

	for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
	{
		const FSkelMeshSection& SkelMeshSection = StaticLODModel.Sections[SectionIndex];
		if (!SkelMeshSection.bDisabled)
		{
			int32 MaterialIndex = SkelMeshSection.MaterialIndex;
			// use the remapping of material indices for all LODs besides the base LOD 
			if (LODIndex > 0 && SrcLODInfo.LODMaterialMap.IsValidIndex(SkelMeshSection.MaterialIndex))
			{
				MaterialIndex = FMath::Clamp<int32>(SrcLODInfo.LODMaterialMap[SkelMeshSection.MaterialIndex], 0, Mesh->Materials.Num());
			}

			// Build 'wedge' info
			const int32 NumTriangles = SkelMeshSection.NumTriangles;
			for (int32 TriIndex = 0; TriIndex < NumTriangles; TriIndex++)
			{
				int32 ChunkIndex = INDEX_NONE;
				for (int32 WedgeIndex = 0; WedgeIndex < 3; WedgeIndex++)
				{
					const int32 VertexIndexForWedge = IndexBuffer.Get(SkelMeshSection.BaseIndex + TriIndex * 3 + WedgeIndex);
					const FSoftSkinVertex& SkinnedVertex = MeshVerts[VertexIndexForWedge];

					uint8 BoneIndex;
					if (SkinnedVertex.GetRigidWeightBone(BoneIndex))
					{
						ChunkIndex = BoneIndexToChunkIndex[SkelMeshSection.BoneMap[BoneIndex]];
						break;
					}
				}

				if (ChunkIndex == INDEX_NONE || ChunkIndex >= RawMeshes.Num())
				{
					continue;
				}
				auto& RawMesh = RawMeshes[ChunkIndex];
				TMap<int32, int32> SkelToChunkMeshVertIdMap;

				// copy face info
				RawMesh.FaceMaterialIndices.Add(MaterialIndex);
				//Leave empty since the skeletal mesh code doesn't save this
				//RawMesh.FaceSmoothingMasks.Add(-1); // Assume this is ignored as bRecomputeNormals is false

				for (int32 WedgeIndex = 0; WedgeIndex < 3; WedgeIndex++)
				{
					const int32 VertexIndexForWedge = IndexBuffer.Get(SkelMeshSection.BaseIndex + TriIndex * 3 + WedgeIndex);
					const FSoftSkinVertex& SkinnedVertex = MeshVerts[VertexIndexForWedge];
					int32* ChunkVertexIndex = SkelToChunkMeshVertIdMap.Find(VertexIndexForWedge);
					if (ChunkVertexIndex == nullptr)
					{
						SkelToChunkMeshVertIdMap.Add(VertexIndexForWedge, RawMesh.VertexPositions.Num());
						RawMesh.WedgeIndices.Add(RawMesh.VertexPositions.Num());
						RawMesh.VertexPositions.Add(SkinnedVertex.Position);
					}
					else
					{
						RawMesh.WedgeIndices.Add(*ChunkVertexIndex);
					}
					RawMesh.WedgeTangentX.Add(SkinnedVertex.TangentX);
					RawMesh.WedgeTangentY.Add(SkinnedVertex.TangentY);
					RawMesh.WedgeTangentZ.Add(SkinnedVertex.TangentZ);

					for (uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++)
					{
						if (TexCoordIndex >= NumTexCoords)
						{
							RawMesh.WedgeTexCoords[TexCoordIndex].AddDefaulted();
						}
						else
						{
							RawMesh.WedgeTexCoords[TexCoordIndex].Add(SkinnedVertex.UVs[TexCoordIndex]);
						}
					}

					if (StaticLODModel.ColorVertexBuffer.IsInitialized())
					{
						RawMesh.WedgeColors.Add(SkinnedVertex.Color);
					}
					else
					{
						RawMesh.WedgeColors.Add(FColor::White);
					}
				}
			}
		}
	}
}

#endif

const TArray<FName>& UBlastMesh::GetChunkIndexToBoneName()
{
	const uint32 ChunkCount = GetChunkCount();
	if (ChunkIndexToBoneName.Num() != ChunkCount)
	{
		ChunkIndexToBoneName.SetNum(ChunkCount);
		for (uint32 i = 0; i < ChunkCount; i++)
		{
			ChunkIndexToBoneName[i] = GetDefaultChunkBoneNameFromIndex(i);
		}
		MarkPackageDirty();
	}
	return ChunkIndexToBoneName;
}


const TArray<FBlastCookedChunkData>& UBlastMesh::GetCookedChunkData()
{
#if WITH_EDITOR
	//Maybe not the best to check this every time, but it's only in the editor
	RebuildCookedBodySetupsIfRequired();
#endif
	return CookedChunkData;
}


const TArray<FBlastCookedChunkData>& UBlastMesh::GetCookedChunkData_AssumeUpToDate() const
{
	return CookedChunkData;
}

const FString UBlastMesh::ChunkPrefix = TEXT("chunk_");

FName UBlastMesh::GetDefaultChunkBoneNameFromIndex(int32 ChunkIndex)
{
	return FName(*FString::Printf(TEXT("chunk_%d"), ChunkIndex));
}


void FBlastCookedChunkData::PopulateBodySetup(UBodySetup* NewBodySetup) const
{
	//These should already be null but just incase
	NewBodySetup->ClearPhysicsMeshes();

	//Make sure they are loaded
	CookedBodySetup->CreatePhysicsMeshes();

	//The assignment operators clear these so make sure we cache them before we touch the arrays
	ConvexMeshTempList ConvexMeshes;
	ConvexMeshTempList MirroredConvexMeshes;
	for (auto& C : CookedBodySetup->AggGeom.ConvexElems)
	{
		physx::PxConvexMesh* Mesh = C.GetConvexMesh();
		if (Mesh)
		{
			Mesh->acquireReference();
		}
		ConvexMeshes.Add(Mesh);
		Mesh = C.GetMirroredConvexMesh();
		if (Mesh)
		{
			Mesh->acquireReference();
		}
		MirroredConvexMeshes.Add(Mesh);
	}

	NewBodySetup->CopyBodyPropertiesFrom(CookedBodySetup);

	UpdateAfterShapesAdded(NewBodySetup, ConvexMeshes, MirroredConvexMeshes);
}

void FBlastCookedChunkData::AppendToBodySetup(UBodySetup* NewBodySetup) const 
{
	//Make sure they are loaded
	CookedBodySetup->CreatePhysicsMeshes();

	//The assignment operators clear these so make sure we cache them before we touch the arrays
	ConvexMeshTempList ConvexMeshes;
	ConvexMeshTempList MirroredConvexMeshes;
	for (auto& C : NewBodySetup->AggGeom.ConvexElems)
	{
		//Already add-refed these before
		ConvexMeshes.Add(C.GetConvexMesh());
		MirroredConvexMeshes.Add(C.GetMirroredConvexMesh());
	}

	for (auto& C : CookedBodySetup->AggGeom.ConvexElems)
	{
		physx::PxConvexMesh* Mesh = C.GetConvexMesh();
		if (Mesh)
		{
			Mesh->acquireReference();
		}
		ConvexMeshes.Add(Mesh);
		Mesh = C.GetMirroredConvexMesh();
		if (Mesh)
		{
			Mesh->acquireReference();
		}
		MirroredConvexMeshes.Add(Mesh);
	}

	//Should we check the PhysicalMaterial, etc are the same
	NewBodySetup->AddCollisionFrom(CookedBodySetup);

	UpdateAfterShapesAdded(NewBodySetup, ConvexMeshes, MirroredConvexMeshes);
}

void FBlastCookedChunkData::UpdateAfterShapesAdded(UBodySetup* NewBodySetup, ConvexMeshTempList& ConvexMeshes, ConvexMeshTempList& MirroredConvexMeshes) const
{
	//Always make sure these get set since they are cleared on copy
	bool bAllThere = true;
	const int32 ConvexCount = NewBodySetup->AggGeom.ConvexElems.Num();
	for (int32 C = 0; C < ConvexCount; C++)
	{
		auto& New = NewBodySetup->AggGeom.ConvexElems[C];
		bAllThere &= (ConvexMeshes[C] != nullptr &&  MirroredConvexMeshes[C] != nullptr);

		New.SetConvexMesh(ConvexMeshes[C]);
		New.SetMirroredConvexMesh(MirroredConvexMeshes[C]);
	}

	//If any are missing we need to fallback to runtime cooking
	NewBodySetup->bCreatedPhysicsMeshes = bAllThere;
}

#undef LOCTEXT_NAMESPACE
