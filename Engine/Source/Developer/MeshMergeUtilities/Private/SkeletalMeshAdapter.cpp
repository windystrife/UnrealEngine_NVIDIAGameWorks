// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshAdapter.h"
#include "MeshMergeHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h"

FSkeletalMeshComponentAdapter::FSkeletalMeshComponentAdapter(USkeletalMeshComponent* InSkeletalMeshComponent)
	: SkeletalMeshComponent(InSkeletalMeshComponent), SkeletalMesh(InSkeletalMeshComponent->SkeletalMesh)
{
	checkf(SkeletalMesh != nullptr, TEXT("Invalid skeletal mesh in adapter"));
	NumLODs = SkeletalMesh->LODInfo.Num();
}

int32 FSkeletalMeshComponentAdapter::GetNumberOfLODs() const
{
	return NumLODs;
}

void FSkeletalMeshComponentAdapter::RetrieveRawMeshData(int32 LODIndex, FRawMesh& InOutRawMesh, bool bPropogateMeshData) const
{
	FMeshMergeHelpers::RetrieveMesh(SkeletalMeshComponent, LODIndex, InOutRawMesh, bPropogateMeshData);
}

void FSkeletalMeshComponentAdapter::RetrieveMeshSections(int32 LODIndex, TArray<FSectionInfo>& InOutSectionInfo) const
{
	FMeshMergeHelpers::ExtractSections(SkeletalMeshComponent, LODIndex, InOutSectionInfo);
}

int32 FSkeletalMeshComponentAdapter::GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const
{
	return SkeletalMesh->GetImportedResource()->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
}

UPackage* FSkeletalMeshComponentAdapter::GetOuter() const
{
	return nullptr;
}

FString FSkeletalMeshComponentAdapter::GetBaseName() const
{
	return SkeletalMesh->GetOutermost()->GetName();
}

void FSkeletalMeshComponentAdapter::SetMaterial(int32 MaterialIndex, UMaterialInterface* Material)
{
	SkeletalMesh->Materials[MaterialIndex] = Material;
}

void FSkeletalMeshComponentAdapter::RemapMaterialIndex(int32 LODIndex, int32 SectionIndex, int32 NewMaterialIndex)
{
	SkeletalMesh->GetImportedResource()->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex = NewMaterialIndex;
}

int32 FSkeletalMeshComponentAdapter::AddMaterial(UMaterialInterface* Material)
{
	return SkeletalMesh->Materials.Add(Material);
}

void FSkeletalMeshComponentAdapter::UpdateUVChannelData()
{
	SkeletalMesh->UpdateUVChannelData(false);
}

bool FSkeletalMeshComponentAdapter::IsAsset() const
{
	return true;
}

int32 FSkeletalMeshComponentAdapter::LightmapUVIndex() const
{
	return INDEX_NONE;
}

FBoxSphereBounds FSkeletalMeshComponentAdapter::GetBounds() const
{
	return SkeletalMesh->GetBounds();
}
