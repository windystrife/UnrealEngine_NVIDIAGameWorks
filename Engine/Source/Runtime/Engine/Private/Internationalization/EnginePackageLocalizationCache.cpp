// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/EnginePackageLocalizationCache.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"

FEnginePackageLocalizationCache::FEnginePackageLocalizationCache()
	: bIsScanningPath(false)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.OnAssetAdded().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetAdded);
	AssetRegistry.OnAssetRemoved().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetRenamed);
}

FEnginePackageLocalizationCache::~FEnginePackageLocalizationCache()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		AssetRegistry.OnAssetAdded().RemoveAll(this);
		AssetRegistry.OnAssetRemoved().RemoveAll(this);
		AssetRegistry.OnAssetRenamed().RemoveAll(this);
	}
}

void FEnginePackageLocalizationCache::FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

#if WITH_EDITOR
	// Make sure the asset registry has the data we need
	{
		TArray<FString> LocalizedPackagePaths;
		LocalizedPackagePaths.Add(InLocalizedRoot);

		// Set bIsScanningPath to avoid us processing newly added assets from this scan
		TGuardValue<bool> SetIsScanningPath(bIsScanningPath, true);
		AssetRegistry.ScanPathsSynchronous(LocalizedPackagePaths);
	}
#endif // WITH_EDITOR

	TArray<FAssetData> LocalizedAssetDataArray;
	AssetRegistry.GetAssetsByPath(*InLocalizedRoot, LocalizedAssetDataArray, /*bRecursive*/true);

	for (const FAssetData& LocalizedAssetData : LocalizedAssetDataArray)
	{
		const FName SourcePackageName = *FPackageName::GetSourcePackagePath(LocalizedAssetData.PackageName.ToString());

		TArray<FName>& PrioritizedLocalizedPackageNames = InOutSourcePackagesToLocalizedPackages.FindOrAdd(SourcePackageName);
		PrioritizedLocalizedPackageNames.AddUnique(LocalizedAssetData.PackageName);
	}
}

void FEnginePackageLocalizationCache::FindAssetGroupPackages(const FName InAssetGroupName, const FName InAssetClassName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// We use the localized paths to find the source assets for the group since it's much faster to scan those paths than perform a full scan
	TArray<FString> LocalizedRootPaths;
	{
		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);
		for (const FString& RootPath : RootPaths)
		{
			LocalizedRootPaths.Add(RootPath / TEXT("L10N"));
		}
	}

#if WITH_EDITOR
	// Make sure the asset registry has the data we need
	AssetRegistry.ScanPathsSynchronous(LocalizedRootPaths);
#endif // WITH_EDITOR

	// Build the filter to get all localized assets of the given class
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (const FString& LocalizedRootPath : LocalizedRootPaths)
	{
		Filter.PackagePaths.Add(*LocalizedRootPath);
	}
	Filter.bIncludeOnlyOnDiskAssets = false;
	Filter.ClassNames.Add(InAssetClassName);
	Filter.bRecursiveClasses = false;

	TArray<FAssetData> LocalizedAssetsOfClass;
	AssetRegistry.GetAssets(Filter, LocalizedAssetsOfClass);

	for (const FAssetData& LocalizedAssetOfClass : LocalizedAssetsOfClass)
	{
		const FName SourcePackageName = *FPackageName::GetSourcePackagePath(LocalizedAssetOfClass.PackageName.ToString());
		PackageNameToAssetGroup.Add(SourcePackageName, InAssetGroupName);
	}
}

void FEnginePackageLocalizationCache::HandleAssetAdded(const FAssetData& InAssetData)
{
	if (bIsScanningPath)
	{
		// Ignore this, it came from the path we're currently scanning
		return;
	}

	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->AddPackage(InAssetData.PackageName.ToString());
	}

	bPackageNameToAssetGroupDirty = true;
}

void FEnginePackageLocalizationCache::HandleAssetRemoved(const FAssetData& InAssetData)
{
	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->RemovePackage(InAssetData.PackageName.ToString());
	}

	bPackageNameToAssetGroupDirty = true;
}

void FEnginePackageLocalizationCache::HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
{
	const FString OldPackagePath = FPackageName::ObjectPathToPackageName(InOldObjectPath);

	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->RemovePackage(OldPackagePath);
		CultureCachePair.Value->AddPackage(InAssetData.PackageName.ToString());
	}

	bPackageNameToAssetGroupDirty = true;
}
