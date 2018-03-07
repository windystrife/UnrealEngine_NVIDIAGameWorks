// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshAdapter.h"
#include "MaterialBakingStructures.h"
#include "Engine/StaticMesh.h"
#include "UObject/Package.h"
#include "MeshMergeHelpers.h"

FStaticMeshAdapter::FStaticMeshAdapter(UStaticMesh* InStaticMesh)
	: StaticMesh(InStaticMesh)
{
	checkf(StaticMesh != nullptr, TEXT("Invalid static mesh in adapter"));
	NumLODs = StaticMesh->GetNumLODs();
}

int32 FStaticMeshAdapter::GetNumberOfLODs() const
{
	return NumLODs;
}

void FStaticMeshAdapter::RetrieveRawMeshData(int32 LODIndex, FRawMesh& InOutRawMesh, bool bPropogateMeshData) const
{
	FMeshMergeHelpers::RetrieveMesh(StaticMesh, LODIndex, InOutRawMesh);
}

void FStaticMeshAdapter::RetrieveMeshSections(int32 LODIndex, TArray<FSectionInfo>& InOutSectionInfo) const
{
	FMeshMergeHelpers::ExtractSections(StaticMesh, LODIndex, InOutSectionInfo);
}

int32 FStaticMeshAdapter::GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const
{
	return StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex).MaterialIndex;
}

void FStaticMeshAdapter::ApplySettings(int32 LODIndex, FMeshData& InOutMeshData) const
{
	InOutMeshData.LightMapIndex = StaticMesh->LightMapCoordinateIndex;
}

UPackage* FStaticMeshAdapter::GetOuter() const
{
	return nullptr;
}

FString FStaticMeshAdapter::GetBaseName() const
{
	return StaticMesh->GetOutermost()->GetName();
}

void FStaticMeshAdapter::SetMaterial(int32 MaterialIndex, UMaterialInterface* Material)
{
	StaticMesh->StaticMaterials[MaterialIndex] = Material;
}

void FStaticMeshAdapter::RemapMaterialIndex(int32 LODIndex, int32 SectionIndex, int32 NewMaterialIndex)
{
	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
	SectionInfo.MaterialIndex = NewMaterialIndex;
	StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);
}

int32 FStaticMeshAdapter::AddMaterial(UMaterialInterface* Material)
{
	return StaticMesh->StaticMaterials.Add(Material);
}

void FStaticMeshAdapter::UpdateUVChannelData()
{
	StaticMesh->UpdateUVChannelData(false);
}

bool FStaticMeshAdapter::IsAsset() const
{
	return true;
}

int32 FStaticMeshAdapter::LightmapUVIndex() const
{
	return StaticMesh->LightMapCoordinateIndex;
}

FBoxSphereBounds FStaticMeshAdapter::GetBounds() const
{
	return StaticMesh->GetBounds();
}
