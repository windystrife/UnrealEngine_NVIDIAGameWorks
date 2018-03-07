// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "IAssetRegistry.h"
#include "AssetRegistryState.h"
#include "PathTree.h"
#include "PackageDependencyData.h"
#include "AssetDataGatherer.h"
#include "BackgroundGatherResults.h"
#include "AssetRegistry.generated.h"

class FDependsNode;
struct FARFilter;

/**
 * The AssetRegistry singleton gathers information about .uasset files in the background so things
 * like the content browser don't have to work with the filesystem
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS

UCLASS(transient)
class UAssetRegistryImpl : public UObject, public IAssetRegistry
{
	GENERATED_BODY()
public:
	UAssetRegistryImpl(const FObjectInitializer& ObjectInitializer);
	virtual ~UAssetRegistryImpl();

	/** Gets the asset registry singleton for asset registry module use */
	static UAssetRegistryImpl& Get();

	// IAssetRegistry implementation
	virtual bool HasAssets(const FName PackagePath, const bool bRecursive = false) const override;
	virtual bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses = false) const override;
	virtual bool GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const override;
	virtual bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData) const override;
	virtual FAssetData GetAssetByObjectPath( const FName ObjectPath, bool bIncludeOnlyOnDiskAssets = false ) const override;
	virtual bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::All) const override;
	virtual bool GetDependencies(FName PackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::Packages) const override;
	virtual bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType = EAssetRegistryDependencyType::All) const override;
	virtual bool GetReferencers(FName PackageName, TArray<FName>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType = EAssetRegistryDependencyType::Packages) const override;
	virtual const FAssetPackageData* GetAssetPackageData(FName PackageName) const override;
	virtual FName GetRedirectedObjectPath(const FName ObjectPath) const override;
	virtual bool GetAncestorClassNames(FName ClassName, TArray<FName>& OutAncestorClassNames) const override;
	virtual void GetDerivedClassNames(const TArray<FName>& ClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& OutDerivedClassNames) const override;
	virtual void GetAllCachedPaths(TArray<FString>& OutPathList) const override;
	virtual void GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const override;
	virtual void RunAssetsThroughFilter (TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const override;
	virtual void ExpandRecursiveFilter(const FARFilter& InFilter, FARFilter& ExpandedFilter) const override;
	virtual EAssetAvailability::Type GetAssetAvailability(const FAssetData& AssetData) const override;	
	virtual float GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual bool GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual void PrioritizeAssetInstall(const FAssetData& AssetData) const override;
	virtual bool AddPath(const FString& PathToAdd) override;
	virtual bool RemovePath(const FString& PathToRemove) override;
	virtual void SearchAllAssets(bool bSynchronousSearch) override;
	virtual void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan = false) override;
	virtual void ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan = false) override;
	virtual void PrioritizeSearchPath(const FString& PathToPrioritize) override;
	virtual void ScanModifiedAssetFiles(const TArray<FString>& InFilePaths) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize(bool bLogDetailed = false) const override;
	virtual void LoadPackageRegistryData(FArchive& Ar, TArray<FAssetData*>& Data) const override;
	virtual void InitializeTemporaryAssetRegistryState(FAssetRegistryState& OutState, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting = false, const TMap<FName, FAssetData*>& OverrideData = TMap<FName, FAssetData*>()) const override;
	virtual void InitializeSerializationOptions(FAssetRegistrySerializationOptions& Options, const FString& PlatformIniName = FString()) const override;

	virtual void SaveRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data, TArray<FName>* InMaps = nullptr) override;
	virtual void LoadRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data) override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FPathAddedEvent, FPathAddedEvent);
	virtual FPathAddedEvent& OnPathAdded() override { return PathAddedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FPathRemovedEvent, FPathRemovedEvent);
	virtual FPathRemovedEvent& OnPathRemoved() override { return PathRemovedEvent; }

	virtual void AssetCreated(UObject* NewAsset) override;
	virtual void AssetDeleted(UObject* DeletedAsset) override;
	virtual void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath) override;

	virtual void PackageDeleted(UPackage* DeletedPackage) override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetAddedEvent, FAssetAddedEvent);
	virtual FAssetAddedEvent& OnAssetAdded() override { return AssetAddedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetRemovedEvent, FAssetRemovedEvent);
	virtual FAssetRemovedEvent& OnAssetRemoved() override { return AssetRemovedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetRenamedEvent, FAssetRenamedEvent);
	virtual FAssetRenamedEvent& OnAssetRenamed() override { return AssetRenamedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FInMemoryAssetCreatedEvent, FInMemoryAssetCreatedEvent );
	virtual FInMemoryAssetCreatedEvent& OnInMemoryAssetCreated() override { return InMemoryAssetCreatedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FInMemoryAssetDeletedEvent, FInMemoryAssetDeletedEvent );
	virtual FInMemoryAssetDeletedEvent& OnInMemoryAssetDeleted() override { return InMemoryAssetDeletedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FFilesLoadedEvent, FFilesLoadedEvent );
	virtual FFilesLoadedEvent& OnFilesLoaded() override { return FileLoadedEvent; }

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FFileLoadProgressUpdatedEvent, FFileLoadProgressUpdatedEvent );
	virtual FFileLoadProgressUpdatedEvent& OnFileLoadProgressUpdated() override { return FileLoadProgressUpdatedEvent; }

	virtual IAssetRegistry::FAssetEditSearchableNameDelegate& OnEditSearchableName(FName PackageName, FName ObjectName) override;
	virtual bool EditSearchableName(const FAssetIdentifier& SearchableName) override;

	virtual bool IsLoadingAssets() const override;

	virtual void Tick (float DeltaTime) override;

	DEPRECATED(4.17, "IsUsingWorldAssets is now always true, remove any code that assumes it could be false")
	static bool IsUsingWorldAssets() { return true; }

