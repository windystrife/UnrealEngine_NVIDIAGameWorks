// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "Misc/AssetRegistryInterface.h"
#include "Interface.h"
#include "ARFilter.h"
#include "IAssetRegistry.generated.h"


struct FARFilter;
struct FAssetRegistrySerializationOptions;
class FAssetRegistryState;

namespace EAssetAvailability
{
	enum Type
	{
		DoesNotExist,	// asset chunkid does not exist
		NotAvailable,	// chunk containing asset has not been installed yet
		LocalSlow,		// chunk containing asset is on local slow media (optical)
		LocalFast		// chunk containing asset is on local fast media (HDD)
	};
}

namespace EAssetAvailabilityProgressReportingType
{
	enum Type
	{
		ETA,					// time remaining in seconds
		PercentageComplete		// percentage complete in 99.99 format
	};
}

namespace EAssetSetManagerResult
{
	enum Type
	{
		DoNotSet,			// Do not set manager
		SetButDoNotRecurse,	// Set but do not recurse
		SetAndRecurse		// Set and recurse into reference
	};
}

namespace EAssetSetManagerFlags
{
	enum Type
	{		
		IsDirectSet = 1,				// This attempt is a direct set instead of a recursive set
		TargetHasExistingManager = 2,	// Target already has a manager from previous run
		TargetHasDirectManager = 4,		// Target has another direct manager that will be set in this run
	};
}


