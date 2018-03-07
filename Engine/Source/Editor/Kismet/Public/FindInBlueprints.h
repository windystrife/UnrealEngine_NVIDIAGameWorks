// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Guid.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "EdGraph/EdGraphSchema.h"
#include "FindInBlueprintManager.h"

class FBlueprintEditor;
class FImaginaryFiBData;
class FUICommandList;

typedef STreeView<FSearchResult>  STreeViewType;

DECLARE_DELEGATE_OneParam(FOnSearchComplete, TArray<TSharedPtr<class FImaginaryFiBData>>&);

/** Some utility functions to help with Find-in-Blueprint functionality */
namespace FindInBlueprintsHelpers
{
	// Stores an FText as if it were an FString, does zero advanced comparisons needed for true FText comparisons
	struct FSimpleFTextKeyStorage
	{
		FText Text;

		FSimpleFTextKeyStorage(FText InText)
			: Text(InText)
		{

		}

		bool operator==(const FSimpleFTextKeyStorage& InObject) const
		{
			return Text.ToString() == InObject.Text.ToString() || Text.BuildSourceString() == InObject.Text.BuildSourceString();
		}
	};

	static uint32 GetTypeHash(const FindInBlueprintsHelpers::FSimpleFTextKeyStorage& InObject)
	{
		return GetTypeHash(InObject.Text.BuildSourceString());
	}

	/** Looks up a JsonValue's FText from the passed lookup table */
	FText AsFText(TSharedPtr< FJsonValue > InJsonValue, const TMap<int32, FText>& InLookupTable);

	/** Looks up a JsonValue's FText from the passed lookup table */
	FText AsFText(int32 InValue, const TMap<int32, FText>& InLookupTable);

	bool IsTextEqualToString(const FText& InText, const FString& InString);

	/**
	 * Retrieves the pin type as a string value
	 *
	 * @param InPinType		The pin type to look at
	 *
	 * @return				The pin type as a string in format [category]'[sub-category object]'
	 */
	FString GetPinTypeAsString(const FEdGraphPinType& InPinType);

	/**
	 * Parses a pin type from passed in key names and values
	 *
	 * @param InKey					The key name for what the data should be translated as
	 * @param InValue				Value to be be translated
	 * @param InOutPinType			Modifies the PinType based on the passed parameters, building it up over multiple calls
	 * @return						TRUE when the parsing is successful
	 */
	bool ParsePinType(FText InKey, FText InValue, FEdGraphPinType& InOutPinType);

	/**
	* Iterates through all the given tree node's children and tells the tree view to expand them
	*/
	void ExpandAllChildren(FSearchResult InTreeNode, TSharedPtr<STreeView<TSharedPtr<FFindInBlueprintsResult>>> InTreeView);
}

/** Graph nodes use this class to store their data */
class FFindInBlueprintsGraphNode : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsGraphNode(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent);
	virtual ~FFindInBlueprintsGraphNode() {}

	/** FFindInBlueprintsResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	virtual UObject* GetObject(UBlueprint* InBlueprint) const override;
	/** End FFindInBlueprintsResult Interface */

private:
	/** The Node Guid to find when jumping to the node */
	FGuid NodeGuid;

	/** The glyph brush for this node */
	FSlateIcon Glyph;

	/** The glyph color for this node */
	FLinearColor GlyphColor;

	/*The class this item refers to */
	UClass* Class;

	/*The class name this item refers to */
	FString ClassName;
};

/** Pins use this class to store their data */
class FFindInBlueprintsPin : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsPin(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, FString InSchemaName);
	virtual ~FFindInBlueprintsPin() {}

	/** FFindInBlueprintsResult Interface */
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FFindInBlueprintsResult Interface */
protected:
	/** The name of the schema this pin exists under */
	FString SchemaName;

	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** Pin's icon color */
	FSlateColor IconColor;
};

/** Property data is stored here */
class FFindInBlueprintsProperty : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsProperty(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent);
	virtual ~FFindInBlueprintsProperty() {}

	/** FFindInBlueprintsResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FFindInBlueprintsResult Interface */
protected:
	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** The default value of a property as a string */
	FString DefaultValue;

	/** TRUE if the property is an SCS_Component */
	bool bIsSCSComponent;
};

/** Graphs, such as functions and macros, are stored here */
class FFindInBlueprintsGraph : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsGraph(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, EGraphType InGraphType);
	virtual ~FFindInBlueprintsGraph() {}

	/** FFindInBlueprintsResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	/** End FFindInBlueprintsResult Interface */