protected:
	virtual void SetManageReferences(const TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap, bool bClearExisting, EAssetRegistryDependencyType::Type RecurseType, ShouldSetManagerPredicate ShouldSetManager) override;
	virtual bool SetPrimaryAssetIdForObjectPath(const FName ObjectPath, FPrimaryAssetId PrimaryAssetId) override;
	virtual const FAssetData* GetCachedAssetDataForObjectPath(const FName ObjectPath) const override;

private:

	/** Internal handler for ScanPathsSynchronous */
	void ScanPathsAndFilesSynchronous(const TArray<FString>& InPaths, const TArray<FString>& InSpecificFiles, bool bForceRescan, EAssetDataCacheMode AssetDataCacheMode);
	void ScanPathsAndFilesSynchronous(const TArray<FString>& InPaths, const TArray<FString>& InSpecificFiles, bool bForceRescan, EAssetDataCacheMode AssetDataCacheMode, TArray<FName>* OutFoundAssets, TArray<FName>* OutFoundPaths);

	/** Called every tick to when data is retrieved by the background asset search. If TickStartTime is < 0, the entire list of gathered assets will be cached. Also used in sychronous searches */
	void AssetSearchDataGathered(const double TickStartTime, TBackgroundGatherResults<FAssetData*>& AssetResults);

	/** Called every tick when data is retrieved by the background path search. If TickStartTime is < 0, the entire list of gathered assets will be cached. Also used in sychronous searches */
	void PathDataGathered(const double TickStartTime, TBackgroundGatherResults<FString>& PathResults);

	/** Called every tick when data is retrieved by the background dependency search */
	void DependencyDataGathered(const double TickStartTime, TBackgroundGatherResults<FPackageDependencyData>& DependsResults);

	/** Called every tick when data is retrieved by the background search for cooked packages that do not contain asset data */
	void CookedPackageNamesWithoutAssetDataGathered(const double TickStartTime, TBackgroundGatherResults<FString>& CookedPackageNamesWithoutAssetDataResults);

	/** Adds an asset to the empty package list which contains packages that have no assets left in them */
	void AddEmptyPackage(FName PackageName);

	/** Removes an asset from the empty package list because it is no longer empty */
	bool RemoveEmptyPackage(FName PackageName);

	/** Adds a path to the cached paths tree. Returns true if the path was added to the tree, as opposed to already existing in the tree */
	bool AddAssetPath(FName PathToAdd);

	/** Removes a path to the cached paths tree. Returns true if successful. */
	bool RemoveAssetPath(FName PathToRemove, bool bEvenIfAssetsStillExist = false);

	/** Helper function to return the name of an object, given the objects export text path */
	FString ExportTextPathToObjectName(const FString& InExportTextPath) const;

	/** Adds the asset data to the lookup maps */
	void AddAssetData(FAssetData* AssetData);

	/** Updates an existing asset data with the new value and updates lookup maps */
	void UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData);

	/** Removes the asset data from the lookup maps */
	bool RemoveAssetData(FAssetData* AssetData);

	/** Removes the asset data associated with this package from the look-up maps */
	void RemovePackageData(const FName PackageName);

	/**
	 * Adds a root path to be discover files in, when asynchronously scanning the disk for asset files
	 *
	 * @param	Path	The path on disk to scan
	 */
	void AddPathToSearch(const FString& Path);

	/** Adds a list of files which will be searched for asset data */
	void AddFilesToSearch (const TArray<FString>& Files);

#if WITH_EDITOR
	/** Called when a file in a content directory changes on disk */
	void OnDirectoryChanged(const TArray<struct FFileChangeData>& Files);

	/** Called when an asset is loaded, it will possibly update the cache */
	void OnAssetLoaded(UObject *AssetLoaded);

	/** Process Loaded Assets to update cache */
	void ProcessLoadedAssetsToUpdateCache(const double TickStartTime);

	/** Update Redirect collector with redirects loaded from asset registry */
	void UpdateRedirectCollector();
