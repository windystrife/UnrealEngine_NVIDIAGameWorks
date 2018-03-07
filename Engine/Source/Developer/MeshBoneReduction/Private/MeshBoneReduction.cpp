// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshBoneReduction.h"
#include "Modules/ModuleManager.h"
#include "GPUSkinPublicDefs.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "UObject/UObjectHash.h"
#include "Templates/ScopedPointer.h"
#include "ComponentReregisterContext.h"
#include "UniquePtr.h"
#include "SkeletalMeshTypes.h"
#include "AnimationBlueprintLibrary.h"
#include "Async/ParallelFor.h"

class FMeshBoneReductionModule : public IMeshBoneReductionModule
{
public:
	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshBoneReductionModule interface.
	virtual class IMeshBoneReduction* GetMeshBoneReductionInterface() override;
};

DEFINE_LOG_CATEGORY_STATIC(LogMeshBoneReduction, Log, All);
IMPLEMENT_MODULE(FMeshBoneReductionModule, MeshBoneReduction);

class FMeshBoneReduction : public IMeshBoneReduction
{
public:

	virtual ~FMeshBoneReduction()
	{
	}

	void EnsureChildrenPresents(FBoneIndexType BoneIndex, const TArray<FMeshBoneInfo>& RefBoneInfo, TArray<FBoneIndexType>& OutBoneIndicesToRemove)
	{
		// just look for direct parent, we could look for RefBoneInfo->Ischild, but more expensive, and no reason to do that all the work
		for (int32 ChildBoneIndex = 0; ChildBoneIndex < RefBoneInfo.Num(); ++ChildBoneIndex)
		{
			if (RefBoneInfo[ChildBoneIndex].ParentIndex == BoneIndex)
			{
				OutBoneIndicesToRemove.AddUnique(ChildBoneIndex);
				EnsureChildrenPresents(ChildBoneIndex, RefBoneInfo, OutBoneIndicesToRemove);
			}
		}
	}

	bool GetBoneReductionData(const USkeletalMesh* SkeletalMesh, int32 DesiredLOD, TMap<FBoneIndexType, FBoneIndexType>& OutBonesToReplace, const TArray<FName>* BoneNamesToRemove = NULL) override
	{
		if (!SkeletalMesh)
		{
			return false;
		}

		if (!SkeletalMesh->LODInfo.IsValidIndex(DesiredLOD))
		{
			return false;
		}

		const TArray<FMeshBoneInfo> & RefBoneInfo = SkeletalMesh->RefSkeleton.GetRefBoneInfo();
		TArray<FBoneIndexType> BoneIndicesToRemove;

		// originally this code was accumulating from LOD 0->DesiredLOd, but that should be done outside of tool if they want to
		// removing it, and just include DesiredLOD
		{
			// if name is entered, use them instead of setting
			const TArray<FName>& BonesToRemoveSetting = [BoneNamesToRemove, SkeletalMesh, DesiredLOD]()
			{
				if (BoneNamesToRemove)
				{
					return *BoneNamesToRemove;
				}
				else
				{
					TArray<FName> RetrievedNames;
					for (const FBoneReference& BoneReference : SkeletalMesh->LODInfo[DesiredLOD].BonesToRemove)
					{
						RetrievedNames.AddUnique(BoneReference.BoneName);
					}
					RetrievedNames.Remove(NAME_None);

					return RetrievedNames;
				}
			}();

			// first gather indices. we don't want to add bones to replace if that "to-be-replace" will be removed as well
			for (int32 Index = 0; Index < BonesToRemoveSetting.Num(); ++Index)
			{
				if (BonesToRemoveSetting[Index] != NAME_None)
				{
					int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(BonesToRemoveSetting[Index]);

					// we don't allow root to be removed
					if (BoneIndex > 0)
					{
						BoneIndicesToRemove.AddUnique(BoneIndex);
						// make sure all children for this joint is included
						EnsureChildrenPresents(BoneIndex, RefBoneInfo, BoneIndicesToRemove);
					}
				}
			
			}
		}

		if (BoneIndicesToRemove.Num() <= 0)
		{
			return false;
		}

		// now make sure the parent isn't the one to be removed, find the one that won't be removed
		for (int32 Index = 0; Index < BoneIndicesToRemove.Num(); ++Index)
		{
			int32 BoneIndex = BoneIndicesToRemove[Index];
			int32 ParentIndex = RefBoneInfo[BoneIndex].ParentIndex;

			while (BoneIndicesToRemove.Contains(ParentIndex))
			{
				ParentIndex = RefBoneInfo[ParentIndex].ParentIndex;
			}

			OutBonesToReplace.Add(BoneIndex, ParentIndex);
		}

		return ( OutBonesToReplace.Num() > 0 );
	}

