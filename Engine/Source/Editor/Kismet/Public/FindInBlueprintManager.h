// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Engine/Blueprint.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Dom/JsonObject.h"
#include "HAL/Runnable.h"
#include "SlateFwd.h"
#include "Input/Reply.h"

struct FAssetData;
class FFindInBlueprintsResult;
class FImaginaryBlueprint;
class FImaginaryFiBData;
class FSpawnTabArgs;
class SFindInBlueprints;

#define MAX_GLOBAL_FIND_RESULTS 4

/**
 *Const values for Find-in-Blueprints to tag searchable data
 */
struct KISMET_API FFindInBlueprintSearchTags
{
	/** Properties tag, for Blueprint variables */
	static const FText FiB_Properties;

	/** Components tags */
	static const FText FiB_Components;
	static const FText FiB_IsSCSComponent;
	/** End Components tags */

	/** Nodes tag */
	static const FText FiB_Nodes;

	/** Schema Name tag, to identify the schema that a graph uses */
	static const FText FiB_SchemaName;
	/** Uber graphs tag */
	static const FText FiB_UberGraphs;
	/** Function graph tag */
	static const FText FiB_Functions;
	/** Macro graph tag */
	static const FText FiB_Macros;
	/** Sub graph tag, for any sub-graphs in a Blueprint */
	static const FText FiB_SubGraphs;

	/** Name tag */
	static const FText FiB_Name;
	/** Native Name tag */
	static const FText FiB_NativeName;
	/** Class Name tag */
	static const FText FiB_ClassName;
	/** NodeGuid tag */
	static const FText FiB_NodeGuid;
	/** Default value */
	static const FText FiB_DefaultValue;
	/** Tooltip tag */
	static const FText FiB_Tooltip;
	/** Description tag */
	static const FText FiB_Description;
	/** Comment tag */
	static const FText FiB_Comment;
	/** Path tag */
	static const FText FiB_Path;
	/** Parent Class tag */
	static const FText FiB_ParentClass;
	/** Interfaces tag */
	static const FText FiB_Interfaces;

	/** Pin type tags */

	/** Pins tag */
	static const FText FiB_Pins;

	/** Pin Category tag */
	static const FText FiB_PinCategory;
	/** Pin Sub-Category tag */
	static const FText FiB_PinSubCategory;
	/** Pin object class tag */
	static const FText FiB_ObjectClass;
	/** Pin IsArray tag */
	static const FText FiB_IsArray;
	/** Pin IsReference tag */
	static const FText FiB_IsReference;
	/** Glyph icon tag */
	static const FText FiB_Glyph;
	/** Style set the glyph belongs to */
	static const FText FiB_GlyphStyleSet;
	/** Glyph icon color tag */
	static const FText FiB_GlyphColor;

	// Identifier for metadata storage, completely unsearchable tag
	static const FText FiBMetaDataTag;
	/** End const values for Find-in-Blueprint */
};

/** Tracks data relevant to a Blueprint for searches */
struct FSearchData
{
	/** The Blueprint this search data points to, if available */
	TWeakObjectPtr<UBlueprint> Blueprint;

	/** The full Blueprint path this search data is associated with */
	FName BlueprintPath;

	/** Search data block for the Blueprint */
	FString Value;

	/** Parent Class */
	FString ParentClass;

	/** Interfaces implemented by the Blueprint */
	TArray<FString> Interfaces;

	/** Cached to determine if the Blueprint is seen as no longer valid, allows it to be cleared out next save to disk */
	bool bMarkedForDeletion;

	/** Cached ImaginaryBlueprint data for the searchable content, prevents having to re-parse every search */
	TSharedPtr< class FImaginaryBlueprint > ImaginaryBlueprint;

	/** Version of the data */
	int32 Version;

	FSearchData()
		: Blueprint(nullptr)
		, bMarkedForDeletion(false)
		, Version(0)
	{
	}

