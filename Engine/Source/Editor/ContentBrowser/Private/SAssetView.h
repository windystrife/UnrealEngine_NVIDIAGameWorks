// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "AssetThumbnail.h"
#include "IContentBrowserSingleton.h"
#include "SourcesData.h"
#include "Animation/CurveSequence.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Editor/ContentBrowser/Private/AssetViewSortManager.h"
#include "AssetViewTypes.h"
#include "HistoryManager.h"

class FMenuBuilder;
class FWeakWidgetPath;
class FWidgetPath;
class SAssetColumnView;
class SAssetListView;
class SAssetTileView;
class SComboButton;
class UFactory;
struct FPropertyChangedEvent;

/**
 * A widget to display a list of filtered assets
 */
class SAssetView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAssetView )
		: _AreRealTimeThumbnailsAllowed(true)
		, _ThumbnailLabel( EThumbnailLabel::ClassName )
		, _AllowThumbnailHintLabel(true)
		, _InitialViewType(EAssetViewType::Tile)
		, _ThumbnailScale(0.1f)
		, _ShowBottomToolbar(true)
		, _AllowThumbnailEditMode(false)
		, _CanShowClasses(true)
		, _CanShowFolders(false)
		, _FilterRecursivelyWithBackendFilter(true)
		, _CanShowRealTimeThumbnails(false)
		, _CanShowDevelopersFolder(false)
		, _CanShowCollections(false)
		, _PreloadAssetsForContextMenu(true)
		, _SelectionMode( ESelectionMode::Multi )
		, _AllowDragging(true)
		, _AllowFocusOnSync(true)
		, _FillEmptySpaceInTileView(true)
		, _ShowPathInColumnView(false)
		, _ShowTypeInColumnView(true)
		, _SortByPathInColumnView(false)
		{}

		/** Called to check if an asset should be filtered out by external code */
		SLATE_EVENT( FOnShouldFilterAsset, OnShouldFilterAsset )

		/** Called to when an asset is selected */
		SLATE_EVENT( FOnAssetSelected, OnAssetSelected )

		/** Called when the user double clicks, presses enter, or presses space on an asset */
		SLATE_EVENT( FOnAssetsActivated, OnAssetsActivated )

		/** Called when an asset is right clicked */
		SLATE_EVENT( FOnGetAssetContextMenu, OnGetAssetContextMenu )

		/** Delegate to invoke when a context menu for a folder is opening. */
		SLATE_EVENT( FOnGetFolderContextMenu, OnGetFolderContextMenu )

		/** The delegate that fires when a path is right clicked and a context menu is requested */
		SLATE_EVENT( FContentBrowserMenuExtender_SelectedPaths, OnGetPathContextMenuExtender )

		/** Invoked when a "Find in Asset Tree" is requested */
		SLATE_EVENT( FOnFindInAssetTreeRequested, OnFindInAssetTreeRequested )

		/** Called when the user has committed a rename of one or more assets */
		SLATE_EVENT( FOnAssetRenameCommitted, OnAssetRenameCommitted )

		/** Called to get a custom asset item tool tip (if necessary) */
		SLATE_EVENT( FOnGetCustomAssetToolTip, OnGetCustomAssetToolTip )

		/** Called when an asset item is about to show a tooltip */
		SLATE_EVENT( FOnVisualizeAssetToolTip, OnVisualizeAssetToolTip )

		/** Called when an asset item's tooltip is closing */
		SLATE_EVENT(FOnAssetToolTipClosing, OnAssetToolTipClosing)

		/** The warning text to display when there are no assets to show */
		SLATE_ATTRIBUTE( FText, AssetShowWarningText )

		/** Attribute to determine if real-time thumbnails should be used */
		SLATE_ATTRIBUTE( bool, AreRealTimeThumbnailsAllowed )

		/** Attribute to determine what text should be highlighted */
		SLATE_ATTRIBUTE( FText, HighlightedText )

		/** What the label on the asset thumbnails should be */
		SLATE_ARGUMENT( EThumbnailLabel::Type, ThumbnailLabel )

		/** Whether to ever show the hint label on thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailHintLabel )

		/** The filter collection used to further filter down assets returned from the backend */
		SLATE_ARGUMENT( TSharedPtr<FAssetFilterCollectionType>, FrontendFilters )

		/** The initial base sources filter */
		SLATE_ARGUMENT( FSourcesData, InitialSourcesData )

		/** The initial backend filter */
		SLATE_ARGUMENT( FARFilter, InitialBackendFilter )

		/** The asset that should be initially selected */
		SLATE_ARGUMENT( FAssetData, InitialAssetSelection )

		/** The initial view type */
		SLATE_ARGUMENT( EAssetViewType::Type, InitialViewType )

		/** The thumbnail scale. [0-1] where 0.5 is no scale */
		SLATE_ATTRIBUTE( float, ThumbnailScale )

		/** Should the toolbar indicating number of selected assets, mode switch buttons, etc... be shown? */
		SLATE_ARGUMENT( bool, ShowBottomToolbar )

		/** True if the asset view may edit thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailEditMode )

		/** Indicates if this view is allowed to show classes */
		SLATE_ARGUMENT( bool, CanShowClasses )

		/** Indicates if the 'Show Folders' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowFolders )

		/** If true, recursive filtering will be caused by applying a backend filter */
		SLATE_ARGUMENT( bool, FilterRecursivelyWithBackendFilter )

		/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowRealTimeThumbnails )

		/** Indicates if the 'Show Developers' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowDevelopersFolder )

		/** Indicates if the 'Show Collections' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowCollections )

		/** Indicates if the context menu is going to load the assets, and if so to preload before the context menu is shown, and warn about the pending load. */
		SLATE_ARGUMENT( bool, PreloadAssetsForContextMenu )

		/** The selection mode the asset view should use */
		SLATE_ARGUMENT( ESelectionMode::Type, SelectionMode )

		/** Whether to allow dragging of items */
		SLATE_ARGUMENT( bool, AllowDragging )

		/** Whether this asset view should allow focus on sync or not */
		SLATE_ARGUMENT( bool, AllowFocusOnSync )

		/** Whether this asset view should allow the thumbnails to consume empty space after the user scale is applied */
		SLATE_ARGUMENT( bool, FillEmptySpaceInTileView )

		/** Should show Path in column view if true */
		SLATE_ARGUMENT(bool, ShowPathInColumnView)

		/** Should show Type in column view if true */
		SLATE_ARGUMENT(bool, ShowTypeInColumnView)

		/** Sort by path in the column view. Only works if the initial view type is Column */
		SLATE_ARGUMENT(bool, SortByPathInColumnView)

		/** Called to check if an asset tag should be display in details view. */
		SLATE_EVENT( FOnShouldDisplayAssetTag, OnAssetTagWantsToBeDisplayed )

		/** Called when a folder is entered */
		SLATE_EVENT( FOnPathSelected, OnPathSelected )

		/** Columns to hide by default */
		SLATE_ARGUMENT( TArray<FString>, HiddenColumnNames )

		/** Custom columns that can be use specific */
		SLATE_ARGUMENT(TArray<FAssetViewCustomColumn>, CustomColumns)

	SLATE_END_ARGS()

	~SAssetView();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Changes the base sources for this view */
	void SetSourcesData(const FSourcesData& InSourcesData);

	/** Returns the sources filter applied to this asset view */
	const FSourcesData& GetSourcesData() const;

	/** Returns true if a real asset path is selected (i.e \Engine\* or \Game\*) */
	bool IsAssetPathSelected() const;

	/** Notifies the asset view that the filter-list filter has changed */
	void SetBackendFilter(const FARFilter& InBackendFilter);

	/** Creates a new asset item designed to allocate a new object once it is named. Uses the supplied factory to create the asset */
	void CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory);

	/** Creates a new asset item designed to duplicate an object once it is named */
	void DuplicateAsset(const FString& PackagePath, const TWeakObjectPtr<UObject>& OriginalObject);

	/** Sets up an inline rename for the specified asset */
	void RenameAsset(const FAssetData& ItemToRename);

	/** Sets up an inline rename for the specified folder */
	void RenameFolder(const FString& FolderToRename);

	/** Selects the paths containing the specified assets. */
	void SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bFocusOnSync = true );

	/** Selects the specified paths. */
	void SyncToFolders( const TArray<FString>& FolderList, const bool bFocusOnSync = true );

	/** Selects the paths containing the specified items. */
	void SyncTo( const FContentBrowserSelection& ItemSelection, const bool bFocusOnSync = true );

	/** Sets the state of the asset view to the one described by the history data */
	void ApplyHistoryData( const FHistoryData& History );

	/** Returns all the items currently selected in the view */
	TArray<TSharedPtr<FAssetViewItem>> GetSelectedItems() const;

	/** Returns all the asset data objects in items currently selected in the view */
	TArray<FAssetData> GetSelectedAssets() const;

	/** Returns all the folders currently selected in the view */
	TArray<FString> GetSelectedFolders() const;

	/** Requests that the asset view refreshes all it's source items. This is slow and should only be used if the source items change. */
	void RequestSlowFullListRefresh();

	/** Requests that the asset view refreshes only items that are filtered through frontend sources. This should be used when possible. */
	void RequestQuickFrontendListRefresh();

	/** Requests that the asset view adds any recently added items in the next update to the filtered asset items */
	void RequestAddNewAssetsNextFrame();

	/** Saves any settings to config that should be persistent between editor sessions */
	void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const;

	/** Loads any settings to config that should be persistent between editor sessions */
	void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString);

	/** Adjusts the selected asset by the selection delta, which should be +1 or -1) */
	void AdjustActiveSelection(int32 SelectionDelta);

	/** Processes assets that were loaded or changed since the last frame */
	void ProcessRecentlyLoadedOrChangedAssets();

	/** Returns true if an asset is currently in the process of being renamed */
	bool IsRenamingAsset() const;

	// SWidget inherited
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent ) override;

	/** Opens the selected assets or folders, depending on the selection */
	void OnOpenAssetsOrFolders();

	/** Loads the selected assets and previews them if possible */
	void OnPreviewAssets();

	/** Clears the selection of all the lists in the view */
	void ClearSelection(bool bForceSilent = false);

	/** Returns true if the asset view is in thumbnail editing mode */
	bool IsThumbnailEditMode() const;

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** Set whether the user is currently searching or not */
	void SetUserSearching(bool bInSearching);

	/** Create a new folder item. The item will create a new folder once it is named */
	void OnCreateNewFolder(const FString& DefaultFolderName, const FString& FolderPath);

	/** Called when a folder is added to the asset registry */
	void OnAssetRegistryPathAdded(const FString& Path);

	/** Called when a folder is removed from the asset registry */
	void OnAssetRegistryPathRemoved(const FString& Path);

	/** Handles updating the content browser when a path is populated with an asset for the first time */
	void OnFolderPopulated(const FString& Path);

	/**
	 * Forces the plugin content folder to be shown.
	 *
	 * @param bEnginePlugin		If true, also forces the engine folder to be shown.
	 */
	void ForceShowPluginFolder( bool bEnginePlugin );

