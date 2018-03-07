// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/IPackageLocalizationCache.h"

class FPackageLocalizationCache;

/** Package localization cache for a specific culture (may contain a chain of cultures internally based on their priority) */
class COREUOBJECT_API FPackageLocalizationCultureCache
{
public:
	/**
	 * Construct a new culture specific cache.
	 *
	 * @param InOwnerCache		A pointer to our owner cache. Used to perform an implementation specific search for localized packages (see FPackageLocalizationCache::FindLocalizedPackages).
	 * @param InCultureName		The name of the culture this cache is for.
	 */
	FPackageLocalizationCultureCache(FPackageLocalizationCache* InOwnerCache, const FString& InCultureName);

	/**
	 * Update this cache, but only if it is dirty.
	 */
	void ConditionalUpdateCache();

	/**
	 * Add a source path to be scanned the next time ConditionalUpdateCache is called.
	 *
	 * @param InRootPath		The root source path to add.
	 */
	void AddRootSourcePath(const FString& InRootPath);

	/**
	 * Remove a source path. This will remove it from the pending list, as well as immediately remove any localized package entries mapped from this source root.
	 *
	 * @param InRootPath		The root source path to remove.
	 */
	void RemoveRootSourcePath(const FString& InRootPath);

	/**
	 * Add a package (potentially source or localized) to this cache.
	 *
	 * @param InPackageName		The name of the package to add.
	 */
	void AddPackage(const FString& InPackageName);

	/**
	 * Remove a package (potentially source or localized) from this cache.
	 *
	 * @param InPackageName		The name of the package to remove.
	 */
	void RemovePackage(const FString& InPackageName);

	/**
	 * Restore this cache to an empty state.
	 */
	void Empty();

	/**
	 * Try and find the localized package name for the given source package for culture we represent.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	FName FindLocalizedPackageName(const FName InSourcePackageName);

private:
	/**
	 * Update this cache, but only if it is dirty. Same as ConditionalUpdateCache, but doesn't take a lock (you must have locked in the calling code).
	 */
	void ConditionalUpdateCache_NoLock();

private:
	/** Critical section preventing concurrent access to our data. */
	mutable FCriticalSection LocalizedPackagesCS;

	/** A pointer to our owner cache. */
	FPackageLocalizationCache* OwnerCache;

	/** An array of culture names that should be scanned, sorted in priority order. */
	TArray<FString> PrioritizedCultureNames;

	/** An array of source paths we should scan on the next call to ConditionalUpdateCache. */
	TArray<FString> PendingSourceRootPathsToSearch;

	/** Mapping between source paths, and prioritized localized paths. */
	TMap<FString, TArray<FString>> SourcePathsToLocalizedPaths;

	/** Mapping between source package names, and prioritized localized package names. */
	TMap<FName, TArray<FName>> SourcePackagesToLocalizedPackages;
};

/** Common implementation for the package localization cache */
class COREUOBJECT_API FPackageLocalizationCache : public IPackageLocalizationCache
{
	friend class FPackageLocalizationCultureCache;

public:
	FPackageLocalizationCache();
	virtual ~FPackageLocalizationCache();

	//~ IPackageLocalizationCache interface
	virtual void ConditionalUpdateCache() override;
	virtual FName FindLocalizedPackageName(const FName InSourcePackageName) override;
	virtual FName FindLocalizedPackageNameForCulture(const FName InSourcePackageName, const FString& InCultureName) override;

protected:
	/**
	 * Find all of the localized packages under the given roots, and update the map with the result.
	 *
	 * @param InSourceRoot		The root package path that contains the source packages we're finding localized packages for, eg) /Game
	 * @param InLocalizedRoot	The root package path to search for localized packages in, eg) /Game/L10N/fr
	 * @param InOutSourcePackagesToLocalizedPackages The map to update. This will be passed to multiple calls of FindLocalizedPackages in order to build a mapping between a source package, and an array of prioritized localized packages.
	 */
	virtual void FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages) = 0;

	/**
	 * Find all of the packages using the given asset group class, and update the PackageNameToAssetGroup map with the result.
	 *
	 * @param InAssetGroupName	The name of the asset group packages of this type belong to.
	 * @param InAssetClassName	The name of a class used by this asset group.
	 */
	virtual void FindAssetGroupPackages(const FName InAssetGroupName, const FName InAssetClassName) = 0;

	/**
	 * Try and find an existing cache for the given culture name, and create an entry for one if no such cache currently exists.
	 *
	 * @param InCultureName		The name of the culture to find the cache for.
	 *
	 * @return A pointer to the cache for the given culture.
	 */
	TSharedPtr<FPackageLocalizationCultureCache> FindOrAddCacheForCulture_NoLock(const FString& InCultureName);

	/**
	 * Update the mapping of package names to asset groups (if required).
	 */
	void ConditionalUpdatePackageNameToAssetGroupCache_NoLock();

	/**
	 * Callback handler for when a new content path is mounted.
	 *
	 * @param InAssetPath		The package path that was mounted, eg) /Game
	 * @param InFilesystemPath	The file-system path for the asset path, eg) ../../../MyGame/Content
	 */
	void HandleContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath);

	/**
	 * Callback handler for when an existing content path is dismounted.
	 *
	 * @param InAssetPath		The package path that was dismounted, eg) /Game
	 * @param InFilesystemPath	The file-system path for the asset path, eg) ../../../MyGame/Content
	 */
	void HandleContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath);

	/**
	 * Callback handler for when the active culture is changed.
	 */
	void HandleCultureChanged();

protected:
	/** Critical section preventing concurrent access to our data. */
	mutable FCriticalSection LocalizedCachesCS;

	/** Pointer to the culture specific cache for the current culture. */
	TSharedPtr<FPackageLocalizationCultureCache> CurrentCultureCache;

	/** Mapping between a culture name, and the culture specific cache for that culture. */
	TArray<TTuple<FString, TSharedPtr<FPackageLocalizationCultureCache>>> AllCultureCaches;

	/** Mapping between a class name, and the asset group the class belongs to (for class specific package localization). */
	TArray<TTuple<FName, FName>> AssetClassesToAssetGroups;

	/** Mapping between a package name, and the asset group it belongs to. */
	TMap<FName, FName> PackageNameToAssetGroup;
	bool bPackageNameToAssetGroupDirty;
};