UINTERFACE(MinimalApi, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UAssetRegistry : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IAssetRegistry
{
	GENERATED_IINTERFACE_BODY()
public:
	/**
	 * Does the given path contain assets, optionally also testing sub-paths?
	 *
	 * @param PackagePath the path to query asset data in
	 * @param bRecursive if true, the supplied path will be tested recursively
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual bool HasAssets(const FName PackagePath, const bool bRecursive = false) const = 0;

	/**
	 * Gets asset data for the assets in the package with the specified package name
	 *
	 * @param PackageName the package name for the requested assets
	 * @param OutAssetData the list of assets in this path
	 */
	UFUNCTION(BlueprintCallable, Category="AssetRegistry")
	virtual bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const = 0;

	/**
	 * Gets asset data for all assets in the supplied folder path
	 *
	 * @param PackagePath the path to query asset data in
	 * @param OutAssetData the list of assets in this path
	 * @param bRecursive if true, all supplied paths will be searched recursively
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const = 0;

	/**
	 * Gets asset data for all assets with the supplied class
	 *
	 * @param ClassName the class name of the assets requested
	 * @param OutAssetData the list of assets in this path
	 * @param bSearchSubClasses if true, all subclasses of the passed in class will be searched as well
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual bool GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses = false) const = 0;

	/**
	 * Gets asset data for all assets with the supplied tags and values
	 *
	 * @param AssetTagsAndValues the tags and values associated with the assets requested
	 * @param OutAssetData the list of assets in this path
	 */
	virtual bool GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const = 0;

	/**
	 * Gets asset data for all assets that match the filter.
	 * Assets returned must satisfy every filter component if there is at least one element in the component's array.
	 * Assets will satisfy a component if they match any of the elements in it.
	 *
	 * @param Filter filter to apply to the assets in the AssetRegistry
	 * @param OutAssetData the list of assets in this path
	 */
	UFUNCTION(BlueprintCallable, Category="AssetRegistry")
	virtual bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData) const = 0;

	/**
	 * Gets the asset data for the specified object path
	 *
	 * @param ObjectPath the path of the object to be looked up
	 * @param bIncludeOnlyOnDiskAssets if true, in-memory objects will be ignored. The call will be faster.
	 * @return the assets data;Will be invalid if object could not be found
	 */
	UFUNCTION(BlueprintCallable, Category="AssetRegistry")
	virtual FAssetData GetAssetByObjectPath( const FName ObjectPath, bool bIncludeOnlyOnDiskAssets = false ) const = 0;

	/**
	 * Gets asset data for all assets in the registry.
	 * This method may be slow, use a filter if possible to avoid iterating over the entire registry.
	 *
	 * @param OutAssetData the list of assets in this path
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const = 0;

	/**
	 * Gets a list of packages and searchable names that are referenced by the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutDependencies	a list of things that are referenced by AssetIdentifier
	 * @param InDependencyType	which kinds of dependency to include in the output list
	 */
	virtual bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::All) const = 0;

	/**
	 * Gets a list of paths to objects that are referenced by the supplied package. (On disk references ONLY)
	 *
	 * @param PackageName		the name of the package for which to gather dependencies
	 * @param OutDependencies	a list of packages that are referenced by the package whose path is PackageName
	 * @param InDependencyType	which kinds of dependency to include in the output list
	 */
	virtual bool GetDependencies(FName PackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::Packages) const = 0;

	/**
	 * Gets a list of packages and searchable names that reference the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutReferencers	a list of things that reference AssetIdentifier
	 * @param InReferenceType	which kinds of reference to include in the output list
	 */
	virtual bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType = EAssetRegistryDependencyType::All) const = 0;

	/**
	 * Gets a list of packages that reference the supplied package. (On disk references ONLY)
	 *
	 * @param PackageName		the name of the package for which to gather dependencies
	 * @param OutReferencers	a list of packages that reference the package whose path is PackageName
	 * @param InReferenceType	which kinds of reference to include in the output list
	 */
	virtual bool GetReferencers(FName PackageName, TArray<FName>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType = EAssetRegistryDependencyType::Packages) const = 0;

	/** Finds Package data for a package name. This data is only updated on save and can only be accessed for valid packages */
	virtual const FAssetPackageData* GetAssetPackageData(FName PackageName) const = 0;

	/** Uses the asset registry to look for ObjectRedirectors. This will follow the chain of redirectors. It will return the original path if no redirectors are found */
	virtual FName GetRedirectedObjectPath(const FName ObjectPath) const = 0;

	/** Returns true if the specified ClassName's ancestors could be found. If so, OutAncestorClassNames is a list of all its ancestors */
	virtual bool GetAncestorClassNames(FName ClassName, TArray<FName>& OutAncestorClassNames) const = 0;

	/** Returns the names of all classes derived by the supplied class names, excluding any classes matching the excluded class names. */
	virtual void GetDerivedClassNames(const TArray<FName>& ClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& OutDerivedClassNames) const = 0;

	/** Gets a list of all paths that are currently cached */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual void GetAllCachedPaths(TArray<FString>& OutPathList) const = 0;

	/** Gets a list of all paths that are currently cached below the passed-in base path */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual void GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const = 0;

	/** Trims items out of the asset data list that do not pass the supplied filter */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual void RunAssetsThroughFilter(TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const = 0;

	/** Modifies passed in filter to make it safe for use on FAssetRegistryState. This expands recursive paths and classes */
	virtual void ExpandRecursiveFilter(const FARFilter& InFilter, FARFilter& ExpandedFilter) const = 0;

	/**
	 * Gets the current availability of an asset, primarily for streaming install purposes.
	 *
	 * @param FAssetData the asset to check for availability
	 */
	virtual EAssetAvailability::Type GetAssetAvailability(const FAssetData& AssetData) const = 0;

	/**
	 * Gets an ETA or percentage complete for an asset that is still in the process of being installed.
	 *
	 * @param FAssetData the asset to check for progress status
	 * @param ReportType the type of report to query.
	 */
	virtual float GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const = 0;

	/**
	 * @param ReportType The report type to query.
	 * Returns if a given report type is supported on the current platform
	 */
	virtual bool GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const = 0;	

	/**
	 * Hint the streaming installers to prioritize a specific asset for install.
	 *
	 * @param FAssetData the asset which needs to have installation prioritized
	 */
	virtual void PrioritizeAssetInstall(const FAssetData& AssetData) const = 0;

	/** Adds the specified path to the set of cached paths. These will be returned by GetAllCachedPaths(). Returns true if the path was actually added and false if it already existed. */
	virtual bool AddPath(const FString& PathToAdd) = 0;

	/** Attempts to remove the specified path to the set of cached paths. This will only succeed if there are no assets left in the specified path. */
	virtual bool RemovePath(const FString& PathToRemove) = 0;

	/** Scan the supplied paths recursively right now and populate the asset registry. If bForceRescan is true, the paths will be scanned again, even if they were previously scanned */
	virtual void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan = false) = 0;

	/** Scan the specified individual files right now and populate the asset registry. If bForceRescan is true, the paths will be scanned again, even if they were previously scanned */
	virtual void ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan = false) = 0;

	/** Look for all assets on disk (can be async or synchronous) */
	virtual void SearchAllAssets(bool bSynchronousSearch) = 0;

	/** If assets are currently being asynchronously scanned in the specified path, this will cause them to be scanned before other assets. */
	virtual void PrioritizeSearchPath(const FString& PathToPrioritize) = 0;

	/** Forces a rescan of specific filenames, call this when you need to refresh from disk */
	virtual void ScanModifiedAssetFiles(const TArray<FString>& InFilePaths) = 0;

	/** Event for when paths are added to the registry */
	DECLARE_EVENT_OneParam( IAssetRegistry, FPathAddedEvent, const FString& /*Path*/ );
	virtual FPathAddedEvent& OnPathAdded() = 0;

	/** Event for when paths are removed from the registry */
	DECLARE_EVENT_OneParam( IAssetRegistry, FPathRemovedEvent, const FString& /*Path*/ );
	virtual FPathRemovedEvent& OnPathRemoved() = 0;

	/** Informs the asset registry that an in-memory asset has been created */
	virtual void AssetCreated (UObject* NewAsset) = 0;

	/** Informs the asset registry that an in-memory asset has been deleted */
	virtual void AssetDeleted (UObject* DeletedAsset) = 0;

	/** Informs the asset registry that an in-memory asset has been renamed */
	virtual void AssetRenamed (const UObject* RenamedAsset, const FString& OldObjectPath) = 0;

	/** Informs the asset registry that an in-memory package has been deleted, and all associated assets should be removed */
	virtual void PackageDeleted (UPackage* DeletedPackage) = 0;

	/** Event for when assets are added to the registry */
	DECLARE_EVENT_OneParam( IAssetRegistry, FAssetAddedEvent, const FAssetData& );
	virtual FAssetAddedEvent& OnAssetAdded() = 0;

	/** Event for when assets are removed from the registry */
	DECLARE_EVENT_OneParam( IAssetRegistry, FAssetRemovedEvent, const FAssetData& );
	virtual FAssetRemovedEvent& OnAssetRemoved() = 0;

	/** Event for when assets are renamed in the registry */
	DECLARE_EVENT_TwoParams( IAssetRegistry, FAssetRenamedEvent, const FAssetData&, const FString& );
	virtual FAssetRenamedEvent& OnAssetRenamed() = 0;

	/** Event for when in-memory assets are created */
	DECLARE_EVENT_OneParam( IAssetRegistry, FInMemoryAssetCreatedEvent, UObject* );
	virtual FInMemoryAssetCreatedEvent& OnInMemoryAssetCreated() = 0;

	/** Event for when assets are deleted */
	DECLARE_EVENT_OneParam( IAssetRegistry, FInMemoryAssetDeletedEvent, UObject* );
	virtual FInMemoryAssetDeletedEvent& OnInMemoryAssetDeleted() = 0;

	/** Event for when the asset registry is done loading files */
	DECLARE_EVENT( IAssetRegistry, FFilesLoadedEvent );
	virtual FFilesLoadedEvent& OnFilesLoaded() = 0;

	/** Payload data for a file progress update */
	struct FFileLoadProgressUpdateData
	{
		FFileLoadProgressUpdateData(int32 InNumTotalAssets, int32 InNumAssetsProcessedByAssetRegistry, int32 InNumAssetsPendingDataLoad, bool InIsDiscoveringAssetFiles)
			: NumTotalAssets(InNumTotalAssets)
			, NumAssetsProcessedByAssetRegistry(InNumAssetsProcessedByAssetRegistry)
			, NumAssetsPendingDataLoad(InNumAssetsPendingDataLoad)
			, bIsDiscoveringAssetFiles(InIsDiscoveringAssetFiles)
		{
		}

		int32 NumTotalAssets;
		int32 NumAssetsProcessedByAssetRegistry;
		int32 NumAssetsPendingDataLoad;
		bool bIsDiscoveringAssetFiles;
	};

	/** Event to update the progress of the background file load */
	DECLARE_EVENT_OneParam( IAssetRegistry, FFileLoadProgressUpdatedEvent, const FFileLoadProgressUpdateData& /*ProgressUpdateData*/ );
	virtual FFileLoadProgressUpdatedEvent& OnFileLoadProgressUpdated() = 0;

	/** Register callback for when someone tries to edit a searchable name */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FAssetEditSearchableNameDelegate, const FAssetIdentifier&);
	virtual FAssetEditSearchableNameDelegate& OnEditSearchableName(FName PackageName, FName ObjectName) = 0;

	/** Tries to edit a searchablename, returns true if any of the callbacks handled it */
	virtual bool EditSearchableName(const FAssetIdentifier& SearchableName) = 0;

	/** Returns true if the asset registry is currently loading files and does not yet know about all assets */
	UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
	virtual bool IsLoadingAssets() const = 0;

	/** Tick the asset registry */
	virtual void Tick (float DeltaTime) = 0;

	/** Serialize the registry to/from a file, skipping editor only data */
	virtual void Serialize(FArchive& Ar) = 0;

	/** Returns memory size of entire registry, optionally logging sizes */
	virtual uint32 GetAllocatedSize(bool bLogDetailed = false) const = 0;

	/**
	 * Fills in a AssetRegistryState with a copy of the data in the internal cache, overriding some
	 *
	 * @param OutState			This will be filled in with a copy of the asset data, platform data, and dependency data
	 * @param Options			Serialization options that will be used to write this later
	 * @param bRefreshExisting	If true, will not delete or add packages in OutState and will just update things that already exist
	 * @param OverrideData		Map of ObjectPath to AssetData. If non empty, it will use this map of AssetData, and will filter Platform/Dependency data to only include this set
	 */
	virtual void InitializeTemporaryAssetRegistryState(FAssetRegistryState& OutState, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting = false, const TMap<FName, FAssetData*>& OverrideData = TMap<FName, FAssetData*>()) const = 0;

	/** Fills in FAssetRegistrySerializationOptions from ini, optionally using a target platform ini name */
	virtual void InitializeSerializationOptions(FAssetRegistrySerializationOptions& Options, const FString& PlatformIniName = FString()) const = 0;

	/** Load FPackageRegistry data from the supplied package */
	virtual void LoadPackageRegistryData(FArchive& Ar, TArray<FAssetData*>& Data) const = 0;

	DEPRECATED(4.16, "Deprecated. Use InitializeTemporaryAssetRegistryState and call Serialize on it directly")
	virtual void SaveRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data, TArray<FName>* InMaps = nullptr) = 0;
	
	DEPRECATED(4.16, "Deprecated. Create a FAssetRegistryState and call Serialize on it directly")
	virtual void LoadRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data) = 0;

