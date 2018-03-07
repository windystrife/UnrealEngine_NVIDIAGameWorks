// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApexClothingUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "EditorDirectories.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "SlateApplication.h"

#include "ClothingAssetFactory.h"
#include "PhysicsPublic.h"
#include "EditorPhysXSupport.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexClothingUtils, Log, All);

#define LOCTEXT_NAMESPACE "ApexClothingUtils"

namespace ApexClothingUtils
{
#if WITH_APEX_CLOTHING

//enforces a call of "OnRegister" to update vertex factories
void ReregisterSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == SkelMesh )
		{
			MeshComponent->ReregisterComponent();
		}
	}
}

void RefreshSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == SkelMesh )
		{
			MeshComponent->RecreateRenderState_Concurrent();
		}
	}
}

FString PromptForClothingFile()
{
	if(IDesktopPlatform* Platform = FDesktopPlatformModule::Get())
{
		const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		TArray<FString> OpenFilenames;
		FString OpenFilePath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::MESH_IMPORT_EXPORT);

		if(Platform->OpenFileDialog(
			ParentWindowWindowHandle,
			*LOCTEXT("ImportClothing_ChooseFile", "Choose clothing asset source file").ToString(),
			OpenFilePath,
			TEXT(""),
			TEXT("APEX clothing asset(*.apx,*.apb)|*.apx;*.apb|All files (*.*)|*.*"),
			EFileDialogFlags::None,
			OpenFilenames
			))
	{
			return OpenFilenames[0];
		}
	}

	// Nothing picked, empty path
	return FString();
}

void PromptAndImportClothing(USkeletalMesh* SkelMesh)
{
	ensure(SkelMesh);

	FString Filename = PromptForClothingFile();

	if(!Filename.IsEmpty())
{
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::MESH_IMPORT_EXPORT, Filename);

		UClothingAssetFactory* Factory = UClothingAssetFactory::StaticClass()->GetDefaultObject<UClothingAssetFactory>();

		if(Factory && Factory->CanImport(Filename))
	{					
			Factory->Import(Filename, SkelMesh);
	}
	}
}

#if WITH_APEX_CLOTHING
apex::ClothingAsset* CreateApexClothingAssetFromPxStream(physx::PxFileBuf& Stream)
{
	// Peek into the buffer to see what kind of data it is (binary or xml)
	NvParameterized::Serializer::SerializeType SerializeType = GApexSDK->getSerializeType(Stream);
	// Create an NvParameterized serializer for the correct data type
	NvParameterized::Serializer* Serializer = GApexSDK->createSerializer(SerializeType);

	if(!Serializer)
	{
		return NULL;
	}
	// Deserialize into a DeserializedData buffer
	NvParameterized::Serializer::DeserializedData DeserializedData;
	NvParameterized::Serializer::ErrorType Error = Serializer->deserialize(Stream, DeserializedData);

	apex::Asset* ApexAsset = NULL;
	if( DeserializedData.size() > 0 )
	{
		// The DeserializedData has something in it, so create an APEX asset from it
		ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL);
	}
	// Release the serializer
	Serializer->release();

	return (apex::ClothingAsset*)ApexAsset;
}

apex::ClothingAsset* CreateApexClothingAssetFromBuffer(const uint8* Buffer, int32 BufferSize)
{
	apex::ClothingAsset* ApexClothingAsset = NULL;

	// Wrap Buffer with the APEX read stream class
	physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream(Buffer, BufferSize);

	if (Stream != NULL)
	{
		ApexClothingAsset = ApexClothingUtils::CreateApexClothingAssetFromPxStream(*Stream);
		// Release our stream
		GApexSDK->releaseMemoryReadStream(*Stream);
	}

	return ApexClothingAsset;
}

#endif// #if WITH_APEX_CLOTHING

void GetOriginSectionIndicesWithCloth(USkeletalMesh* InSkelMesh, int32 LODIndex, int32 AssetIndex, TArray<uint32>& OutSectionIndices)
{
	FSkeletalMeshResource* Resource = InSkelMesh->GetImportedResource();

	OutSectionIndices.Empty();

	check(Resource->LODModels.IsValidIndex(LODIndex));

	FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
	int32 NumSections = LODModel.Sections.Num();

	for(int32 SecIdx = 0; SecIdx < NumSections; SecIdx++)
				{
		if(LODModel.Sections[SecIdx].CorrespondClothAssetIndex == AssetIndex)
					{
			//add original sections
			OutSectionIndices.Add(LODModel.Sections[SecIdx].CorrespondClothSectionIndex);
					}
				}
			}

void RestoreAllClothingSections(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 AssetIndex)
{
	TArray<uint32> SectionIndices;
	GetOriginSectionIndicesWithCloth(SkelMesh, LODIndex, AssetIndex, SectionIndices);

	for(int32 i=0; i < SectionIndices.Num(); i++)
	{
		RestoreOriginalClothingSection(SkelMesh, LODIndex, SectionIndices[i], false);
	}
}

