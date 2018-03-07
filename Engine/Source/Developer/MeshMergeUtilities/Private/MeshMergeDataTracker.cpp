// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeDataTracker.h"
#include "MeshMergeHelpers.h"

FMeshMergeDataTracker::FMeshMergeDataTracker()
	: AvailableLightMapUVChannel(INDEX_NONE), SummedLightMapPixels(0)
{
	FMemory::Memzero(bWithVertexColors);
	FMemory::Memzero(bOcuppiedUVChannels);
}

FRawMesh& FMeshMergeDataTracker::AddAndRetrieveRawMesh(int32 MeshIndex, int32 LODIndex)
{
	checkf(!RawMeshLODs.Contains(FMeshLODKey(MeshIndex, LODIndex)), TEXT("Raw Mesh already added for this key"));
	return RawMeshLODs.Add(FMeshLODKey(MeshIndex, LODIndex));
}

void FMeshMergeDataTracker::RemoveRawMesh(int32 MeshIndex, int32 LODIndex)
{
	checkf(RawMeshLODs.Contains(FMeshLODKey(MeshIndex, LODIndex)), TEXT("No Raw Mesh for this key"));
	RawMeshLODs.Remove(FMeshLODKey(MeshIndex, LODIndex));
}

TConstRawMeshIterator FMeshMergeDataTracker::GetConstRawMeshIterator() const
{
	return RawMeshLODs.CreateConstIterator();
}

TRawMeshIterator FMeshMergeDataTracker::GetRawMeshIterator()
{
	return RawMeshLODs.CreateIterator();
}

void FMeshMergeDataTracker::AddLightmapChannelRecord(int32 MeshIndex, int32 LODIndex, int32 LightmapChannelIndex)
{
	LightmapChannelLODs.Add(FMeshLODKey(MeshIndex, LODIndex), LightmapChannelIndex);
}

int32 FMeshMergeDataTracker::AddSection(const FSectionInfo& SectionInfo)
{
	return UniqueSections.AddUnique(SectionInfo);
}

int32 FMeshMergeDataTracker::NumberOfUniqueSections() const
{
	return UniqueSections.Num();
}

UMaterialInterface* FMeshMergeDataTracker::GetMaterialForSectionIndex(int32 SectionIndex)
{
	checkf(UniqueSections.IsValidIndex(SectionIndex), TEXT("Invalid section index for stored data"));
	return UniqueSections[SectionIndex].Material;
}

const FSectionInfo& FMeshMergeDataTracker::GetSection(int32 SectionIndex) const
{
	checkf(UniqueSections.IsValidIndex(SectionIndex), TEXT("Invalid section index for stored data"));
	return UniqueSections[SectionIndex];
}

void FMeshMergeDataTracker::AddBakedMaterialSection(const FSectionInfo& SectionInfo)
{
	UniqueSections.Empty(1);
	UniqueSections.AddUnique(SectionInfo);
}

void FMeshMergeDataTracker::AddMaterialSlotName(UMaterialInterface *MaterialInterface, FName MaterialSlotName)
{
	FName *FindMaterialSlotName = MaterialInterfaceToMaterialSlotName.Find(MaterialInterface);
	//If there is a material use by more then one slot, only the first slot name occurrence will be use. (selection order)
	if (FindMaterialSlotName == nullptr)
	{
		MaterialInterfaceToMaterialSlotName.Add(MaterialInterface, MaterialSlotName);
	}
}

FName FMeshMergeDataTracker::GetMaterialSlotName(UMaterialInterface *MaterialInterface) const
{
	const FName *MaterialSlotName = MaterialInterfaceToMaterialSlotName.Find(MaterialInterface);
	return MaterialSlotName == nullptr ? NAME_None : *MaterialSlotName;
}

void FMeshMergeDataTracker::AddLODIndex(int32 LODIndex)
{
	LODIndices.AddUnique(LODIndex);
}

int32 FMeshMergeDataTracker::GetNumLODsForMergedMesh() const
{
	return LODIndices.Num();
}

TConstLODIndexIterator FMeshMergeDataTracker::GetLODIndexIterator() const
{
	return LODIndices.CreateConstIterator();
}

void FMeshMergeDataTracker::AddLightMapPixels(int32 Dimension)
{
	SummedLightMapPixels += FMath::Max(Dimension, 0);
}

int32 FMeshMergeDataTracker::GetLightMapDimension() const
{
	return FMath::CeilToInt(FMath::Sqrt(SummedLightMapPixels));
}

bool FMeshMergeDataTracker::DoesLODContainVertexColors(int32 LODIndex) const
{
	checkf(FMath::IsWithinInclusive(LODIndex, 0, MAX_STATIC_MESH_LODS - 1), TEXT("Invalid LOD index"));
	return bWithVertexColors[LODIndex];
}

bool FMeshMergeDataTracker::DoesUVChannelContainData(int32 UVChannel, int32 LODIndex) const
{
	checkf(FMath::IsWithinInclusive(LODIndex, 0, MAX_STATIC_MESH_LODS - 1), TEXT("Invalid LOD index"));
	checkf(FMath::IsWithinInclusive(UVChannel, 0, MAX_MESH_TEXTURE_COORDS - 1), TEXT("Invalid UV channel index"));
	return bOcuppiedUVChannels[LODIndex][UVChannel];
}

bool FMeshMergeDataTracker::DoesMeshLODRequireUniqueUVs(FMeshLODKey Key)
{
	// if we have vertex color, we require unique UVs
	return RequiresUniqueUVs.Contains(Key);
}

