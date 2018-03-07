// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Misc/TextFilter.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserDelegates.h"

struct FHistoryData;
struct FTreeItem;

typedef TTextFilter< const FString& > FolderTextFilter;

/**
 * The tree view of folders which contain content.
 */
class SPathView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPathView )
		: _FocusSearchBoxWhenOpened(true)
		, _ShowTreeTitle(false)
		, _ShowSearchBar(true)
		, _ShowSeparator(true)
		, _AllowContextMenu(true)
		, _AllowClassesFolder(false)
		, _SelectionMode( ESelectionMode::Multi )
		{}

		/** Content displayed to the left of the search bar */
		SLATE_NAMED_SLOT( FArguments, SearchContent )

		/** Called when a tree paths was selected */
		SLATE_EVENT( FOnPathSelected, OnPathSelected )

		/** Called when a context menu is opening on a folder */
		SLATE_EVENT( FOnGetFolderContextMenu, OnGetFolderContextMenu )

		/** Called a context menu is opening on a folder */
		SLATE_EVENT( FContentBrowserMenuExtender_SelectedPaths, OnGetPathContextMenuExtender )

		/** If true, the search box will be focus the frame after construction */
		SLATE_ARGUMENT( bool, FocusSearchBoxWhenOpened )

		/** If true, The tree title will be displayed */
		SLATE_ARGUMENT( bool, ShowTreeTitle )

		/** If true, The tree search bar will be displayed */
		SLATE_ARGUMENT( bool, ShowSearchBar )

		/** If true, The tree search bar separator be displayed */
		SLATE_ARGUMENT( bool, ShowSeparator )

		/** If false, the context menu will be suppressed */
		SLATE_ARGUMENT( bool, AllowContextMenu )

		/** If false, the classes folder will be suppressed */
		SLATE_ARGUMENT( bool, AllowClassesFolder )

		/** The selection mode for the tree view */
		SLATE_ARGUMENT( ESelectionMode::Type, SelectionMode )

	SLATE_END_ARGS()

	/** Destructor */
	~SPathView();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Sets focus to the search box */
	void FocusSearchBox();

	/** Selects the closest matches to the supplied paths in the tree. "/" delimited */
	void SetSelectedPaths(const TArray<FString>& Paths);

	/** Clears selection of all paths */
	void ClearSelection();

	/** Returns the first selected path in the tree view */
	FString GetSelectedPath() const;

	/** Returns all selected paths in the tree view */
	TArray<FString> GetSelectedPaths() const;

	/** Adds nodes to the tree in order to construct the specified path. If bUserNamed is true, the user will name the folder and Path includes the default name. */
	TSharedPtr<FTreeItem> AddPath(const FString& Path, bool bUserNamed = false);

	/** Attempts to removed the folder at the end of the specified path from the tree. Returns true when successful. */
	bool RemovePath(const FString& Path);

	/** Sets up an inline rename for the specified folder */
	void RenameFolder(const FString& FolderToRename);

	/**
	 * Selects the paths containing the specified assets.
	 *
	 *	@param AssetDataList		- A list of assets to sync the view to
	 * 
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset 
	 */
	void SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bAllowImplicitSync = false );

	/**
	 * Selects the given paths.
	 *
	 *	@param FolderList			- A list of folders to sync the view to
	 * 
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset 
	 */
	void SyncToFolders( const TArray<FString>& FolderList, const bool bAllowImplicitSync = false );

	/**
	 * Selects the given items.
	 *
	 *	@param ItemSelection		- A list of assets and folders to sync the view to
	 * 
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset 
	 */
	void SyncTo( const FContentBrowserSelection& ItemSelection, const bool bAllowImplicitSync = false );

	/** Finds the item that represents the specified path, if it exists. */
	TSharedPtr<FTreeItem> FindItemRecursive(const FString& Path) const;

	/** Sets the state of the path view to the one described by the history data */
	void ApplyHistoryData( const FHistoryData& History );

	/** Saves any settings to config that should be persistent between editor sessions */
	void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const;

	/** Loads any settings to config that should be persistent between editor sessions */
	void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString);

	/** Populates the tree with all folders that are not filtered out */
	void Populate();

