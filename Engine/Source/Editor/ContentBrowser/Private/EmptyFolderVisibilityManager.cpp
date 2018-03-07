// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EmptyFolderVisibilityManager.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserUtils.h"
#include "Settings/ContentBrowserSettings.h"
#include "Paths.h"

FEmptyFolderVisibilityManager::FEmptyFolderVisibilityManager()
{
	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnPathRemoved().AddRaw(this, &FEmptyFolderVisibilityManager::OnAssetRegistryPathRemoved);
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FEmptyFolderVisibilityManager::OnAssetRegistryAssetAdded);

	// Query all paths currently gathered from the asset registry
	TArray<FString> PathList;
	AssetRegistryModule.Get().GetAllCachedPaths(PathList);
	for (const FString& Path : PathList)
	{
		const bool bPathIsEmpty = ContentBrowserUtils::IsEmptyFolder(Path, true);
		if (!bPathIsEmpty)
		{
			PathsToAlwaysShow.Add(Path);
		}
	}
}

FEmptyFolderVisibilityManager::~FEmptyFolderVisibilityManager()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		// Load the asset registry module to stop listening for updates
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnPathRemoved().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
	}
}

bool FEmptyFolderVisibilityManager::ShouldShowPath(const FString& InPath) const
{
	const bool bDisplayEmpty = GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;
	if (bDisplayEmpty)
	{
		return true;
	}

	const bool bPathIsEmpty = ContentBrowserUtils::IsEmptyFolder(InPath, true);
	return !bPathIsEmpty || PathsToAlwaysShow.Contains(InPath);
}

void FEmptyFolderVisibilityManager::SetAlwaysShowPath(const FString& InPath)
{
	if (InPath.IsEmpty())
	{
		return;
	}

	FString PathToAdd = InPath;

	bool bWasAlreadyShown = false;
	if (PathToAdd[PathToAdd.Len() - 1] == TEXT('/'))
	{
		PathToAdd = PathToAdd.Mid(0, PathToAdd.Len() - 1);
	}
	PathsToAlwaysShow.Add(PathToAdd, &bWasAlreadyShown);

	if (!bWasAlreadyShown)
	{
		OnFolderPopulatedDelegate.Broadcast(PathToAdd);

		// We also need to make sure the parents of this path are on the visible list too
		SetAlwaysShowPath(FPaths::GetPath(PathToAdd));
	}
}

void FEmptyFolderVisibilityManager::OnAssetRegistryPathRemoved(const FString& InPath)
{
	PathsToAlwaysShow.Remove(InPath);
}

void FEmptyFolderVisibilityManager::OnAssetRegistryAssetAdded(const FAssetData& InAssetData)
{
	SetAlwaysShowPath(InAssetData.PackagePath.ToString());
}