	// Adding move semantics (on Mac this counts as a user defined constructor, so I have to reimplement the default
	// copy constructor. PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS is not true on all compilers yet, so I'm implementing manually:
	FSearchData(FSearchData&& Other)
		: Blueprint(Other.Blueprint)
		, BlueprintPath(MoveTemp(Other.BlueprintPath))
		, Value(MoveTemp(Other.Value))
		, ParentClass(MoveTemp(Other.ParentClass))
		, Interfaces(MoveTemp(Other.Interfaces))
		, bMarkedForDeletion(Other.bMarkedForDeletion)
		, ImaginaryBlueprint(Other.ImaginaryBlueprint)
		, Version(Other.Version)
	{
	}

	FSearchData& operator=(FSearchData&& RHS)
	{
		if (this == &RHS)
		{
			return *this;
		}

		Blueprint = RHS.Blueprint;
		BlueprintPath = MoveTemp(RHS.BlueprintPath);
		Value = MoveTemp(RHS.Value);
		bMarkedForDeletion = RHS.bMarkedForDeletion;
		ParentClass = MoveTemp(RHS.ParentClass);
		Interfaces = MoveTemp(RHS.Interfaces);
		Version = RHS.Version;
		ImaginaryBlueprint = RHS.ImaginaryBlueprint;
		return *this;
	}

	FSearchData(const FSearchData& Other)
		: Blueprint(Other.Blueprint)
		, BlueprintPath(Other.BlueprintPath)
		, Value(Other.Value)
		, ParentClass(Other.ParentClass)
		, Interfaces(Other.Interfaces)
		, bMarkedForDeletion(Other.bMarkedForDeletion)
		, ImaginaryBlueprint(Other.ImaginaryBlueprint)
		, Version(Other.Version)
	{
	}

	FSearchData& operator=(const FSearchData& RHS)
	{
		if (this == &RHS)
		{
			return *this;
		}

		Blueprint = RHS.Blueprint;
		BlueprintPath = RHS.BlueprintPath;
		Value = RHS.Value;
		bMarkedForDeletion = RHS.bMarkedForDeletion;
		ParentClass = RHS.ParentClass;
		Interfaces = RHS.Interfaces;
		Version = RHS.Version;
		ImaginaryBlueprint = RHS.ImaginaryBlueprint;
		return *this;
	}


	~FSearchData()
	{
	}
};

/** Filters are used by functions for searching to decide whether items can call certain functions or match the requirements of a function */
enum ESearchQueryFilter
{
	BlueprintFilter = 0,
	GraphsFilter,
	UberGraphsFilter,
	FunctionsFilter,
	MacrosFilter,
	NodesFilter,
	PinsFilter,
	PropertiesFilter,
	VariablesFilter,
	ComponentsFilter,
	AllFilter, // Will search all items, when used inside of another filter it will search all sub-items of that filter
};

/** Used for external gather functions to add Key/Value pairs to be placed into Json */
struct FSearchTagDataPair
{
	FSearchTagDataPair(FText InKey, FText InValue)
		: Key(InKey)
		, Value(InValue)
	{}

	FText Key;
	FText Value;
};

enum EFiBVersion
{
	FIB_VER_BASE = 0, // All Blueprints prior to versioning will automatically be assumed to be at 0 if they have FiB data collected
	FIB_VER_VARIABLE_REFERENCE, // Variable references (FMemberReference) is collected in FiB
	FIB_VER_INTERFACE_GRAPHS, // Implemented Interface Graphs is collected in FiB

	// -----<new versions can be added before this line>-------------------------------------------------
	FIB_VER_PLUS_ONE,
	FIB_VER_LATEST = FIB_VER_PLUS_ONE - 1 // Always the last version, we want Blueprints to be at latest
};

struct KISMET_API FFiBMD
{
	static const FString FiBSearchableMD;
	static const FString FiBSearchableShallowMD;
	static const FString FiBSearchableExplicitMD;
	static const FString FiBSearchableHiddenExplicitMD;
};

////////////////////////////////////
// FFindInBlueprintsResult

/* Item that matched the search results */
class FFindInBlueprintsResult : public TSharedFromThis< FFindInBlueprintsResult >
{
public:
	/* Create a root */
	FFindInBlueprintsResult(const FText& InDisplayText);

	/* Create a listing for a search result*/
	FFindInBlueprintsResult(const FText& InDisplayText, TSharedPtr<FFindInBlueprintsResult> InParent);

	virtual ~FFindInBlueprintsResult() {}