protected:

	/** The type of graph this represents */
	EGraphType GraphType;
};

/*Widget for searching for (functions/events) across all blueprints or just a single blueprint */
class SFindInBlueprints: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SFindInBlueprints )
		: _bIsSearchWindow(true)
		, _bHideSearchBar(false)
		, _ContainingTab()
	{}
		SLATE_ARGUMENT(bool, bIsSearchWindow)
		SLATE_ARGUMENT(bool, bHideSearchBar)
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, ContainingTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FBlueprintEditor> InBlueprintEditor = nullptr);
	~SFindInBlueprints();

	/** Focuses this widget's search box, and changes the mode as well, and optionally the search terms */
	void FocusForUse(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false);

	/**
	 * Submits a search query
	 *
	 * @param InSearchString						String to search using
	 * @param bInIsFindWithinBlueprint				TRUE if searching within the current Blueprint only
	 * @param InSearchFilterForImaginaryDataReturn	If requesting a callback on search complete for the filtered imaginary data, this is the filter that the raw data will be passed through. By default nothing is collected
	 * @param InMinimiumVersionRequirement			The minimum search requirement Blueprints must be to be searched or they will be reported as out-of-date.
	 * @param InOnSearchComplete					Callback when the search is complete, passing the filtered imaginary data (if any).
	 */
	void MakeSearchQuery(FString InSearchString, bool bInIsFindWithinBlueprint, enum ESearchQueryFilter InSearchFilterForImaginaryDataReturn = ESearchQueryFilter::AllFilter, EFiBVersion InMinimiumVersionRequirement = EFiBVersion::FIB_VER_LATEST, FOnSearchComplete InOnSearchComplete = FOnSearchComplete());

	/** Called when caching Blueprints is complete, if this widget initiated the indexing */
	void OnCacheComplete();

	/**
	 * Asynchronously caches all Blueprints below a specified version.
	 *
	 * @param InOnFinished						Callback when the cache process is complete.
	 * @param InMinimiumVersionRequirement		Version that Blueprints must be below to be loaded and indexed, by default all out-of-date Blueprints
	 */
	void CacheAllBlueprints(FSimpleDelegate InOnFinished = FSimpleDelegate(), EFiBVersion InMinimiumVersionRequirement = EFiBVersion::FIB_VER_LATEST);

	/** If this is a global find results widget, returns the host tab's unique ID. Otherwise, returns NAME_None. */
	FName GetHostTabId() const;

	/** If this is a global find results widget, ask the host tab to close */
	void CloseHostTab();

	/** Determines if this context does not accept syncing from an external source */
	bool IsLocked() const
	{
		return bIsLocked;
	}