	void FixUpSectionBoneMaps( FSkelMeshSection & Section, const TMap<FBoneIndexType, FBoneIndexType> &BonesToRepair ) override
	{
		// now you have list of bones, remove them from vertex influences
		{
			TMap<uint8, uint8> BoneMapRemapTable;
			// first go through bone map and see if this contains BonesToRemove
			int32 BoneMapSize = Section.BoneMap.Num();
			int32 AdjustIndex=0;

			for (int32 BoneMapIndex=0; BoneMapIndex < BoneMapSize; ++BoneMapIndex )
			{
				// look for this bone to be removed or not?
				const FBoneIndexType* ParentBoneIndex = BonesToRepair.Find(Section.BoneMap[BoneMapIndex]);
				if ( ParentBoneIndex  )
				{
					// this should not happen, I don't ever remove root
					check (*ParentBoneIndex!=INDEX_NONE);

					// if Parent already exists in the current BoneMap, we just have to fix up the mapping
					int32 ParentBoneMapIndex = Section.BoneMap.Find(*ParentBoneIndex);

					// if it exists
					if (ParentBoneMapIndex != INDEX_NONE)
					{
						// if parent index is higher, we have to decrease it to match to new index
						if (ParentBoneMapIndex > BoneMapIndex)
						{
							--ParentBoneMapIndex;
						}

						// remove current section count, will replace with parent
						Section.BoneMap.RemoveAt(BoneMapIndex);
					}
					else
					{
						// if parent doens't exists, we have to add one
						// this doesn't change bone map size 
						Section.BoneMap.RemoveAt(BoneMapIndex);
						ParentBoneMapIndex = Section.BoneMap.Add(*ParentBoneIndex);
					}

					// first fix up all indices of BoneMapRemapTable for the indices higher than BoneMapIndex, since BoneMapIndex is being removed
					for (auto Iter = BoneMapRemapTable.CreateIterator(); Iter; ++Iter)
					{
						uint8& Value = Iter.Value();

						check (Value != BoneMapIndex);
						if (Value > BoneMapIndex)
						{
							--Value;
						}
					}

					int32 OldIndex = BoneMapIndex+AdjustIndex;
					int32 NewIndex = ParentBoneMapIndex;
					// you still have to add no matter what even if same since indices might change after added
					{
						// add to remap table
						check (OldIndex < 256 && OldIndex >= 0);
						check (NewIndex < 256 && NewIndex >= 0);
						check (BoneMapRemapTable.Contains((uint8)OldIndex) == false);
						BoneMapRemapTable.Add((uint8)OldIndex, (uint8)NewIndex);
					}

					// reduce index since the item is removed
					--BoneMapIndex;
					--BoneMapSize;

					// this is to adjust the later indices. We need to refix their indices
					++AdjustIndex;
				}
				else if (AdjustIndex > 0)
				{
					int32 OldIndex = BoneMapIndex+AdjustIndex;
					int32 NewIndex = BoneMapIndex;

					check (OldIndex < 256 && OldIndex >= 0);
					check (NewIndex < 256 && NewIndex >= 0);
					check (BoneMapRemapTable.Contains((uint8)OldIndex) == false);
					BoneMapRemapTable.Add((uint8)OldIndex, (uint8)NewIndex);
				}
			}

			if ( BoneMapRemapTable.Num() > 0 )
			{
				// fix up soft verts
				for (int32 VertIndex=0; VertIndex < Section.SoftVertices.Num(); ++VertIndex)
				{
					FSoftSkinVertex & Vert = Section.SoftVertices[VertIndex];
					bool ShouldRenormalize = false;

					for(int32 InfluenceIndex = 0;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
					{
						uint8 *RemappedBone = BoneMapRemapTable.Find(Vert.InfluenceBones[InfluenceIndex]);
						if (RemappedBone)
						{
							Vert.InfluenceBones[InfluenceIndex] = *RemappedBone;
							ShouldRenormalize = true;
						}
					}

					if (ShouldRenormalize)
					{
						// should see if same bone exists
						for(int32 InfluenceIndex = 0;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
						{
							for(int32 InfluenceIndex2 = InfluenceIndex+1;InfluenceIndex2 < MAX_TOTAL_INFLUENCES;InfluenceIndex2++)
							{
								// cannot be 0 because we don't allow removing root
								if (Vert.InfluenceBones[InfluenceIndex] != 0 && Vert.InfluenceBones[InfluenceIndex] == Vert.InfluenceBones[InfluenceIndex2])
								{
									Vert.InfluenceWeights[InfluenceIndex] += Vert.InfluenceWeights[InfluenceIndex2];
									// reset
									Vert.InfluenceBones[InfluenceIndex2] = 0;
									Vert.InfluenceWeights[InfluenceIndex2] = 0;
								}
							}
						}
					}
				}
			}
		}
	}

	void RetrieveBoneTransforms(USkeletalMesh* SkeletalMesh, const int32 LODIndex, TArray<FBoneIndexType>& BonesToRemove, TArray<FTransform>& InOutTransforms)
	{
		if (!SkeletalMesh->LODInfo.IsValidIndex(LODIndex))
		{
			return;
		}

		// Retrieve all bone names in skeleton
		TArray<FName> BoneNames;
		const int32 NumBones = SkeletalMesh->RefSkeleton.GetNum();
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			BoneNames.Add(SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex));
		}
				
		TArray<FTransform> MultipliedBonePoses;
		const UAnimSequence* BakePose = SkeletalMesh->LODInfo[LODIndex].BakePose;
		if (BakePose)
		{
			// Retrieve posed bone transforms
			TArray<FTransform> BonePoses;
			UAnimationBlueprintLibrary::GetBonePosesForFrame(BakePose, BoneNames, 0, true, BonePoses);
			MultipliedBonePoses.AddDefaulted(BonePoses.Num());

			// Retrieve ref pose bone transforms
			TArray<FTransform> RefBonePoses = SkeletalMesh->RefSkeleton.GetRawRefBonePose();
			TArray<FTransform> MultipliedRefBonePoses;
			MultipliedRefBonePoses.AddDefaulted(RefBonePoses.Num());

			const bool bDebugBonePoses = false;

			TArray<int32> Processed;
			Processed.SetNumZeroed(RefBonePoses.Num());
			// Multiply out parent to child transforms for ref-pose
			for (int32 BonePoseIndex = 0; BonePoseIndex < RefBonePoses.Num(); BonePoseIndex++)
			{
				const int32 BoneIndex = SkeletalMesh->RefSkeleton.FindRawBoneIndex(BoneNames[BonePoseIndex]);
				const int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
				MultipliedRefBonePoses[BoneIndex] = RefBonePoses[BoneIndex];

				if (ParentIndex != INDEX_NONE)
				{
					checkf(ParentIndex == 0 || Processed[ParentIndex] == 1, TEXT("Parent bone was not yet processed"));
					UE_CLOG(bDebugBonePoses, LogMeshBoneReduction, Log, TEXT("Original: [%i]\n%s"), BonePoseIndex, *RefBonePoses[BoneIndex].ToHumanReadableString());

					MultipliedRefBonePoses[BoneIndex] = MultipliedRefBonePoses[BoneIndex] * MultipliedRefBonePoses[ParentIndex];
					MultipliedRefBonePoses[BoneIndex].NormalizeRotation();

					UE_CLOG(bDebugBonePoses, LogMeshBoneReduction, Log, TEXT("Relative: [%i]\n%s"), BonePoseIndex, *MultipliedRefBonePoses[BoneIndex].ToHumanReadableString());
					checkSlow(MultipliedRefBonePoses[BoneIndex].IsRotationNormalized());
					checkSlow(!MultipliedRefBonePoses[BoneIndex].ContainsNaN());
				}
				

				Processed[BoneIndex] = 1;
			}

			Processed.Empty();
			Processed.SetNumZeroed(BonePoses.Num());
			// Multiply out parent to child transforms for bake-pose
			for (int32 BonePoseIndex = 0; BonePoseIndex < BonePoses.Num(); BonePoseIndex++)
			{
				const int32 BoneIndex = SkeletalMesh->RefSkeleton.FindRawBoneIndex(BoneNames[BonePoseIndex]);
				const int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
				MultipliedBonePoses[BoneIndex] = BonePoses[BoneIndex];
				if (ParentIndex != INDEX_NONE)
				{
					checkf(ParentIndex == 0 || Processed[ParentIndex] == 1, TEXT("Parent bone was not yet processed"));
					UE_CLOG(bDebugBonePoses, LogMeshBoneReduction, Log, TEXT("Original: [%i]\n%s"), BonePoseIndex, *BonePoses[BoneIndex].ToHumanReadableString());

					MultipliedBonePoses[BoneIndex] = MultipliedBonePoses[BoneIndex] * MultipliedBonePoses[ParentIndex];
					MultipliedBonePoses[BoneIndex].NormalizeRotation();

					UE_CLOG(bDebugBonePoses, LogMeshBoneReduction, Log, TEXT("Relative: [%i]\n%s"), BonePoseIndex, *MultipliedBonePoses[BoneIndex].ToHumanReadableString());
					checkSlow(MultipliedBonePoses[BoneIndex].IsRotationNormalized());
					checkSlow(!MultipliedBonePoses[BoneIndex].ContainsNaN());
				}

				Processed[BoneIndex] = 1;
			}

			// Calculate final bone pose transforms from ref-pose to bake-pose 
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				MultipliedBonePoses[BoneIndex] = MultipliedRefBonePoses[BoneIndex].Inverse() * MultipliedBonePoses[BoneIndex];
				UE_CLOG(bDebugBonePoses, LogMeshBoneReduction, Log, TEXT("Final: [%i]\n%s"), BoneIndex, *MultipliedBonePoses[BoneIndex].ToHumanReadableString());
			}
		}
		else
		{
			MultipliedBonePoses.AddDefaulted(BoneNames.Num());
		}