private:

	/** Sets the pending selection to the current selection (used when changing views or refreshing the view). */
	void SyncToSelection( const bool bFocusOnSync = true );

	/** @return the thumbnail scale setting path to use when looking up the setting in an ini. */
	FString GetThumbnailScaleSettingPath(const FString& SettingsString) const;

	/** @return the view type setting path to use when looking up the setting in an ini. */
	FString GetCurrentViewTypeSettingPath(const FString& SettingsString) const;

	/** Calculates a new filler scale used to adjust the thumbnails to fill empty space. */
	void CalculateFillScale( const FGeometry& AllottedGeometry );

	/** Calculates the latest color and opacity for the hint on thumbnails */
	void CalculateThumbnailHintColorAndOpacity();

	/** Handles amortizing the backend filters */
	void ProcessQueriedItems( const double TickStartTime );

	/** Creates a new tile view */
	TSharedRef<class SAssetTileView> CreateTileView();

	/** Creates a new list view */
	TSharedRef<class SAssetListView> CreateListView();

	/** Creates a new column view */
	TSharedRef<class SAssetColumnView> CreateColumnView();

	/** Returns true if the specified search token is allowed */
	bool IsValidSearchToken(const FString& Token) const;

	/** Regenerates the AssetItems list from the AssetRegistry */
	void RefreshSourceItems();

	/** Regenerates the FilteredAssetItems list from the AssetItems list */
	void RefreshFilteredItems();

	/** Regenerates folders if we are displaying them */
	void RefreshFolders();

	/** Sets the asset type that represents the majority of the assets in view */
	void SetMajorityAssetType(FName NewMajorityAssetType);

	/** Handler for when an asset is added to a collection */
	void OnAssetsAddedToCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths );

	/** Handler for when an asset was created or added to the asset registry */
	void OnAssetAdded(const FAssetData& AssetData);

	/** Process assets that we were recently informed of & buffered in RecentlyAddedAssets */
	void ProcessRecentlyAddedAssets();

	/** Handler for when an asset is removed from a collection */
	void OnAssetsRemovedFromCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths );

	/** Handler for when an asset was deleted or removed from the asset registry */
	void OnAssetRemoved(const FAssetData& AssetData);

	/** Removes the specified asset from view's caches */
	void RemoveAssetByPath( const FName& ObjectPath );

	/** Handler for when a collection is renamed */
	void OnCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection );

	/** Handler for when a collection is updated */
	void OnCollectionUpdated( const FCollectionNameType& Collection );

	/** Handler for when an asset was renamed in the asset registry */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	/** Handler for when an asset was loaded */
	void OnAssetLoaded(UObject* Asset);

	/** Handler for when an asset's property has changed */
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

	/** Called when the class hierarchy is updated due to the available modules changing */
	void OnClassHierarchyUpdated();

	/** Handler for when any frontend filters have been changed */
	void OnFrontendFiltersChanged();

	/** Returns true if there is any frontend filter active */
	bool IsFrontendFilterActive() const;

	/** Returns true if the specified asset data item passes all applied frontend (non asset registry) filters */
	bool PassesCurrentFrontendFilter(const FAssetData& Item) const;

	/** Returns true if the specified asset data item passes all applied backend (asset registry) filters */
	void RunAssetsThroughBackendFilter(TArray<FAssetData>& InOutAssetDataList) const;

	/** Returns true if the current filters deem that the asset view should be filtered recursively (overriding folder view) */
	bool ShouldFilterRecursively() const;

	/** Sorts the contents of the asset view alphabetically */
	void SortList(bool bSyncToSelection = true);

	/** Returns the thumbnails hint color and opacity */
	FLinearColor GetThumbnailHintColorAndOpacity() const;

	/** Returns the foreground color for the view button */
	FSlateColor GetViewButtonForegroundColor() const;

	/** Handler for when the view combo button is clicked */
	TSharedRef<SWidget> GetViewButtonContent();

	/** Toggle whether folders should be shown or not */
	void ToggleShowFolders();

	/** Whether or not it's possible to show folders */
	bool IsToggleShowFoldersAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingFolders() const;

	/** Toggle whether empty folders should be shown or not */
	void ToggleShowEmptyFolders();

	/** Whether or not it's possible to show empty folders */
	bool IsToggleShowEmptyFoldersAllowed() const;

	/** @return true when we are showing empty folders */
	bool IsShowingEmptyFolders() const;

	/** Toggle whether localized content should be shown or not */
	void ToggleShowLocalizedContent();

	/** Whether or not it's possible to show localized content */
	bool IsToggleShowLocalizedContentAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingLocalizedContent() const;

	/** Toggle whether to show real-time thumbnails */
	void ToggleRealTimeThumbnails();

	/** Whether it is possible to show real-time thumbnails */
	bool CanShowRealTimeThumbnails() const;

	/** @return true if we are showing real-time thumbnails */
	bool IsShowingRealTimeThumbnails() const;

	/** Toggle whether plugin content should be shown or not */
	void ToggleShowPluginContent();

	/** @return true when we are showing plugin content */
	bool IsShowingPluginContent() const;

	/** Toggle whether engine content should be shown or not */
	void ToggleShowEngineContent();

	/** @return true when we are showing engine content */
	bool IsShowingEngineContent() const;

	/** Toggle whether developers content should be shown or not */
	void ToggleShowDevelopersContent();

	/** Whether or not it's possible to toggle developers content */
	bool IsToggleShowDevelopersContentAllowed() const;

	/** @return true when we are showing the developers content */
	bool IsShowingDevelopersContent() const;

	/** Toggle whether collections should be shown or not */
	void ToggleShowCollections();

	/** Whether or not it's possible to toggle collections */
	bool IsToggleShowCollectionsAllowed() const;

	/** @return true when we are showing collections */
	bool IsShowingCollections() const;

	/** Toggle whether C++ content should be shown or not */
	void ToggleShowCppContent();

	/** Whether or not it's possible to show C++ content */
	bool IsToggleShowCppContentAllowed() const;

	/** @return true when we are showing C++ content */
	bool IsShowingCppContent() const;

	/** Sets the view type and updates lists accordingly */
	void SetCurrentViewType(EAssetViewType::Type NewType);

	/** Clears the reference to the current view and creates a new one, based on CurrentViewType */
	void CreateCurrentView();

	/** Gets the current view type (list or tile) */
	EAssetViewType::Type GetCurrentViewType() const;

	TSharedRef<SWidget> CreateShadowOverlay( TSharedRef<STableViewBase> Table );

	/** Returns true if ViewType is the current view type */
	bool IsCurrentViewType(EAssetViewType::Type ViewType) const;

	/** Set the keyboard focus to the correct list view that should be active */
	void FocusList() const;

	/** Refreshes the list view to display any changes made to the non-filtered assets */
	void RefreshList();

	/** Sets the sole selection for all lists in the view */
	void SetSelection(const TSharedPtr<FAssetViewItem>& Item);

	/** Sets selection for an item in all lists in the view */
	void SetItemSelection(const TSharedPtr<FAssetViewItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Scrolls the selected item into view for all lists in the view */
	void RequestScrollIntoView(const TSharedPtr<FAssetViewItem>& Item);

	/** Handler for list view widget creation */
	TSharedRef<ITableRow> MakeListViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for tile view widget creation */
	TSharedRef<ITableRow> MakeTileViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for column view widget creation */
	TSharedRef<ITableRow> MakeColumnViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for when any asset item widget gets destroyed */
	void AssetItemWidgetDestroyed(const TSharedPtr<FAssetViewItem>& Item);
	
	/** Creates new thumbnails that are near the view area and deletes old thumbnails that are no longer relevant. */
	void UpdateThumbnails();

	/**  Helper function for UpdateThumbnails. Adds the specified item to the new thumbnail relevancy map and creates any thumbnails for new items. Returns the thumbnail. */
	TSharedPtr<class FAssetThumbnail> AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FAssetViewAsset>& Item, TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<class FAssetThumbnail> >& NewRelevantThumbnails);

	/** Handler for tree view selection changes */
	void AssetSelectionChanged( TSharedPtr<FAssetViewItem > AssetItem, ESelectInfo::Type SelectInfo );

	/** Handler for when an item has scrolled into view after having been requested to do so */
	void ItemScrolledIntoView(TSharedPtr<FAssetViewItem> AssetItem, const TSharedPtr<ITableRow>& Widget);

	/** Handler for context menus */
	TSharedPtr<SWidget> OnGetContextMenuContent();

	/** Handler called when an asset context menu is about to open */
	bool CanOpenContextMenu() const;

	/** Handler for double clicking an item */
	void OnListMouseButtonDoubleClick(TSharedPtr<FAssetViewItem> AssetItem);

	/** Handle dragging an asset */
	FReply OnDraggingAssetItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Checks if the asset name being committed is valid */
	bool AssetVerifyRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FText& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage);

	/** An asset item has started to be renamed */
	void AssetRenameBegin(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor);

	/** An asset item that was prompting the user for a new name was committed. Return false to indicate that the user should enter a new name */
	void AssetRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor, const ETextCommit::Type CommitType);

	/** Gets the color and opacity for all names of assets in the asset view */
	FLinearColor GetAssetNameColorAndOpacity() const;

	/** Returns true if tooltips should be allowed right now. Tooltips are typically disabled while right click scrolling. */
	bool ShouldAllowToolTips() const;

	/** Returns true if the asset view is currently allowing the user to edit thumbnails */
	bool IsThumbnailEditModeAllowed() const;

	/** The "Done Editing" button was pressed in the thumbnail edit mode strip */
	FReply EndThumbnailEditModeClicked();

	/** Gets the text for the asset count label */
	FText GetAssetCountText() const;

	/** Gets the visibility of the Thumbnail Edit Mode label */
	EVisibility GetEditModeLabelVisibility() const;

	/** Gets the visibility of the list view */
	EVisibility GetListViewVisibility() const;

	/** Gets the visibility of the tile view */
	EVisibility GetTileViewVisibility() const;

	/** Gets the visibility of the column view */
	EVisibility GetColumnViewVisibility() const;

	/** Toggles thumbnail editing mode */
	void ToggleThumbnailEditMode();

	/** Gets the current value for the scale slider (0 to 1) */
	float GetThumbnailScale() const;

	/** Sets the current scale value (0 to 1) */
	void SetThumbnailScale( float NewValue );

	/** Is thumbnail scale slider locked? */
	bool IsThumbnailScalingLocked() const;

	/** Gets the scaled item height for the list view */
	float GetListViewItemHeight() const;
	
	/** Gets the final scaled item height for the tile view */
	float GetTileViewItemHeight() const;

	/** Gets the scaled item height for the tile view before the filler scale is applied */
	float GetTileViewItemBaseHeight() const;

	/** Gets the final scaled item width for the tile view */
	float GetTileViewItemWidth() const;

	/** Gets the scaled item width for the tile view before the filler scale is applied */
	float GetTileViewItemBaseWidth() const;

	/** Gets the sort mode for the supplied ColumnId */
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	/** Gets the sort order for the supplied ColumnId */
	EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;

	/** Handler for when a column header is clicked */
	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);

	/** @return The state of the is working progress bar */
	TOptional< float > GetIsWorkingProgressBarState() const;

	/** Creates an asset from a temporary asset
	 * @param InName			The name of the asset
	 * @param InItem			The asset item with all the information to create the asset
	 * @param OutErrorText		The error text generated
	 * @return					The created UObject for the asset
	 */
	UObject* CreateAssetFromTemporary(FString InName, const TSharedPtr<FAssetViewAsset>& InItem, FText& OutErrorText);

	/** Is the no assets to show warning visible? */
	EVisibility IsAssetShowWarningTextVisible() const;

	/** Gets the text for displaying no assets to show warning */
	FText GetAssetShowWarningText() const;

	/** Whether we have a single source collection selected */
	bool HasSingleCollectionSource() const;

	/** Delegate for when assets or asset paths are dragged onto a folder */
	void OnAssetsOrPathsDragDropped(const TArray<FAssetData>& AssetList, const TArray<FString>& AssetPaths, const FString& DestinationPath);

	/** Delegate for when external assets are dragged onto a folder */
	void OnFilesDragDropped(const TArray<FString>& AssetList, const FString& DestinationPath);

	/** Delegate to respond to drop of assets or asset paths onto a folder */
	void ExecuteDropCopy(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath);

	/** Delegate to respond to drop of assets or asset paths onto a folder */
	void ExecuteDropMove(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath);

	/** Creates a new asset from deferred data */
	void DeferredCreateNewAsset();

	/** Creates a new folder from deferred data */
	void DeferredCreateNewFolder();

	/** @return The current quick-jump term */
	FText GetQuickJumpTerm() const;

	/** @return Whether the quick-jump term is currently visible? */
	EVisibility IsQuickJumpVisible() const;

	/** @return The color that should be used for the quick-jump term */
	FSlateColor GetQuickJumpColor() const;

	/** Reset the quick-jump to its empty state */
	void ResetQuickJump();

	/**
	 * Called from OnKeyChar and OnKeyDown to handle quick-jump key presses
	 * @param InCharacter		The character that was typed
	 * @param bIsControlDown	Was the control key pressed?
	 * @param bIsAltDown		Was the alt key pressed?
	 * @param bTestOnly			True if we only want to test whether the key press would be handled, but not actually update the quick-jump term
	 * @return FReply::Handled or FReply::Unhandled
	 */
	FReply HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly);

	/**
	 * Perform a quick-jump to the next available asset in FilteredAssetItems that matches the current term
	 * @param bWasJumping		True if we were performing an ongoing quick-jump last Tick
	 * @return True if the quick-jump found a valid match, false otherwise
	 */
	bool PerformQuickJump(const bool bWasJumping);
	
	/** Generates the column filtering menu */
	void FillToggleColumnsMenu(FMenuBuilder& MenuBuilder);

	/** Resets the column filtering state to make them all visible */
	void ResetColumns();

	/** Export columns to CSV */
	void ExportColumns();

	/** Toggle the column at ColumnIndex */
	void ToggleColumn(const FString ColumnName);
	/** Sets the column visibility by removing/inserting the column*/
	void SetColumnVisibility(const FString ColumnName, const bool bShow);

	/** Whether or not a column can be toggled, has to be valid column and mandatory minimum number of columns = 1*/
	bool CanToggleColumn(const FString ColumnName) const;

	/** Whether or not a column is visible to show it's state in the filtering menu */
	bool IsColumnVisible(const FString ColumnName) const;

	/** Creates the row header context menu allowing for hiding individually clicked columns*/
	TSharedRef<SWidget> CreateRowHeaderMenuContent(const FString ColumnName);
	/** Will compute the max row size from all its children for the specified column id*/
	FVector2D GetMaxRowSizeForColumn(const FName& ColumnId);

