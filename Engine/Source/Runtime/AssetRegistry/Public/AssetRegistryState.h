// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "Misc/AssetRegistryInterface.h"

class FDependsNode;
struct FARFilter;

/** Load/Save options used to modify how the cache is serialized. These are read out of the AssetRegistry section of Engine.ini and can be changed per platform. */
struct FAssetRegistrySerializationOptions
{
	/** True rather to load/save registry at all */
	bool bSerializeAssetRegistry;

	/** True rather to load/save dependency info. If true this will handle hard and soft package references */
	bool bSerializeDependencies;

	/** True rather to load/save dependency info for Name references,  */
	bool bSerializeSearchableNameDependencies;

	/** True rather to load/save dependency info for Manage references,  */
	bool bSerializeManageDependencies;

	/** If true will read/write FAssetPackageData */
	bool bSerializePackageData;

	/** True if CookFilterlistTagsByClass is a whitelist. False if it is a blacklist. */
	bool bUseAssetRegistryTagsWhitelistInsteadOfBlacklist;

	/** True if we want to only write out asset data if it has valid tags. This saves memory by not saving data for things like textures */
	bool bFilterAssetDataWithNoTags;

	/** The map of classname to tag set of tags that are allowed in cooked builds. This is either a whitelist or blacklist depending on bUseAssetRegistryTagsWhitelistInsteadOfBlacklist */
	TMap<FName, TSet<FName>> CookFilterlistTagsByClass;

	FAssetRegistrySerializationOptions()
		: bSerializeAssetRegistry(false)
		, bSerializeDependencies(false)
		, bSerializeSearchableNameDependencies(false)
		, bSerializeManageDependencies(false)
		, bSerializePackageData(false)
		, bUseAssetRegistryTagsWhitelistInsteadOfBlacklist(false)
		, bFilterAssetDataWithNoTags(false)
	{}

	/** Options used to read/write the DevelopmentAssetRegistry, which includes all data */
	void ModifyForDevelopment()
	{
		bSerializeAssetRegistry = bSerializeDependencies = bSerializeSearchableNameDependencies = bSerializeManageDependencies = bSerializePackageData = true;
		bFilterAssetDataWithNoTags = false;
	}
};

/** The state of an asset registry, this is used internally by IAssetRegistry to represent the disk cache, and is also accessed directly to save/load cooked caches */
class ASSETREGISTRY_API FAssetRegistryState
{
public:
	FAssetRegistryState();
	~FAssetRegistryState();

	/**
	 * Does the given path contain assets?
	 * @note This function doesn't recurse into sub-paths.
	 */
	 bool HasAssets(const FName PackagePath) const;

	/**
	 * Gets asset data for all assets that match the filter.
	 * Assets returned must satisfy every filter component if there is at least one element in the component's array.
	 * Assets will satisfy a component if they match any of the elements in it.
	 *
	 * @param Filter filter to apply to the assets in the AssetRegistry
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param OutAssetData the list of assets in this path
	 */
	bool GetAssets(const FARFilter& Filter, const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData) const;

	/**
	 * Gets asset data for all assets in the registry state.
	 *
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param OutAssetData the list of assets
	 */
	bool GetAllAssets(const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData) const;