	/* Called when user clicks on the search item */
	virtual FReply OnClick();

	/* Get Category for this search result */
	virtual FText GetCategory() const;

	/* Create an icon to represent the result */
	virtual TSharedRef<SWidget>	CreateIcon() const;

	/** Finalizes any content for the search data that was unsafe to do on a separate thread */
	virtual void FinalizeSearchData() {};

	/* Gets the comment on this node if any */
	FString GetCommentText() const;

	/** gets the blueprint housing all these search results */
	UBlueprint* GetParentBlueprint() const;

	/**
	* Parses search info for specific data important for displaying the search result in an easy to understand format
	*
	* @param	InTokens		The search tokens to check results against
	* @param	InKey			This is the tag for the data, describing what it is so special handling can occur if needed
	* @param	InValue			Compared against search query to see if it passes the filter, sometimes data is rejected because it is deemed unsearchable
	* @param	InParent		The parent search result
	*/
	virtual void ParseSearchInfo(FText InKey, FText InValue) {};

	/** Returns the Object represented by this search information give the Blueprint it can be found in */
	virtual UObject* GetObject(UBlueprint* InBlueprint) const;

	/**
	* Adds extra search info, anything that does not have a predestined place in the search result. Adds a sub-item to the searches and formats its description so the tag displays
	*
	* @param	InKey			This is the tag for the data, describing what it is so special handling can occur if needed
	* @param	InValue			Compared against search query to see if it passes the filter, sometimes data is rejected because it is deemed unsearchable
	* @param	InParent		The parent search result
	*/
	void AddExtraSearchInfo(FText InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent);

	/** Returns the display string for the row */
	FText GetDisplayString() const;

public:
	/*Any children listed under this category */
	TArray< TSharedPtr<FFindInBlueprintsResult> > Children;

	/*If it exists it is the blueprint*/
	TWeakPtr<FFindInBlueprintsResult> Parent;

	/*The display text for this item */
	FText DisplayText;

	/** Display text for comment information */
	FString CommentText;
};

typedef TSharedPtr<FFindInBlueprintsResult> FSearchResult;

////////////////////////////////////
// FStreamSearch

/**
 * Async task for searching Blueprints
 */
class FStreamSearch : public FRunnable
{
public:
	/** Constructor */
	FStreamSearch(const FString& InSearchValue);
	FStreamSearch(const FString& InSearchValue, enum ESearchQueryFilter InImaginaryDataFilter, EFiBVersion InMinimiumVersionRequirement);

	/** Begin FRunnable Interface */
	virtual bool Init() override;

	virtual uint32 Run() override;

	virtual void Stop() override;

	virtual void Exit() override;
	/** End FRunnable Interface */

	/** Brings the thread to a safe stop before continuing. */
	void EnsureCompletion();

	/** Returns TRUE if the thread is done with it's work. */
	bool IsComplete() const;

	/**
	 * Appends the items filtered through the search filter to the passed array
	 *
	 * @param OutItemsFound		All the items found since last queried
	 */
	void GetFilteredItems(TArray<TSharedPtr<class FFindInBlueprintsResult>>& OutItemsFound);

	/** Helper function to query the percent complete this search is */
	float GetPercentComplete() const;

	/** Returns the Out-of-Date Blueprint count */
	int32 GetOutOfDateCount() const
	{
		return BlueprintCountBelowVersion;
	}

	/** Returns the FilteredImaginaryResults from the search query, these results have been filtered by the ImaginaryDataFilter. */
	void GetFilteredImaginaryResults(TArray<TSharedPtr<class FImaginaryFiBData>>& OutFilteredImaginaryResults);

public:
	/** Thread to run the cleanup FRunnable on */
	FRunnableThread* Thread;

	/** A list of items found, cleared whenever the main thread pulls them to display to screen */
	TArray<TSharedPtr<class FFindInBlueprintsResult>> ItemsFound;

	/** The search value to filter results by */
	FString SearchValue;

	/** Prevents searching while other threads are pulling search results */
	FCriticalSection SearchCriticalSection;

	// Whether the thread has finished running
	bool bThreadCompleted;

	/** > 0 if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter StopTaskCounter;

	/** When searching, any Blueprint below this version will be considered out-of-date */
	EFiBVersion MinimiumVersionRequirement;