private:
	/** Internal sync implementation, syncs to the tree to the given array of items */
	void SyncToInternal( const TArray<FAssetData>& AssetDataList, const TArray<FString>& FolderPaths, const bool bAllowImplicitSync );

	/** Sort the root items into the correct order */
	void SortRootItems();

	/** Gets the content for a context menu */
	TSharedPtr<SWidget> MakePathViewContextMenu();

	/** Called when "new folder" is selected in the context menu */
	void OnCreateNewFolder(const FString& FolderName, const FString& FolderPath);

	/** Selects the given path only if it exists. Returns true if selected. */
	bool ExplicitlyAddPathToSelection(const FString& Path);

	/** Returns true if the selection changed delegate should be allowed */
	bool ShouldAllowTreeItemChangedDelegate() const;

	/** Expands all parents of the specified item */
	void RecursiveExpandParents(const TSharedPtr<FTreeItem>& Item);

	/** Adds a new root folder */
	TSharedPtr<FTreeItem> AddRootItem( const FString& InFolderName );

	/** Creates a list item for the tree view */
	TSharedRef<ITableRow> GenerateTreeRow( TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles focusing a folder widget after it has been created with the intent to rename */
	void TreeItemScrolledIntoView( TSharedPtr<FTreeItem> TreeItem, const TSharedPtr<ITableRow>& Widget );

	/** Handler for returning a list of children associated with a particular tree node */
	void GetChildrenForTree( TSharedPtr< FTreeItem > TreeItem, TArray< TSharedPtr<FTreeItem> >& OutChildren );

	/** Handler for recursively expanding/collapsing items in the tree view */
	void SetTreeItemExpansionRecursive( TSharedPtr< FTreeItem > TreeItem, bool bInExpansionState );

	/** Handler for tree view selection changes */
	void TreeSelectionChanged( TSharedPtr< FTreeItem > TreeItem, ESelectInfo::Type SelectInfo );

	/** Handler for tree view expansion changes */
	void TreeExpansionChanged( TSharedPtr< FTreeItem > TreeItem, bool bIsExpanded );

	/** Handler for when search terms change in the asset tree search box */
	void OnAssetTreeSearchBoxChanged( const FText& InSearchText );

	/** Handler for when the search box filter has changed */
	void FilterUpdated();

	/** Gets the string to highlight in tree items (used in folder searching) */
	FText GetHighlightText() const;

	/** Populates OutSearchStrings with the strings that should be used in searching */
	void PopulateFolderSearchStrings( const FString& FolderName, OUT TArray< FString >& OutSearchStrings ) const;

	/** Handler for tree view folders are dragged */
	FReply OnFolderDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	/** Handler use dto verify the name of a new folder */
	bool VerifyFolderNameChanged(const FString& InName, FText& OutErrorMessage, const FString& InFolderPath) const;

	/** Handler for when a name was given to a new folder */
	void FolderNameChanged( const TSharedPtr< FTreeItem >& TreeItem, const FString& OldPath, const FVector2D& MessageLocation, const ETextCommit::Type CommitType );

	/** Returns true if the supplied folder item already exists in the tree. If so, ExistingItem will be set to the found item. */
	bool FolderAlreadyExists(const TSharedPtr< FTreeItem >& TreeItem, TSharedPtr< FTreeItem >& ExistingItem);

	/** Removes the supplied folder from the tree. */
	void RemoveFolderItem(const TSharedPtr< FTreeItem >& TreeItem);

	/** Handler for when assets or asset paths are dropped on a tree item */
	void TreeAssetsOrPathsDropped(const TArray<FAssetData>& AssetList, const TArray<FString>& AssetPaths, const TSharedPtr<FTreeItem>& TreeItem);

	/** Handler for when asset paths are dropped on a tree item */
	void TreeFilesDropped(const TArray<FString>& FileNames, const TSharedPtr<FTreeItem>& TreeItem);

	/** True if the specified item is expanded in the asset tree */
	bool IsTreeItemExpanded(TSharedPtr<FTreeItem> TreeItem) const;

	/** True if the specified item is selected in the asset tree */
	bool IsTreeItemSelected(TSharedPtr<FTreeItem> TreeItem) const;

	/** Gets all the UObjects represented by assets in the AssetList */
	void GetDroppedObjects(const TArray<FAssetData>& AssetList, TArray<UObject*>& OutDroppedObjects);

	/** Handler for the user selecting to copy assets or asset paths to the specified folder */
	void ExecuteTreeDropCopy(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath);

	/** Handler for the user selecting to move assets or asset paths to the specified folder */
	void ExecuteTreeDropMove(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath);

	/** Handles updating the content browser when an asset path is added to the asset registry */
	void OnAssetRegistryPathAdded(const FString& Path);

	/** Handles updating the content browser when an asset path is removed from the asset registry */
	void OnAssetRegistryPathRemoved(const FString& Path);

	/** Notification for when the Asset Registry has completed it's initial search */
	void OnAssetRegistrySearchCompleted();

	/** Handles updating the content browser when a path is populated with an asset for the first time */
	void OnFolderPopulated(const FString& Path);

	/** Called from an engine core event when a new content path has been added or removed, so that we can refresh our root set of paths */
	void OnContentPathMountedOrDismounted( const FString& AssetPath, const FString& FileSystemPath );

	/** Called when the class hierarchy is updated due to the available modules changing */
	void OnClassHierarchyUpdated();

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** One-off active timer to focus the widget post-construct */
	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);

	/** One-off active timer to repopulate the path view */
	EActiveTimerReturnType TriggerRepopulate(double InCurrentTime, float InDeltaTime);