#endif // WITH_EDITOR

	/**
	 * Called by the engine core when a new content path is added dynamically at runtime.  This is wired to 
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathMounted( const FString& AssetPath, const FString& FileSystemPath );

	/**
	 * Called by the engine core when a content path is removed dynamically at runtime.  This is wired to 
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathDismounted( const FString& AssetPath, const FString& FileSystemPath );

	/** Called to refresh the native classes list, called at end of engine initialization */
	void RefreshNativeClasses();

	/** Returns the names of all subclasses of the class whose name is ClassName */
	void GetSubClasses(const TArray<FName>& InClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& SubClassNames) const;
	void GetSubClasses_Recursive(FName InClassName, TSet<FName>& SubClassNames, const TMap<FName, TSet<FName>>& ReverseInheritanceMap, const TSet<FName>& ExcludedClassNames) const;

	/** Finds all class names of classes capable of generating new UClasses */
	void CollectCodeGeneratorClasses();

private:
	
	/** Internal state of the cached asset registry */
	FAssetRegistryState State;

	/** Default options used for serialization */
	FAssetRegistrySerializationOptions SerializationOptions;

	/** The set of empty package names (packages which contain no assets but have not yet been saved) */
	TSet<FName> CachedEmptyPackages;

	/** The map of classes to their parents, and a map of parents to their classes */
	TMap<FName, FName> CachedInheritanceMap;

	/** If true, will cache AssetData loaded from in memory assets back into the disk cache */
	bool bUpdateDiskCacheAfterLoad;

	/** The tree of known cached paths that assets may reside within */
	FPathTree CachedPathTree;

	/** Async task that gathers asset information from disk */
	TSharedPtr< class FAssetDataGatherer > BackgroundAssetSearch;

	/** A list of results that were gathered from the background thread that are waiting to get processed by the main thread */
	TBackgroundGatherResults<FAssetData*> BackgroundAssetResults;
	TBackgroundGatherResults<FString> BackgroundPathResults;
	TBackgroundGatherResults<FPackageDependencyData> BackgroundDependencyResults;
	TBackgroundGatherResults<FString> BackgroundCookedPackageNamesWithoutAssetDataResults;

	/** The max number of results to process per tick */
	float MaxSecondsPerFrame;

	/** The delegate to execute when an asset path is added to the registry */
	FPathAddedEvent PathAddedEvent;

	/** The delegate to execute when an asset path is removed from the registry */
	FPathRemovedEvent PathRemovedEvent;

	/** The delegate to execute when an asset is added to the registry */
	FAssetAddedEvent AssetAddedEvent;

	/** The delegate to execute when an asset is removed from the registry */
	FAssetRemovedEvent AssetRemovedEvent;

	/** The delegate to execute when an asset is renamed in the registry */
	FAssetRenamedEvent AssetRenamedEvent;

	/** The delegate to execute when an in-memory asset was just created */
	FInMemoryAssetCreatedEvent InMemoryAssetCreatedEvent;

	/** The delegate to execute when an in-memory asset was just deleted */
	FInMemoryAssetDeletedEvent InMemoryAssetDeletedEvent;

	/** The delegate to execute when finished loading files */
	FFilesLoadedEvent FileLoadedEvent;

	/** The delegate to execute while loading files to update progress */
	FFileLoadProgressUpdatedEvent FileLoadProgressUpdatedEvent;

	/** Delegates to call when editing searchable name */
	TMap<FAssetIdentifier, FAssetEditSearchableNameDelegate> EditSearchableNameDelegates;

	/** The start time of the full asset search */
	double FullSearchStartTime;
	double AmortizeStartTime;
	double TotalAmortizeTime;

	/** Flag to enable/disable dependency gathering */
	bool bGatherDependsData;

	/** Flag to indicate if the initial background search has completed */
	bool bInitialSearchCompleted;

	/** A set used to ignore repeated requests to synchronously scan the same folder or file multiple times */
	TSet<FString> SynchronouslyScannedPathsAndFiles;

	/** List of all class names derived from Blueprint (including Blueprint itself) */
	TSet<FName> ClassGeneratorNames;

	/** Handles to all registered OnDirectoryChanged delegates */
	TMap<FString, FDelegateHandle> OnDirectoryChangedDelegateHandles;

	/** Handle to the registered OnDirectoryChanged delegate for the OnContentPathMounted handler */
	FDelegateHandle OnContentPathMountedOnDirectoryChangedDelegateHandle;

#if WITH_EDITOR
	/** List of loaded objects that need to be processed */
	TArray<TWeakObjectPtr<UObject>> LoadedAssetsToProcess;

	/** Objects that couldn't be processed because the asset data didn't exist, reprocess these after more directories are scanned */
	TArray<TWeakObjectPtr<UObject>> LoadedAssetsThatDidNotHaveCachedData;

	/** The set of object paths that have had their disk cache updated from the in memory version */
	TSet<FName> AssetDataObjectPathsUpdatedOnLoad;
#endif

};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