	/** A going count of all Blueprints below the MinimiumVersionRequirement */
	int32 BlueprintCountBelowVersion;

	/** Filtered (ImaginaryDataFilter) list of imaginary data results that met the search requirements */
	TArray<TSharedPtr<class FImaginaryFiBData>> FilteredImaginaryResults;

	/** Filter to limit the FilteredImaginaryResults to */
	enum ESearchQueryFilter ImaginaryDataFilter;
};

////////////////////////////////////
// FFindInBlueprintSearchManager

/** Singleton manager for handling all Blueprint searches, helps to manage the going progress of Blueprints, and is thread-safe. */
class KISMET_API FFindInBlueprintSearchManager
{
public:
	static FFindInBlueprintSearchManager* Instance;
	static FFindInBlueprintSearchManager& Get();

	FFindInBlueprintSearchManager();
	~FFindInBlueprintSearchManager();

	/**
	 * Gathers the Blueprint's search metadata and adds or updates it in the cache
	 *
	 * @param InBlueprint		Blueprint to cache the searchable data for
	 * @param bInForceReCache	Forces the Blueprint to be recache'd, regardless of what data it believes exists
	 */
	void AddOrUpdateBlueprintSearchMetadata(UBlueprint* InBlueprint, bool bInForceReCache = false);

	/**
	 * Starts a search query, the FiB manager handles where the thread is at in the search query at all times so that post-save of the cache to disk it can correct the index
	 *
	 * @param InSearchOriginator		Pointer to the thread object that the query originates from
	 */
	void BeginSearchQuery(const class FStreamSearch* InSearchOriginator);

	/**
	 * Continues a search query, returning a single piece of search data
	 *
	 * @param InSearchOriginator		Pointer to the thread object that the query originates from
	 * @param OutSearchData				Result of the search, if there is any Blueprint search data still available to query
	 * @return							TRUE if the search was successful, FALSE if the search is complete
	 */
	bool ContinueSearchQuery(const class FStreamSearch* InSearchOriginator, FSearchData& OutSearchData);

	/**
	 * This function ensures that the passed in search query ends in a safe manner. The search will no longer be valid to this manager, though it does not destroy any objects.
	 * Use this whenever the search is finished or canceled.
	 *
	 * @param InSearchOriginator	Identifying search stream to be stopped
	 */
	void EnsureSearchQueryEnds(const class FStreamSearch* InSearchOriginator);

	/**
	 * Query how far along a search thread is
	 *
	 * @param OutSearchData				Result of the search, if there is any Blueprint search data still available to query
	 * @return							Percent along the search thread is
	 */
	float GetPercentComplete(const class FStreamSearch* InSearchOriginator) const;

	/**
	 * Query for a single, specific Blueprint's search block
	 *
	 * @param InBlueprint				The Blueprint to search for
	 * @param bInRebuildSearchData		When TRUE the search data will be freshly collected
	 * @return							The search block
	 */
	FString QuerySingleBlueprint(UBlueprint* InBlueprint, bool bInRebuildSearchData = true);

	/** Converts a string of hex characters, previously converted by ConvertFTextToHexString, to an FText. */
	static FText ConvertHexStringToFText(FString InHexString);

	/** Serializes an FText to memory and converts the memory into a string of hex characters */
	static FString ConvertFTextToHexString(FText InValue);

	/** Returns the number of uncached Blueprints */
	int32 GetNumberUncachedBlueprints() const;

	/**
	 * Starts caching all uncached Blueprints at a rate of 1 per tick
	 *
	 * @param InSourceWidget				The source FindInBlueprints widget, this widget will be informed when caching is complete
	 * @param InOutActiveTimerDelegate		Binds an object that ticks every time the Find-in-Blueprints widget ticks to cache all Blueprints
	 * @param InOnFinished					Callback when caching is finished
	 * @param InMinimiumVersionRequirement	Minimum version requirement for caching, any Blueprints below this version will be re-indexed
	 */
	void CacheAllUncachedBlueprints(TWeakPtr< class SFindInBlueprints > InSourceWidget, FWidgetActiveTimerDelegate& InOutActiveTimerDelegate, FSimpleDelegate InOnFinished = FSimpleDelegate(), EFiBVersion InMinimiumVersionRequirement = EFiBVersion::FIB_VER_LATEST);
	