	/**
	 * Gets a list of packages and searchable names that are referenced by the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutDependencies	a list of things that are referenced by AssetIdentifier
	 * @param InDependencyType	which kinds of dependency to include in the output list
	 */
	bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::All) const;

	/**
	 * Gets a list of packages and searchable names that reference the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutReferencers	a list of things that reference AssetIdentifier
	 * @param InReferenceType	which kinds of reference to include in the output list
	 */
	bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType = EAssetRegistryDependencyType::All) const;

	/**
	 * Gets the asset data for the specified object path
	 *
	 * @param ObjectPath the path of the object to be looked up
	 * @return the assets data, null if not found
	 */
	const FAssetData* GetAssetByObjectPath(const FName ObjectPath) const
	{
		FAssetData* const* FoundAsset = CachedAssetsByObjectPath.Find(ObjectPath);
		if (FoundAsset)
		{
			return *FoundAsset;
		}

		return nullptr;
	}

	/**
	 * Gets the asset data for the specified package name
	 *
	 * @param PackageName the path of the package to be looked up
	 * @return an array of AssetData*, empty if nothing found
	 */
	const TArray<const FAssetData*>& GetAssetsByPackageName(const FName PackageName) const
	{
		static TArray<const FAssetData*> InvalidArray;
		const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByPackageName.Find(PackageName);
		if (FoundAssetArray)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(*FoundAssetArray);
		}

		return InvalidArray;
	}

	/**
	 * Gets the asset data for the specified asset class
	 *
	 * @param ClassName the class name of the assets to look for
	 * @return An array of AssetData*, empty if nothing found
	 */
	const TArray<const FAssetData*>& GetAssetsByClassName(const FName ClassName) const
	{
		static TArray<const FAssetData*> InvalidArray;
		const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByClass.Find(ClassName);
		if (FoundAssetArray)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(*FoundAssetArray);
		}

		return InvalidArray;
	}

	/**
	 * Gets the asset data for the specified asset tag
	 *
	 * @param TagName the tag name to search for 
	 * @return An array of AssetData*, empty if nothing found
	 */
	const TArray<const FAssetData*>& GetAssetsByTagName(const FName TagName) const
	{
		static TArray<const FAssetData*> InvalidArray;
		const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByTag.Find(TagName);
		if (FoundAssetArray)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(*FoundAssetArray);
		}

		return InvalidArray;
	}

	/** Returns const version of internal ObjectPath->AssetData map for fast iteration */
	const TMap<FName, const FAssetData*>& GetObjectPathToAssetDataMap() const
	{
		return reinterpret_cast<const TMap<FName, const FAssetData*>&>(CachedAssetsByObjectPath);
	}

	/** Returns const version of internal PackageName->PackageData map for fast iteration */
	const TMap<FName, const FAssetPackageData*>& GetAssetPackageDataMap() const
	{
		return reinterpret_cast<const TMap<FName, const FAssetPackageData*>&>(CachedPackageData);
	}

	/** Returns non-editable pointer to the asset package data */
	const FAssetPackageData* GetAssetPackageData(FName PackageName) const;

	/** Finds an existing package data, or creates a new one to modify */
	FAssetPackageData* CreateOrGetAssetPackageData(FName PackageName);

	/** Removes existing package data */
	bool RemovePackageData(FName PackageName);

	/** Adds the asset data to the lookup maps */
	void AddAssetData(FAssetData* AssetData);

	/** Updates an existing asset data with the new value and updates lookup maps */
	void UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData);

	/** Removes the asset data from the lookup maps */
	bool RemoveAssetData(FAssetData* AssetData);

	/** Resets to default state */
	void Reset();

	/** Initializes cache from existing set of asset data and depends nodes */
	void InitializeFromExisting(const TMap<FName, FAssetData*>& AssetDataMap, const TMap<FAssetIdentifier, FDependsNode*>& DependsNodeMap, const TMap<FName, FAssetPackageData*>& AssetPackageDataMap, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting = false);
	void InitializeFromExisting(const FAssetRegistryState& Existing, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting = false)
	{
		InitializeFromExisting(Existing.CachedAssetsByObjectPath, Existing.CachedDependsNodes, Existing.CachedPackageData, Options, bRefreshExisting);
	}

	/** 
	 * Prunes an asset cache, this removes asset data, nodes, and package data that isn't needed. 
	 * @param RequiredPackages If set, only these packages will be maintained. If empty it will keep all unless filtered by other parameters
	 * @param RemovePackages These packages will be removed from the current set
	 * @param bFilterAssetDataWithNoTags If true, any AssetData with no Tags will be removed
	 */
	void PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, bool bFilterAssetDataWithNoTags = false);

	/** Serialize the registry to/from a file, skipping editor only data */
	bool Serialize(FArchive& Ar, FAssetRegistrySerializationOptions& Options);

	/** Returns memory size of entire registry, optionally logging sizes */
	uint32 GetAllocatedSize(bool bLogDetailed = false) const;

	/** Checks a filter to make sure there are no illegal entries */
	static bool IsFilterValid(const FARFilter& Filter, bool bAllowRecursion);

private:
	/** Find the first non-redirector dependency node starting from InDependency. */
	FDependsNode* ResolveRedirector(FDependsNode* InDependency, TMap<FName, FAssetData*>& InAllowedAssets, TMap<FDependsNode*, FDependsNode*>& InCache);

	/** Finds an existing node for the given package and returns it, or returns null if one isn't found */
	FDependsNode* FindDependsNode(const FAssetIdentifier& Identifier);

	/** Creates a node in the CachedDependsNodes map or finds the existing node and returns it */
	FDependsNode* CreateOrFindDependsNode(const FAssetIdentifier& Identifier);

	/** Removes the depends node and updates the dependencies to no longer contain it as as a referencer. */
	bool RemoveDependsNode(const FAssetIdentifier& Identifier);

	/** The map of ObjectPath names to asset data for assets saved to disk */
	TMap<FName, FAssetData*> CachedAssetsByObjectPath;

	/** The map of package names to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByPackageName;

	/** The map of long package path to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByPath;

	/** The map of class name to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByClass;

	/** The map of asset tag to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByTag;

	/** A map of object names to dependency data */
	TMap<FAssetIdentifier, FDependsNode*> CachedDependsNodes;

	/** A map of Package Names to Package Data */
	TMap<FName, FAssetPackageData*> CachedPackageData;

	/** When loading a registry from disk, we can allocate all the FAssetData objects in one chunk, to save on 10s of thousands of heap allocations */
	TArray<FAssetData*> PreallocatedAssetDataBuffers;
	TArray<FDependsNode*> PreallocatedDependsNodeDataBuffers;
	TArray<FAssetPackageData*> PreallocatedPackageDataBuffers;

	/** Counters for asset/depends data memory allocation to ensure that every FAssetData and FDependsNode created is deleted */
	int32 NumAssets;
	int32 NumDependsNodes;
	int32 NumPackageData;

	friend class UAssetRegistryImpl;
};