void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bReleaseAsset, bool bRecreateSkelMeshComponent)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	int32 NumLODs = ImportedResource->LODModels.Num();

	for(int32 LODIdx=0; LODIdx < NumLODs; LODIdx++)
	{
		RestoreAllClothingSections(SkelMesh, LODIdx, AssetIndex);

		FStaticLODModel& LODModel = ImportedResource->LODModels[LODIdx];

		int32 NumSections = LODModel.Sections.Num();

		//decrease asset index because one asset is removed
		for(int32 SectionIdx=0; SectionIdx < NumSections; SectionIdx++)
		{
			if(LODModel.Sections[SectionIdx].CorrespondClothAssetIndex > (int16)AssetIndex)
				LODModel.Sections[SectionIdx].CorrespondClothAssetIndex--;
		}
	}


	apex::ClothingAsset* ApexClothingAsset = SkelMesh->ClothingAssets_DEPRECATED[AssetIndex].ApexClothingAsset;	//Can't delete apex asset until after apex actors so we save this for now and reregister component (which will trigger the actor delete)
	SkelMesh->ClothingAssets_DEPRECATED.RemoveAt(AssetIndex);	//have to remove the asset from the array so that new actors are not created for asset pending deleting
	ReregisterSkelMeshComponents(SkelMesh);

	if(bReleaseAsset)
	{
	//Now we can actually delete the asset
	GPhysCommandHandler->DeferredRelease(ApexClothingAsset);
	}

	if(bRecreateSkelMeshComponent)
	{
		// Refresh skeletal mesh components
		RefreshSkelMeshComponents(SkelMesh);
	}
}

void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, bool bReregisterSkelMeshComponent)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	int32 OriginSectionIndex = -1, ClothSectionIndex = -1;

	// if this section is original mesh section ( without clothing data )
	if(Section.CorrespondClothSectionIndex >= 0
		&& !Section.HasClothingData())
	{
		ClothSectionIndex = Section.CorrespondClothSectionIndex;
		OriginSectionIndex = SectionIndex;
	}

	// if this section is clothing section
	if(Section.CorrespondClothSectionIndex >= 0
		&& Section.HasClothingData())
	{
		ClothSectionIndex = SectionIndex;
		OriginSectionIndex = Section.CorrespondClothSectionIndex;
	}

	if(OriginSectionIndex >= 0 && ClothSectionIndex >= 0)
	{
		// Apply to skeletal mesh
		SkelMesh->PreEditChange(NULL);

		FSkelMeshSection& ClothSection = LODModel.Sections[ClothSectionIndex];
		FSkelMeshSection& OriginSection = LODModel.Sections[OriginSectionIndex];

		// remove cloth section & chunk
		TArray<uint32> OutIndexBuffer;
		LODModel.MultiSizeIndexContainer.GetIndexBuffer(OutIndexBuffer);

		uint32 RemovedBaseIndex = ClothSection.BaseIndex;
		uint32 RemovedNumIndices = ClothSection.NumTriangles * 3;
		uint32 RemovedBaseVertexIndex = ClothSection.BaseVertexIndex;

		int32 NumIndexBuffer = OutIndexBuffer.Num();

		int32 NumRemovedVertices = ClothSection.GetNumVertices();

		// Remove old indices and update any indices for other sections
		OutIndexBuffer.RemoveAt(RemovedBaseIndex, RemovedNumIndices);

		NumIndexBuffer = OutIndexBuffer.Num();

		for(int32 i=0; i < NumIndexBuffer; i++)
		{
			if(OutIndexBuffer[i] >= ClothSection.BaseVertexIndex)
				OutIndexBuffer[i] -= NumRemovedVertices;
		}

		LODModel.MultiSizeIndexContainer.CopyIndexBuffer(OutIndexBuffer);

		LODModel.Sections.RemoveAt(OriginSection.CorrespondClothSectionIndex);

		LODModel.NumVertices -= NumRemovedVertices;

		int32 NumSections = LODModel.Sections.Num();
		//decrease indices
		for(int32 i=0; i < NumSections; i++)
		{
			if(LODModel.Sections[i].CorrespondClothSectionIndex > OriginSection.CorrespondClothSectionIndex)
				LODModel.Sections[i].CorrespondClothSectionIndex -= 1;

			if(LODModel.Sections[i].BaseIndex > RemovedBaseIndex)
				LODModel.Sections[i].BaseIndex -= RemovedNumIndices;

			if(LODModel.Sections[i].BaseVertexIndex > RemovedBaseVertexIndex)
				LODModel.Sections[i].BaseVertexIndex -= NumRemovedVertices;
		}

		OriginSection.bDisabled = false;
		OriginSection.CorrespondClothSectionIndex = -1;

		if(bReregisterSkelMeshComponent)
		{
			ReregisterSkelMeshComponents(SkelMesh);
		}
		
		SkelMesh->PostEditChange();
	}
	else
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("No exists proper section : %d "), SectionIndex );	
	}
}

#else

void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, bool bRecreateSkelMeshComponent)
{
	// print error about not supporting APEX
	UE_LOG(LogApexClothingUtils, Warning,TEXT("APEX Clothing is not supported") );	
}

#endif // WITH_APEX_CLOTHING

} // namespace ApexClothingUtils

#undef LOCTEXT_NAMESPACE
