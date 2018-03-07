// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_VectorFieldStatic.h"
#include "EditorFramework/AssetImportData.h"

void FAssetTypeActions_VectorFieldStatic::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto StaticVectorField = CastChecked<UVectorFieldStatic>(Asset);
		StaticVectorField->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}
