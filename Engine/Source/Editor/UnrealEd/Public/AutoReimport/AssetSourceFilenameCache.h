// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetData.h"
#include "EditorFramework/AssetImportData.h"

class IAssetRegistry;


/** Class responsible for maintaing a cache of clean source file names (bla.txt) to asset data */
class UNREALED_API FAssetSourceFilenameCache
{
public:
	FAssetSourceFilenameCache();

	/** Singleton access */
	static FAssetSourceFilenameCache& Get();

	/** Helper functions to extract asset import information from asset registry tags */
	static TOptional<FAssetImportInfo> ExtractAssetImportInfo(const FAssetData& AssetData);

	/** Retrieve a list of assets that were imported from the specified filename */
	TArray<FAssetData> GetAssetsPertainingToFile(const IAssetRegistry& Registry, const FString& AbsoluteFilename) const;

	/** Shutdown this instance of the cache */
	void Shutdown();

	/** Event for when an asset has been renamed, and has been updated in our source file cache */
	DECLARE_EVENT_TwoParams( FAssetSourceFilenameCache, FAssetRenamedEvent, const FAssetData&, const FString& );
	FAssetRenamedEvent& OnAssetRenamed() { return AssetRenamedEvent; }

private:
	/** Delegate bindings that keep the cache up-to-date */
	void HandleOnAssetAdded(const FAssetData& AssetData);
	void HandleOnAssetRemoved(const FAssetData& AssetData);
	void HandleOnAssetRenamed(const FAssetData& AssetData, const FString& OldPath);
	void HandleOnAssetUpdated(const FAssetImportInfo& OldData, const UAssetImportData* ImportData);
	
	/** Event that is triggered when an asset has been renamed, and we've updated our cache */
	FAssetRenamedEvent AssetRenamedEvent;

	/** Map of clean filenames (no leading path information) to object paths that were imported with that file */
	TMap<FString, TSet<FName>> SourceFileToObjectPathCache;
};