private:
	/** Processes results of the ongoing async stream search */
	EActiveTimerReturnType UpdateSearchResults( double InCurrentTime, float InDeltaTime );

	/** Register any Find-in-Blueprint commands */
	void RegisterCommands();

	/*Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& Text);

	/*Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Called when the find mode checkbox is hit */
	void OnFindModeChanged(ECheckBoxState CheckState);

	/** Called to check what the find mode is for the checkbox */
	ECheckBoxState OnGetFindModeChecked() const;

	/* Get the children of a row */
	void OnGetChildren( FSearchResult InItem, TArray< FSearchResult >& OutChildren );

	/* Called when user double clicks on a new result */
	void OnTreeSelectionDoubleClicked( FSearchResult Item );

	/* Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Launches a thread for streaming more content into the results widget */
	void LaunchStreamThread(const FString& InSearchValue);
	void LaunchStreamThread(const FString& InSearchValue, enum ESearchQueryFilter InSearchFilterForRawDataReturn, EFiBVersion InMinimiumVersionRequirement, FOnSearchComplete InOnSearchComplete);

	/** Returns the percent complete on the search for the progress bar */
	TOptional<float> GetPercentCompleteSearch() const;

	/** Returns the progress bar visiblity */
	EVisibility GetSearchbarVisiblity() const;

	/** Adds the "cache" bar at the bottom of the Find-in-Blueprints widget, to notify the user that the search is incomplete */
	void ConditionallyAddCacheBar();

	/** Callback to remove the "cache" bar when a button is pressed */
	FReply OnRemoveCacheBar();

	/** Callback to return the cache bar's display text, informing the user of the situation */
	FText GetUncachedBlueprintWarningText() const;

	/** Callback to return the cache bar's current indexing Blueprint name */
	FText GetCurrentCacheBlueprintName() const;

	/** Callback to cache all uncached Blueprints */
	FReply OnCacheAllBlueprints();
	FReply OnCacheAllBlueprints(FSimpleDelegate InOnFinished, EFiBVersion InMinimiumVersionRequirement = EFiBVersion::FIB_VER_LATEST);

	/** Callback to cancel the caching process */
	FReply OnCancelCacheAll();

	/** Retrieves the current index of the Blueprint caching process */
	int32 GetCurrentCacheIndex() const;

	/** Gets the percent complete of the caching process */
	TOptional<float> GetPercentCompleteCache() const;

	/** Returns the visibility of the caching progress bar, visible when in progress, hidden when not */
	EVisibility GetCachingProgressBarVisiblity() const;

	/** Returns the visibility of the "Cache All" button, visible when not caching, collapsed when caching is in progress */
	EVisibility GetCacheAllButtonVisibility() const;

	/** Returns the caching bar's visibility, it goes invisible when there is nothing to be cached. The next search will remove this bar or make it visible again */
	EVisibility GetCachingBarVisibility() const;

	/** Returns the visibility of the caching Blueprint name, visible when in progress, collapsed when not */
	EVisibility GetCachingBlueprintNameVisiblity() const;

	/** Returns the visibility of the popup button that displays the list of Blueprints that failed to cache */
	EVisibility GetFailedToCacheListVisibility() const;

	/** Returns TRUE if Blueprint caching is in progress */
	bool IsCacheInProgress() const;

	/** Returns the color of the caching bar */
	FSlateColor GetCachingBarColor() const;

	/** Callback to build the context menu when right clicking in the tree */
	TSharedPtr<SWidget> OnContextMenuOpening();

	/** Helper function to select all items */
	void SelectAllItemsHelper(FSearchResult InItemToSelect);

	/** Callback when user attempts to select all items in the search results */
	void OnSelectAllAction();

	/** Callback when user attempts to copy their selection in the Find-in-Blueprints */
	void OnCopyAction();

	/** Called when the user clicks the global find results button */
	FReply OnOpenGlobalFindResults();

	/** Called when the host tab is closed (if valid) */
	void OnHostTabClosed(TSharedRef<SDockTab> DockTab);

	/** Called when the lock button is clicked in a global find results tab */
	FReply OnLockButtonClicked();

	/** Returns the image used for the lock button in a global find results tab */
	const FSlateBrush* OnGetLockButtonImage() const;

private:
	/** Pointer back to the blueprint editor that owns us */
	TWeakPtr<class FBlueprintEditor> BlueprintEditorPtr;
	
	/* The tree view displays the results */
	TSharedPtr<STreeViewType> TreeView;

	/** The search text box */
	TSharedPtr<class SSearchBox> SearchTextField;
	
	/* This buffer stores the currently displayed results */
	TArray<FSearchResult> ItemsFound;

	/** In Find Within Blueprint mode, we need to keep a handle on the root result, because it won't show up in the tree */
	FSearchResult RootSearchResult;

	/* The string to highlight in the results */
	FText HighlightText;

	/* The string to search for */
	FString	SearchValue;

	/** Should we search within the current blueprint only (rather than all blueprints) */
	bool bIsInFindWithinBlueprintMode;

	/** Thread object that searches through Blueprint data on a separate thread */
	TSharedPtr< class FStreamSearch> StreamSearch;

	/** Vertical box, used to add and remove widgets dynamically */
	TWeakPtr< SVerticalBox > MainVerticalBox;

	/** Weak pointer to the cache bar slot, so it can be removed */
	TWeakPtr< SWidget > CacheBarSlot;

	/** Callback when search is complete */
	FOnSearchComplete OnSearchComplete;

	/** Cached count of out of date Blueprints from last search. */
	int32 OutOfDateWithLastSearchBPCount;

	/** Cached version that was last searched */
	EFiBVersion LastSearchedFiBVersion;

	/** Commands handled by this widget */
	TSharedPtr< FUICommandList > CommandList;

	/** Tab hosting this widget. May be invalid. */
	TWeakPtr<SDockTab> HostTab;

	/** True if current search should not be changed by an external source */
	bool bIsLocked;

	/** True if the most recent search was a global search */
	bool bHasGlobalSearchResults;
};