int32 FMeshMergeDataTracker::GetAvailableLightMapUVChannel() const
{
	return AvailableLightMapUVChannel;
}

FRawMesh* FMeshMergeDataTracker::GetRawMeshPtr(int32 MeshIndex, int32 LODIndex)
{
	return RawMeshLODs.Find(FMeshLODKey(MeshIndex, LODIndex));
}

FRawMesh* FMeshMergeDataTracker::GetRawMeshPtr(FMeshLODKey Key)
{
	return RawMeshLODs.Find(Key);
}

FRawMesh* FMeshMergeDataTracker::FindRawMeshAndLODIndex(int32 MeshIndex, int32& OutLODIndex)
{
	FRawMesh* FoundMeshPtr = nullptr;
	OutLODIndex = INDEX_NONE;
	for (TPair<FMeshLODKey, FRawMesh>& Pair : RawMeshLODs)
	{
		if (Pair.Key.GetMeshIndex() == MeshIndex)
		{
			FoundMeshPtr = &Pair.Value;
			OutLODIndex = Pair.Key.GetLODIndex();
			break;
		}
	}

	return FoundMeshPtr;
}

FRawMesh* FMeshMergeDataTracker::TryFindRawMeshForLOD(int32 MeshIndex, int32& InOutDesiredLODIndex)
{
	FRawMesh* FoundMeshPtr = RawMeshLODs.Find(FMeshLODKey(MeshIndex, InOutDesiredLODIndex));
	int32 SearchIndex = InOutDesiredLODIndex - 1;
	while (FoundMeshPtr == nullptr && SearchIndex >= 0)
	{
		for (TPair<FMeshLODKey, FRawMesh>& Pair : RawMeshLODs)
		{
			if (Pair.Key.GetMeshIndex() == MeshIndex && Pair.Key.GetLODIndex() == SearchIndex)
			{
				FoundMeshPtr = &Pair.Value;
				InOutDesiredLODIndex = SearchIndex;
				break;
			}
		}

		--SearchIndex;
	}

	return FoundMeshPtr;
}

void FMeshMergeDataTracker::AddSectionRemapping(int32 MeshIndex, int32 LODIndex, int32 OriginalIndex, int32 UniqueIndex)
{
	UniqueSectionIndexPerLOD.Add(FMeshLODKey(MeshIndex, LODIndex), SectionRemapPair(OriginalIndex, UniqueIndex));
	UniqueSectionToMeshLOD.Add(UniqueIndex, FMeshLODKey(MeshIndex, LODIndex));
}

void FMeshMergeDataTracker::GetMeshLODsMappedToUniqueSection(int32 UniqueIndex, TArray<FMeshLODKey>& InOutMeshLODs)
{
	UniqueSectionToMeshLOD.MultiFind(UniqueIndex, InOutMeshLODs);
}

void FMeshMergeDataTracker::GetMappingsForMeshLOD(FMeshLODKey Key, TArray<SectionRemapPair>& InOutMappings)
{
	UniqueSectionIndexPerLOD.MultiFind(Key, InOutMappings);
}

void FMeshMergeDataTracker::ProcessRawMeshes()
{
	bool bPotentialLightmapUVChannels[MAX_MESH_TEXTURE_COORDS];
	FMemory::Memset(bPotentialLightmapUVChannels, 1);
	bool bPotentialLODLightmapUVChannels[MAX_STATIC_MESH_LODS][MAX_MESH_TEXTURE_COORDS];
	FMemory::Memset(bPotentialLODLightmapUVChannels, 1);

	// Retrieve information in regards to occupied UV channels whether or not a mesh contains vertex colors, and if 
	for (const TPair<FMeshLODKey, FRawMesh>& MeshPair : RawMeshLODs)
	{
		const FMeshLODKey& Key = MeshPair.Key;
		const int32 LODIndex = Key.GetLODIndex();
		const FRawMesh& RawMesh = MeshPair.Value;

		const int32 LightmapChannelIdx = LightmapChannelLODs.FindRef(Key);
		bool bNeedsVertexData = false;
		
		for (int32 ChannelIndex = 0; ChannelIndex < MAX_MESH_TEXTURE_COORDS; ++ChannelIndex)
		{
			if (RawMesh.WedgeTexCoords[ChannelIndex].Num())
			{
				bOcuppiedUVChannels[LODIndex][ChannelIndex] = true;
				bPotentialLODLightmapUVChannels[LODIndex][ChannelIndex] = (ChannelIndex == LightmapChannelIdx);

				const bool bWrappingUVs = FMeshMergeHelpers::CheckWrappingUVs(RawMesh.WedgeTexCoords[ChannelIndex]);
				if (bWrappingUVs)
				{
					bNeedsVertexData = true;
				}
			}
		}

		// Merge available lightmap slots from LODs into one set, so we can assess later what slots are available
		for (int32 ChannelIdx = 1; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
		{
			bPotentialLightmapUVChannels[ChannelIdx] &= bPotentialLODLightmapUVChannels[LODIndex][ChannelIdx];
		}

		if (bNeedsVertexData)
		{
			RequiresUniqueUVs.Add(Key);
		}

		bWithVertexColors[LODIndex] |= RawMesh.WedgeColors.Num() != 0;
	}

	// Look for an available lightmap slot we can use in the merged set
	// We start at channel 1 as merged meshes always use texcoord 0 for their expected mapping channel, so we cant use it;
	AvailableLightMapUVChannel = INDEX_NONE;
	for (int32 ChannelIdx = 1; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
	{
		if(bPotentialLightmapUVChannels[ChannelIdx])
		{
			AvailableLightMapUVChannel = ChannelIdx;
			break;
		}
	}
}