protected:
	// Functions specifically for calling from the asset manager
	friend class UAssetManager;
	
	/**
	 * Predicate called to decide rather to recurse into a reference when setting manager
	 *
	 * @param Manager			Identifier of what manager will be set
	 * @param Source			Identifier of the reference currently being iterated
	 * @param Target			Identifier that will managed by manager
	 * @param DependencyType	Type of dependencies to recurse over
	 * @param Flags				Flags describing this particular set attempt
	 */
	typedef TFunction<EAssetSetManagerResult::Type(const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target, EAssetRegistryDependencyType::Type DependencyType, EAssetSetManagerFlags::Type Flags)> ShouldSetManagerPredicate;

	/**
	 * Specifies a list of manager mappings, optionally recursing to dependencies. These mappings can then be queried later to see which assets "manage" other assets
	 * This function is only meant to be called by the AssetManager, calls from anywhere else will conflict and lose data
	 *
	 * @param ManagerMap		Map of Managing asset to Managed asset. This will construct Manager references and clear existing 
	 * @param bClearExisting	If true, will clear any existing manage dependencies
	 * @param RecurseType		Dependency types to recurse into, from the value of the manager map
	 * @param RecursePredicate	Predicate that is called on recursion if bound, return true if it should recurse into that node
	 */
	virtual void SetManageReferences(const TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap, bool bClearExisting, EAssetRegistryDependencyType::Type RecurseType, ShouldSetManagerPredicate ShouldSetManager = nullptr) = 0;

	/** Sets the PrimaryAssetId for a specific asset. This should only be called by the AssetManager, and is needed when the AssetManager is more up to date than the on disk Registry */
	virtual bool SetPrimaryAssetIdForObjectPath(const FName ObjectPath, FPrimaryAssetId PrimaryAssetId) = 0;

	/** Returns pointer to cached AssetData for an object path. This is always the on disk version. This will return null if not found, and is exposed for  */
	virtual const FAssetData* GetCachedAssetDataForObjectPath(const FName ObjectPath) const = 0;
};