	/**
	 * Starts the actual caching process
	 *
	 * @param bInSourceControlActive		TRUE if source control is active
	 * @param bInCheckoutAndSave			TRUE if the system should checkout and save all assets that need to be reindexed
	 */
	void OnCacheAllUncachedBlueprints(bool bInSourceControlActive, bool bInCheckoutAndSave);

	/** Stops the caching process where it currently is at, the rest can be continued later */
	void CancelCacheAll(SFindInBlueprints* InFindInBlueprintWidget);

	/** Returns the current index in the caching */
	int32 GetCurrentCacheIndex() const;

	/** Returns the name of the current Blueprint being cached */
	FName GetCurrentCacheBlueprintName() const;

	/** Returns the progress complete on the caching */
	float GetCacheProgress() const;

	/** Returns the list of Blueprint paths that failed to cache */
	TSet<FName> GetFailedToCachePathList() const { return FailedToCachePaths; }

	/** Returns the number of Blueprints that failed to cache */
	int32 GetFailedToCacheCount() const { return FailedToCachePaths.Num(); }

	/** Returns TRUE if caching failed */
	bool HasCachingFailed() const { return FailedToCachePaths.Num() > 0; };
	/**
	 * Callback to note that Blueprint caching is complete
	 *
	 * @param InNumberCached		The number of Blueprints cached, to be chopped off the existing array so the rest (if any) can be finished later
	 */
	void FinishedCachingBlueprints(int32 InNumberCached, TSet<FName>& InFailedToCacheList);

	/** Returns TRUE if Blueprints are being cached. */
	bool IsCacheInProgress() const;

	/** Serializes an FString to memory and converts the memory into a string of hex characters */
	static FString ConvertFStringToHexString(FString InValue);

	/** Given a fully constructed Find-in-Blueprint FString of searchable data, will parse and construct a JsonObject */
	static TSharedPtr< class FJsonObject > ConvertJsonStringToObject(bool bInIsVersioned, FString InJsonString, TMap<int32, FText>& OutFTextLookupTable);

	void EnableGatheringData(bool bInEnableGatheringData) { bEnableGatheringData = bInEnableGatheringData; }

	bool IsGatheringDataEnabled() const { return bEnableGatheringData; }

	/** Find or create the global find results widget */
	TSharedPtr<SFindInBlueprints> GetGlobalFindResults();

	/** Enable or disable the global find results tab feature */
	void EnableGlobalFindResults(bool bEnable);

	/** Close any orphaned global find results tabs for a particular tab manager */
	void CloseOrphanedGlobalFindResultsTabs(TSharedPtr<class FTabManager> TabManager);

	void GlobalFindResultsClosed(const TSharedRef<SFindInBlueprints>& FindResults);

private:
	/** Initializes the FiB manager */
	void Initialize();

	/** Callback hook during pre-garbage collection, pauses all processing searches so that the GC can do it's job */ 
	void PauseFindInBlueprintSearch();

	/** Callback hook during post-garbage collection, saves the cache to disk and unpauses all processing searches */
	void UnpauseFindInBlueprintSearch();

	/** Callback hook from the Asset Registry when an asset is added */
	void OnAssetAdded(const struct FAssetData& InAssetData);

	/** Callback hook from the Asset Registry, marks the asset for deletion from the cache */
	void OnAssetRemoved(const struct FAssetData& InAssetData);

	/** Callback hook from the Asset Registry, marks the asset for deletion from the cache */
	void OnAssetRenamed(const struct FAssetData& InAssetData, const FString& InOldName);

	/** Callback hook from the Asset Registry when an asset is loaded */
	void OnAssetLoaded(class UObject* InAsset);

	/** Callback from Kismet when a Blueprint is unloaded */
	void OnBlueprintUnloaded(class UBlueprint* InBlueprint);

	/** Callback hook from the Hot Reload manager that indicates that a module has been hot-reloaded */
	void OnHotReload(bool bWasTriggeredAutomatically);

	/** Helper to gathers the Blueprint's search metadata */
	FString GatherBlueprintSearchMetadata(const UBlueprint* Blueprint);

