// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"

class USkeletalMesh;

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}

// Define interface for importing apex clothing asset
namespace ApexClothingUtils
{
	UNREALED_API FString PromptForClothingFile();

	// Prompt the user to select an APEX file that will be imported to the new UClothingAsset format
	UNREALED_API void PromptAndImportClothing(USkeletalMesh* SkelMesh);

	// Given a buffer, build an apex clothing asset
	UNREALED_API nvidia::apex::ClothingAsset* CreateApexClothingAssetFromBuffer(const uint8* Buffer, int32 BufferSize);


	// Functions below remain from previous clothing system and only remain to remove the bound
	// data from a skeletal mesh. This is done when postloading USkeletalMesh when upgrading the assets

	// Function to restore all clothing section to original mesh section related to specified asset index
	UNREALED_API void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bReleaseAsset = true, bool bRecreateSkelMeshComponent = false);
	// Function to restore clothing section to original mesh section
	UNREALED_API void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, bool bReregisterSkelMeshComponent = true);
}
