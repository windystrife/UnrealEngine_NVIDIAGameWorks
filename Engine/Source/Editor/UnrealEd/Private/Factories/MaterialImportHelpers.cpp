// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/MaterialImportHelpers.h"
#include "AssetRegistryModule.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "Materials/MaterialInterface.h"
#include "Paths.h"

UMaterialInterface* UMaterialImportHelpers::FindExistingMaterialFromSearchLocation(const FString& MaterialFullName, const FString& BasePackagePath, EMaterialSearchLocation SearchLocation, FText& OutError)
{
	UMaterialInterface* FoundMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialFullName, nullptr, LOAD_Quiet | LOAD_NoWarn);

	if (FoundMaterial == nullptr && SearchLocation != EMaterialSearchLocation::Local)
	{
		// Search recursively in asset's folder
		FString SearchPath = FPaths::GetPath(BasePackagePath);
		
		FoundMaterial = FindExistingMaterial(SearchPath, MaterialFullName, OutError);

		if (FoundMaterial == nullptr &&
			(	SearchLocation == EMaterialSearchLocation::UnderParent ||
				SearchLocation == EMaterialSearchLocation::UnderRoot ||
				SearchLocation == EMaterialSearchLocation::AllAssets))
		{
			// Search recursively in parent's folder
			SearchPath = FPaths::GetPath(SearchPath);

			FoundMaterial = FindExistingMaterial(SearchPath, MaterialFullName, OutError);
		}
		if (FoundMaterial == nullptr &&
			(	SearchLocation == EMaterialSearchLocation::UnderRoot ||
				SearchLocation == EMaterialSearchLocation::AllAssets))
		{
			// Search recursively in root folder of asset
			FString OutPackageRoot, OutPackagePath, OutPackageName;
			FPackageName::SplitLongPackageName(SearchPath, OutPackageRoot, OutPackagePath, OutPackageName);

			FoundMaterial = FindExistingMaterial(OutPackageRoot, MaterialFullName, OutError);
		}
		if (FoundMaterial == nullptr &&
			SearchLocation == EMaterialSearchLocation::AllAssets)
		{
			// Search everywhere
			FoundMaterial = FindExistingMaterial(TEXT("/"), MaterialFullName, OutError);
		}
	}

	return FoundMaterial;
}

UMaterialInterface* UMaterialImportHelpers::FindExistingMaterial(const FString& BasePath, const FString& MaterialFullName, FText& OutError)
{
	UMaterialInterface* Material = nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TArray<FAssetData> AssetData;
	FARFilter Filter;

	AssetRegistry.SearchAllAssets(true);

	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.ClassNames.Add(UMaterialInterface::StaticClass()->GetFName());
	Filter.PackagePaths.Add(FName(*BasePath));

	AssetRegistry.GetAssets(Filter, AssetData);

	TArray<UMaterialInterface*> FoundMaterials;
	for (const FAssetData& Data : AssetData)
	{
		if (Data.AssetName == FName(*MaterialFullName))
		{
			Material = Cast<UMaterialInterface>(Data.GetAsset());
			if (Material != nullptr)
			{
				FoundMaterials.Add(Material);
			}
		}
	}

	if (FoundMaterials.Num() > 1)
	{
		check(Material != nullptr);
		OutError =
			FText::Format(NSLOCTEXT("MaterialImportHelpers", "MultipleMaterialsFound", "Found {0} materials matching name '{1}'. Using '{2}'."),
				FText::FromString(FString::FromInt(FoundMaterials.Num())),
				FText::FromString(MaterialFullName),
				FText::FromString(Material->GetOutermost()->GetName()));
	}
	return Material;
}