private:
	/** A helper class to manage PreventTreeItemChangedDelegateCount by incrementing it when constructed (on the stack) and decrementing when destroyed */
	class FScopedPreventTreeItemChangedDelegate
	{
	public:
		FScopedPreventTreeItemChangedDelegate(const TSharedRef<SPathView>& InPathView)
			: PathView(InPathView)
		{
			PathView->PreventTreeItemChangedDelegateCount++;
		}

		~FScopedPreventTreeItemChangedDelegate()
		{
			check(PathView->PreventTreeItemChangedDelegateCount > 0);
			PathView->PreventTreeItemChangedDelegateCount--;
		}

	private:
		TSharedRef<SPathView> PathView;
	};

	/** The tree view widget */
	TSharedPtr< STreeView< TSharedPtr<FTreeItem>> > TreeViewPtr;

	/** The asset tree search box */
	TSharedPtr< SSearchBox > SearchBoxPtr;

	/** The list of folders in the tree */
	TArray< TSharedPtr<FTreeItem> > TreeRootItems;

	/** Delegate to invoke when selection changes. */
	FOnPathSelected OnPathSelected;

	/** Delegate to invoke when generating the context menu for a folder */
	FOnGetFolderContextMenu OnGetFolderContextMenu;

	/** Delegate to invoke when a context menu for a folder is opening. */
	FContentBrowserMenuExtender_SelectedPaths OnGetPathContextMenuExtender;

	/** The The TextFilter attached to the SearchBox widget */
	TSharedPtr< FolderTextFilter > SearchBoxFolderFilter;

	/** The paths that were last reported by OnPathSelected event. Used in preserving selection when filtering folders */
	TSet< FString > LastSelectedPaths;

	/** The paths that were last reported by OnPathExpanded event. Used in preserving expansion when filtering folders */
	TSet< FString > LastExpandedPaths;

	/** If not empty, this is the path of the folders to sync once they are available while assets are still being discovered */
	TArray<FString> PendingInitialPaths;

	/** If > 0, the selection or expansion changed delegate will not be called. Used to update the tree from an external source or in certain bulk operations. */
	int32 PreventTreeItemChangedDelegateCount;

	/** If false, the context menu will not open when right clicking an item in the tree */
	bool bAllowContextMenu;

	/** If false, the classes folder will not be added to the tree automatically */
	bool bAllowClassesFolder;
};