	/** Cleans the cache of any excess data from Blueprints that have been moved, renamed, or deleted. Occurs during post-garbage collection */
	void CleanCache();

	/** Builds the cache from all available Blueprint assets that the asset registry has discovered at the time of this function. Occurs on startup */
	void BuildCache();

	/**
	 * Helper to properly add a Blueprint's SearchData to the database
	 *
	 * @param InSearchData		Data to add to the database
	 * @return					Index into the SearchArray for looking up the added item
	 */
	int32 AddSearchDataToDatabase(FSearchData InSearchData);

	/** Removes a Blueprint from being managed by the FiB system by passing in the UBlueprint's path */
	void RemoveBlueprintByPath(FName InPath);

	/** Begins the process of extracting unloaded FiB data */
	void ExtractUnloadedFiBData(const FAssetData& InAssetData, const FString& InFiBData, bool bIsVersioned);

	/** Determines the global find results tab label */
	FText GetGlobalFindResultsTabLabel(int32 TabIdx);

	/** Handler for a request to spawn a new global find results tab */
	TSharedRef<SDockTab> SpawnGlobalFindResultsTab(const FSpawnTabArgs& SpawnTabArgs, int32 TabIdx);

	/** Creates and opens a new global find results tab */
	TSharedPtr<SFindInBlueprints> OpenGlobalFindResultsTab();

protected:
	/** Tells if gathering data is currently allowed */
	bool bEnableGatheringData;

	/** Maps the Blueprint paths to their index in the SearchArray */
	TMap<FName, int32> SearchMap;

	/** Stores the Blueprint search data and is used to iterate over in small chunks */
	TArray<FSearchData> SearchArray;

	/** Counter of active searches */
	FThreadSafeCounter ActiveSearchCounter;

	/** A mapping of active search queries and where they are currently at in the search data */
	TMap< const class FStreamSearch*, int32 > ActiveSearchQueries;

	/** Critical section to safely add, remove, and find data in ActiveSearchQueries */
	mutable FCriticalSection SafeQueryModifyCriticalSection;

	/** Critical section to lock threads during the pausing procedure */
	FCriticalSection PauseThreadsCriticalSection;

	/** Critical section to safely modify cached data */
	FCriticalSection SafeModifyCacheCriticalSection;

	/** TRUE when the the FiB manager wants to pause all searches, helps manage the pausing procedure */
	volatile bool bIsPausing;

	/** Because we are unable to query for the module on another thread, cache it for use later */
	class FAssetRegistryModule* AssetRegistryModule;

	/** FindInBlueprints widget that started the cache process */
	TWeakPtr<SFindInBlueprints> SourceCachingWidget;

	/** Blueprint paths that have not been cached for searching due to lack of data, this means that they are either older Blueprints, or the DDC cannot find the data */
	TSet<FName> UncachedBlueprints;

	/** List of paths for Blueprints that failed to cache */
	TSet<FName> FailedToCachePaths;

	/** Tickable object that does the caching of uncached Blueprints at a rate of once per tick */
	class FCacheAllBlueprintsTickableObject* CachingObject;

	/** Mapping between a class name and its UClass instance - used for faster look up in FFindInBlueprintSearchManager::OnAssetAdded */
	TMap<FName, const UClass*> CachedAssetClasses;

	/** The tab identifier/instance name for global find results */
	FName GlobalFindResultsTabIDs[MAX_GLOBAL_FIND_RESULTS];

	/** Array of open global find results widgets */
	TArray<TWeakPtr<SFindInBlueprints>> GlobalFindResults;

	/** Global Find Results workspace menu item */
	TSharedPtr<class FWorkspaceItem> GlobalFindResultsMenuItem;
};

struct KISMET_API FDisableGatheringDataOnScope
{
	bool bOriginallyEnabled;
	FDisableGatheringDataOnScope() : bOriginallyEnabled(FFindInBlueprintSearchManager::Get().IsGatheringDataEnabled())
	{
		FFindInBlueprintSearchManager::Get().EnableGatheringData(false);
	}
	~FDisableGatheringDataOnScope()
	{
		FFindInBlueprintSearchManager::Get().EnableGatheringData(bOriginallyEnabled);
	}
};