		// Add bone transforms we're interested in
		InOutTransforms.Reset(BonesToRemove.Num());
		for (const FBoneIndexType& Index : BonesToRemove)
		{
			InOutTransforms.Add(MultipliedBonePoses[Index]);
		}
	}

	bool ReduceBoneCounts(USkeletalMesh* SkeletalMesh, int32 DesiredLOD, const TArray<FName>* BoneNamesToRemove) override
	{
		check (SkeletalMesh);
		USkeleton* Skeleton = SkeletalMesh->Skeleton;
		check (Skeleton);

		// find all the bones to remove from Skeleton settings
		TMap<FBoneIndexType, FBoneIndexType> BonesToRemove;

		bool bNeedsRemoval = GetBoneReductionData(SkeletalMesh, DesiredLOD, BonesToRemove, BoneNamesToRemove);
		// Always restore all previously removed bones if not contained by BonesToRemove
		SkeletalMesh->CalculateRequiredBones(SkeletalMesh->GetImportedResource()->LODModels[DesiredLOD], SkeletalMesh->RefSkeleton, &BonesToRemove);
		
		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
		SkeletalMesh->ReleaseResources();
		SkeletalMesh->ReleaseResourcesFence.Wait();

		FSkeletalMeshResource* SkeletalMeshResource = SkeletalMesh->GetImportedResource();
		check(SkeletalMeshResource);

		FStaticLODModel** LODModels = SkeletalMeshResource->LODModels.GetData();
		FStaticLODModel* SrcModel = LODModels[DesiredLOD];
		FStaticLODModel* NewModel = nullptr;
				
		if (bNeedsRemoval)
		{
			NewModel = new FStaticLODModel();
			LODModels[DesiredLOD] = NewModel;

			// Bulk data arrays need to be locked before a copy can be made.
			SrcModel->RawPointIndices.Lock(LOCK_READ_ONLY);
			SrcModel->LegacyRawPointIndices.Lock(LOCK_READ_ONLY);
			*NewModel = *SrcModel;
			SrcModel->RawPointIndices.Unlock();
			SrcModel->LegacyRawPointIndices.Unlock();

			// The index buffer needs to be rebuilt on copy.
			FMultiSizeIndexContainerData IndexBufferData, AdjacencyIndexBufferData;
			SrcModel->MultiSizeIndexContainer.GetIndexBufferData(IndexBufferData);
			SrcModel->AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyIndexBufferData);
			NewModel->RebuildIndexBuffer(&IndexBufferData, &AdjacencyIndexBufferData);

			TArray<FBoneIndexType> BoneIndices;
			TArray<FTransform> RemovedBoneTransforms;
			const bool bBakePoseToRemovedInfluences = (SkeletalMesh->LODInfo[DesiredLOD].BakePose != nullptr);
			if (bBakePoseToRemovedInfluences)
			{
				for (const FBoneReference& BoneReference : SkeletalMesh->LODInfo[DesiredLOD].BonesToRemove)
				{
					int32 BoneIndex = SkeletalMesh->RefSkeleton.FindRawBoneIndex(BoneReference.BoneName);
					if (BoneIndex != INDEX_NONE)
					{
						BoneIndices.AddUnique(BoneIndex);
					}
				}

				for (const TPair<FBoneIndexType, FBoneIndexType>& BonePair : BonesToRemove)
				{
					if (BonePair.Key != INDEX_NONE)
					{
						BoneIndices.AddUnique(BonePair.Key);
					}
				}

				RetrieveBoneTransforms(SkeletalMesh, DesiredLOD, BoneIndices, RemovedBoneTransforms);
			}

			// fix up chunks
			ParallelFor(NewModel->Sections.Num(), [this, NewModel, bBakePoseToRemovedInfluences, &RemovedBoneTransforms, &BoneIndices, &BonesToRemove](const int32 SectionIndex)
			{
				FSkelMeshSection& Section = NewModel->Sections[SectionIndex];
				if (bBakePoseToRemovedInfluences)
				{
					const float InfluenceMultiplier = 1.0f / 255.0f;
					for (FSoftSkinVertex& Vertex : Section.SoftVertices)
					{
						for (uint8 InfluenceIndex = 0; InfluenceIndex < 8; ++InfluenceIndex)
						{
							const int32 ArrayIndex = BoneIndices.IndexOfByKey(Section.BoneMap[Vertex.InfluenceBones[InfluenceIndex]]);
							if (ArrayIndex != INDEX_NONE)
							{
								Vertex.Position += ((RemovedBoneTransforms[ArrayIndex].TransformPosition(Vertex.Position) - Vertex.Position) * ((float)Vertex.InfluenceWeights[InfluenceIndex] * InfluenceMultiplier));
							}
						}
					}
				}
				FixUpSectionBoneMaps(Section, BonesToRemove);
			});

			// fix up RequiredBones/ActiveBoneIndices
			for (auto Iter = BonesToRemove.CreateIterator(); Iter; ++Iter)
			{
				FBoneIndexType BoneIndex = Iter.Key();
				FBoneIndexType MappingIndex = Iter.Value();
				NewModel->ActiveBoneIndices.Remove(BoneIndex);
				NewModel->RequiredBones.Remove(BoneIndex);

				NewModel->ActiveBoneIndices.AddUnique(MappingIndex);
				NewModel->RequiredBones.AddUnique(MappingIndex);
			}

			delete SrcModel;
		}
		else
		{
			NewModel = SrcModel;
		}		

		NewModel->ActiveBoneIndices.Sort();
		NewModel->RequiredBones.Sort();

		SkeletalMesh->PostEditChange();
		SkeletalMesh->InitResources();
		SkeletalMesh->MarkPackageDirty();
		
		return true;
	}
};

TUniquePtr<FMeshBoneReduction> GMeshBoneReduction;

void FMeshBoneReductionModule::StartupModule()
{
	GMeshBoneReduction = MakeUnique<FMeshBoneReduction>();
}

void FMeshBoneReductionModule::ShutdownModule()
{
	GMeshBoneReduction = nullptr;
}

IMeshBoneReduction* FMeshBoneReductionModule::GetMeshBoneReductionInterface()
{
	return GMeshBoneReduction.Get();
}