private:

	/** The asset items being displayed in the view and the filtered list */
	TArray<FAssetData> QueriedAssetItems;
	TArray<FAssetData> AssetItems;
	TArray<TSharedPtr<FAssetViewItem>> FilteredAssetItems;

	/** The folder items being displayed in the view */
	TSet<FString> Folders;

	/** A map of object paths to assets that were loaded or changed since the last frame */
	TMap< FName, TWeakObjectPtr<UObject> > RecentlyLoadedOrChangedAssets;

	/** A list of assets that were recently reported as added by the asset registry */
	TArray<FAssetData> RecentlyAddedAssets;
	TArray<FAssetData> FilteredRecentlyAddedAssets;
	double LastProcessAddsTime;

	/** The list view that is displaying the assets */
	EAssetViewType::Type CurrentViewType;
	TSharedPtr<class SAssetListView> ListView;
	TSharedPtr<class SAssetTileView> TileView;
	TSharedPtr<class SAssetColumnView> ColumnView;
	TSharedPtr<SBorder> ViewContainer;

	/** The button that displays view options */
	TSharedPtr<SComboButton> ViewOptionsComboButton;

	/** The current base source filter for the view */
	FSourcesData SourcesData;
	FARFilter BackendFilter;
	TSharedPtr<FAssetFilterCollectionType> FrontendFilters;

	/** If true, the source items will be refreshed next frame. Very slow. */
	bool bSlowFullListRefreshRequested;

	/** If true, the frontend items will be refreshed next frame. Much faster. */
	bool bQuickFrontendListRefreshRequested;

	/** The list of items to sync next frame */
	FSelectionData PendingSyncItems;

	/** Should we take focus when the PendingSyncAssets are processed? */
	bool bPendingFocusOnSync;

	/** Was recursive filtering enabled the last time a full slow refresh performed? */
	bool bWereItemsRecursivelyFiltered;

	/** Called to check if an asset should be filtered out by external code */
	FOnShouldFilterAsset OnShouldFilterAsset;

	/** Called when an asset was selected in the list */
	FOnAssetSelected OnAssetSelected;

	/** Called when the user double clicks, presses enter, or presses space on an asset */
	FOnAssetsActivated OnAssetsActivated;

	/** Called when the user right clicks on an asset in the view */
	FOnGetAssetContextMenu OnGetAssetContextMenu;

	/** Delegate to invoke when generating the context menu for a folder */
	FOnGetFolderContextMenu OnGetFolderContextMenu;

	/** The delegate that fires when a folder is right clicked and a context menu is requested */
	FContentBrowserMenuExtender_SelectedPaths OnGetPathContextMenuExtender;

	/** Called when a "Find in Asset Tree" is requested */
	FOnFindInAssetTreeRequested OnFindInAssetTreeRequested;

	/** Called when the user has committed a rename of one or more assets */
	FOnAssetRenameCommitted OnAssetRenameCommitted;

	/** Called to check if an asset tag should be display in details view. */
	FOnShouldDisplayAssetTag OnAssetTagWantsToBeDisplayed;

	/** Called to get a custom asset item tooltip (If necessary) */
	FOnGetCustomAssetToolTip OnGetCustomAssetToolTip;

	/** Called when a custom asset item is about to show a tooltip */
	FOnVisualizeAssetToolTip OnVisualizeAssetToolTip;

	/** Called when a custom asset item's tooltip is closing */
	FOnAssetToolTipClosing OnAssetToolTipClosing;

	/** When true, filtered list items will be sorted next tick. Provided another sort hasn't happened recently or we are renaming an asset */
	bool bPendingSortFilteredItems;
	double CurrentTime;
	double LastSortTime;
	double SortDelaySeconds;

	/** Weak ptr to the asset that is waiting to be renamed when scrolled into view, and the window is active */
	TWeakPtr<struct FAssetViewItem> AwaitingRename;

	/** Set when the user is in the process of naming an asset */
	TWeakPtr<struct FAssetViewItem> RenamingAsset;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;

	/** A map of FAssetViewAsset to the thumbnail that represents it. Only items that are currently visible or within half of the FilteredAssetItems array index distance described by NumOffscreenThumbnails are in this list */
	TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<class FAssetThumbnail> > RelevantThumbnails;

	/** The set of FAssetItems that currently have widgets displaying them. */
	TArray< TSharedPtr<FAssetViewItem> > VisibleItems;

	/** The number of thumbnails to keep for asset items that are not currently visible. Half of the thumbnails will be before the earliest item and half will be after the latest. */
	int32 NumOffscreenThumbnails;

	/** The current size of relevant thumbnails */
	int32 CurrentThumbnailSize;

	/** Flag to defer thumbnail updates until the next frame */
	bool bPendingUpdateThumbnails;

	/** The size of thumbnails */
	int32 ListViewThumbnailResolution;
	int32 ListViewThumbnailSize;
	int32 ListViewThumbnailPadding;
	int32 TileViewThumbnailResolution;
	int32 TileViewThumbnailSize;
	int32 TileViewThumbnailPadding;
	int32 TileViewNameHeight;

	/** The current value for the thumbnail scale from the thumbnail slider */
	TAttribute< float > ThumbnailScaleSliderValue;

	/** The max and min thumbnail scales as a fraction of the rendered size */
	float MinThumbnailScale;
	float MaxThumbnailScale;

	/** Flag indicating if we will be filling the empty space in the tile view. */
	bool bFillEmptySpaceInTileView;

	/** The amount to scale each thumbnail so that the empty space is filled. */
	float FillScale;

	/** When in columns view, this is the name of the asset type which is most commonly found in the recent results */
	FName MajorityAssetType;

	/** The manager responsible for sorting assets in the view */
	FAssetViewSortManager SortManager;

	/** When true, selection change notifications will not be sent */
	bool bBulkSelecting;

	/** When true, the user may edit thumbnails */
	bool bAllowThumbnailEditMode;

	/** True when the asset view is currently allowing the user to edit thumbnails */
	bool bThumbnailEditMode;

	/** Indicates if this view is allowed to show classes */
	bool bCanShowClasses;

	/** Indicates if the 'Show Folders' option should be enabled or disabled */
	bool bCanShowFolders;

	/** If true, recursive filtering will be caused by applying a backend filter */
	bool bFilterRecursivelyWithBackendFilter;

	/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
	bool bCanShowRealTimeThumbnails;

	/** Indicates if the 'Show Developers' option should be enabled or disabled */
	bool bCanShowDevelopersFolder;

	/** Indicates if the 'Show Collections' option should be enabled or disabled */
	bool bCanShowCollections;

	/** Indicates if the context menu is going to load the assets, and if so to preload before the context menu is shown, and warn about the pending load. */
	bool bPreloadAssetsForContextMenu;

	/** If true, it will show path column in the asset view */
	bool bShowPathInColumnView;

	/** If true, it will show type in the asset view */
	bool bShowTypeInColumnView;

	/** If true, it sorts by path and then name */
	bool bSortByPathInColumnView;

	/** The current selection mode used by the asset view */
	ESelectionMode::Type SelectionMode;

	/** The max number of results to process per tick */
	float MaxSecondsPerFrame;

	/** When delegate amortization began */
	double AmortizeStartTime;

	/** The total time spent amortizing the delegate filter */
	double TotalAmortizeTime;

	/** Whether the asset view is currently working on something and should display a cue to the user */
	bool bIsWorking;

	/** The text to highlight on the assets */
	TAttribute< FText > HighlightedText;

	/** What the label on the thumbnails should be */
	EThumbnailLabel::Type ThumbnailLabel;

	/** Whether to ever show the hint label on thumbnails */
	bool AllowThumbnailHintLabel;

	/** The sequence used to generate the opacity of the thumbnail hint */
	FCurveSequence ThumbnailHintFadeInSequence;

	/** The current thumbnail hint color and opacity*/
	FLinearColor ThumbnailHintColorAndOpacity;

	/** The text to show when there are no assets to show */
	TAttribute< FText > AssetShowWarningText;

	/** Whether to allow dragging of items */
	bool bAllowDragging;

	/** Whether this asset view should allow focus on sync or not */
	bool bAllowFocusOnSync;

	/** Delegate to invoke when folder is entered. */
	FOnPathSelected OnPathSelected;

	/** Flag set if the user is currently searching */
	bool bUserSearching;
	
	/** Whether or not to notify about newly selected items on on the next asset sync */
	bool bShouldNotifyNextAssetSync;

	/** A struct to hold data for the deferred creation of assets */
	struct FCreateDeferredAssetData
	{
		/** The name of the asset */
		FString DefaultAssetName;

		/** The path where the asset will be created */
		FString PackagePath;

		/** The class of the asset to be created */
		UClass* AssetClass;

		/** The factory to use */
		UFactory* Factory;
	};

	/** Asset pending deferred creation */
	TSharedPtr<FCreateDeferredAssetData> DeferredAssetToCreate;

	/** A struct to hold data for the deferred creation of a folder */
	struct FCreateDeferredFolderData
	{
		/** The name of the folder to create */
		FString FolderName;

		/** The path of the folder to create */
		FString FolderPath;
	};

	/** Folder pending deferred creation */
	TSharedPtr<FCreateDeferredFolderData> DeferredFolderToCreate;

	/** Struct holding the data for the asset quick-jump */
	struct FQuickJumpData
	{
		FQuickJumpData()
			: bIsJumping(false)
			, bHasChangedSinceLastTick(false)
			, bHasValidMatch(false)
			, LastJumpTime(0)
		{
		}

		/** True if we're currently performing an ongoing quick-jump */
		bool bIsJumping;

		/** True if the jump data has changed since the last Tick */
		bool bHasChangedSinceLastTick;

		/** True if the jump term found a valid match */
		bool bHasValidMatch;

		/** Time (taken from Tick) that we last performed a quick-jump */
		double LastJumpTime;

		/** The string we should be be looking for */
		FString JumpTerm;
	};

	/** Data for the asset quick-jump */
	FQuickJumpData QuickJumpData;
	
	/** Column filtering state */
	TArray<FString> DefaultHiddenColumnNames;
	TArray<FString> HiddenColumnNames;
	int32 NumVisibleColumns;

	TArray<FAssetViewCustomColumn> CustomColumns;
public:
	bool ShouldColumnGenerateWidget(const FString ColumnName) const;
};
