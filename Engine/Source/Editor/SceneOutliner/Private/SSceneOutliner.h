// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorUndoClient.h"
#include "Misc/TextFilter.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerPublicTypes.h"
#include "Editor/SceneOutliner/Private/SOutlinerTreeView.h"
#include "Editor/SceneOutliner/Private/SceneOutlinerStandaloneTypes.h"

class FMenuBuilder;
class ISceneOutlinerColumn;
class SComboButton;

template<typename ItemType> class STreeView;

/**
 * Scene Outliner definition
 * Note the Scene Outliner is also called the World Outliner
 */
namespace SceneOutliner
{
	typedef TTextFilter< const ITreeItem& > TreeItemTextFilter;

	/** Structure that defines an operation that should be applied to the tree */
	struct FPendingTreeOperation
	{
		enum EType { Added, Removed, Moved };
		FPendingTreeOperation(EType InType, TSharedRef<ITreeItem> InItem) : Type(InType), Item(InItem) { }

		/** The type of operation that is to be applied */
		EType Type;

		/** The tree item to which this operation relates */
		FTreeItemRef Item;
	};

	/** Set of actions to apply to new tree items */
	namespace ENewItemAction 
	{
		enum Type
		{
			/** Select the item when it is created */
			Select			= 1 << 0,
			/** Scroll the item into view when it is created */
			ScrollIntoView	= 1 << 1,
			/** Interactively rename the item when it is created (implies the above) */
			Rename			= 1 << 2,
		};
	}

	/** Get a description of a world to display in the scene outliner */
	FText GetWorldDescription(UWorld* World);

	/**
	 * Scene Outliner widget
	 */
	class SSceneOutliner : public ISceneOutliner, public FEditorUndoClient
	{

	public:

		SLATE_BEGIN_ARGS( SSceneOutliner ){}

			SLATE_ARGUMENT( FOnSceneOutlinerItemPicked, OnItemPickedDelegate )

		SLATE_END_ARGS()

		/**
		 * Construct this widget.  Called by the SNew() Slate macro.
		 *
		 * @param	InArgs		Declaration used by the SNew() macro to construct this widget
		 * @param	InitOptions	Programmer-driven initialization options for this widget
		 */
		void Construct( const FArguments& InArgs, const FInitializationOptions& InitOptions );

		/** Default constructor - initializes data that is shared between all tree items */
		SSceneOutliner() : SharedData(MakeShareable( new FSharedOutlinerData )) {}

		/** SSceneOutliner destructor */
		~SSceneOutliner();

		/** SWidget interface */
		virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
		virtual bool SupportsKeyboardFocus() const override;
		virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

		/**	Broadcasts whenever the current selection changes */
		FSimpleMulticastDelegate SelectionChanged;

		/** Sends a requests to the Scene Outliner to refresh itself the next chance it gets */
		virtual void Refresh() override;

		//~ Begin FEditorUndoClient Interface
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
		// End of FEditorUndoClient

		/** @return Returns the common data for this outliner */
		virtual const FSharedOutlinerData& GetSharedData() const override
		{
			return *SharedData;
		}

		/** Get a const reference to the actual tree hierarchy */
		virtual const STreeView<FTreeItemPtr>& GetTree() const override
		{
			return *OutlinerTreeView;
		}

		/** @return Returns a string to use for highlighting results in the outliner list */
		virtual TAttribute<FText> GetFilterHighlightText() const override;

		/** Set the keyboard focus to the outliner */
		virtual void SetKeyboardFocus() override;
		
	private:
		/** Methods that implement structural modification logic for the tree */

		/** Empty all the tree item containers maintained by this outliner */
		void EmptyTreeItems();

		/** Apply incremental changes to, or a complete repopulation of the tree  */
		void Populate();

		/** Repopulates the entire tree */
		void RepopulateEntireTree();

		/** Tells the scene outliner that it should do a full refresh, which will clear the entire tree and rebuild it from scratch. */
		void FullRefresh();

		/** Attempts to add an item to the tree. Will add any parents if required. */
		bool AddItemToTree(FTreeItemRef InItem);
		
		/** Add an item to the tree, even if it doesn't match the filter terms. Used to add parent's that would otherwise be filtered out */
		void AddUnfilteredItemToTree(FTreeItemRef Item);

		/** Ensure that the specified item's parent is added to the tree, if applicable */
		FTreeItemPtr EnsureParentForItem(FTreeItemRef Item);

		/** Remove the specified item from the tree */
		void RemoveItemFromTree(FTreeItemRef InItem);

		/** Called when a child has been removed from the specified parent. Will potentially remove the parent from the tree */
		void OnChildRemovedFromParent(ITreeItem& Parent);

		/** Called when a child has been moved in the tree hierarchy */
		void OnItemMoved(const FTreeItemRef& Item);

		/** Adds a new item for the specified type and refreshes the tree, provided it matches the filter terms */
		template<typename TreeItemType, typename DataType>
		void ConstructItemFor(const DataType& Data)
		{
			// We test the filters with a temporary so we don't allocate on the heap unnecessarily
			const TreeItemType Temporary(Data);
			if (Filters->PassesAllFilters(Temporary) && SearchBoxFilter->PassesFilter(Temporary))
			{
				FTreeItemRef NewItem = MakeShareable(new TreeItemType(Data));
				PendingOperations.Emplace(FPendingTreeOperation::Added, NewItem);
				PendingTreeItemMap.Add(NewItem->GetID(), NewItem);
				Refresh();
			}
		}

		/** Visitor that is used to set up type-specific data after tree items are added to the tree */
		struct FOnItemAddedToTree : IMutableTreeItemVisitor
		{
			SSceneOutliner& Outliner;
			FOnItemAddedToTree(SSceneOutliner& InOutliner) : Outliner(InOutliner) {}

			virtual void Visit(FActorTreeItem& Actor) const override;
			virtual void Visit(FFolderTreeItem& Folder) const override;
		};

		/** Friendship required so the visitor can access our guts */
		friend FOnItemAddedToTree;

	public:

		/** Instruct the outliner to perform an action on the specified item when it is created */
		void OnItemAdded(const FTreeItemID& ItemID, uint8 ActionMask);

		/** Get the columns to be displayed in this outliner */
		const TMap<FName, TSharedPtr<ISceneOutlinerColumn>>& GetColumns() const
		{
			return Columns;
		}

	private:

		/** Map of columns that are shown on this outliner. */
		TMap<FName, TSharedPtr<ISceneOutlinerColumn>> Columns;

		/** Set up the columns required for this outliner */
		void SetupColumns(SHeaderRow& HeaderRow);

		/** Populates OutSearchStrings with the strings associated with TreeItem that should be used in searching */
		void PopulateSearchStrings( const ITreeItem& TreeItem, OUT TArray< FString >& OutSearchStrings ) const;


	public:
		/** Miscellaneous helper functions */

		/** Scroll the specified item into view */
		void ScrollItemIntoView(FTreeItemPtr Item);

		/** Open a rename for the specified tree Item */
		void InitiateRename(TSharedRef<ITreeItem> TrreeItem);

	private:

		/** Synchronize the current actor selection in the world, to the tree */
		void SynchronizeActorSelection();

		/** Check that we are reflecting a valid world */
		bool CheckWorld() const { return SharedData->RepresentingWorld != nullptr; }

		/** Check whether we should be showing folders or not in this scene outliner */
		bool ShouldShowFolders() const;

		/** Get an array of selected folders */
		TArray<FFolderTreeItem*> GetSelectedFolders() const;

		/** Checks to see if the actor is valid for displaying in the outliner */
		bool IsActorDisplayable( const AActor* Actor ) const;

		/** @return	Returns true if the filter is currently active */
		bool IsFilterActive() const;

	private:
		/** Tree view event bindings */

		/** Called by STreeView to generate a table row for the specified item */
		TSharedRef< ITableRow > OnGenerateRowForOutlinerTree( FTreeItemPtr Item, const TSharedRef< STableViewBase >& OwnerTable );

		/** Called by STreeView to get child items for the specified parent item */
		void OnGetChildrenForOutlinerTree( FTreeItemPtr InParent, TArray< FTreeItemPtr >& OutChildren );

		/** Called by STreeView when the tree's selection has changed */
		void OnOutlinerTreeSelectionChanged( FTreeItemPtr TreeItem, ESelectInfo::Type SelectInfo );

		/** Called by STreeView when the user double-clicks on an item in the tree */
		void OnOutlinerTreeDoubleClick( FTreeItemPtr TreeItem );

		/** Called by STreeView when an item is scrolled into view */
		void OnOutlinerTreeItemScrolledIntoView( FTreeItemPtr TreeItem, const TSharedPtr<ITableRow>& Widget );

		/** Called when an item in the tree has been collapsed or expanded */
		void OnItemExpansionChanged(FTreeItemPtr TreeItem, bool bIsExpanded) const;

	private:
		/** Level, editor and other global event hooks required to keep the outliner up to date */

		/** Called by USelection::SelectionChangedEvent delegate when the level's selection changes */
		void OnLevelSelectionChanged(UObject* Obj);

		/** Called by the engine when a level is added to the world. */
		void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);

		/** Called by the engine when a level is removed from the world. */
		void OnLevelRemoved(ULevel* InLevel, UWorld* InWorld);
		
		/** Called by the engine when an actor is added to the world. */
		void OnLevelActorsAdded(AActor* InActor);

		/** Called by the engine when an actor is remove from the world. */
		void OnLevelActorsRemoved(AActor* InActor);

		/** Called by the engine when an actor is attached in the world. */
		void OnLevelActorsAttached(AActor* InActor, const AActor* InParent);

		/** Called by the engine when an actor is dettached in the world. */
		void OnLevelActorsDetached(AActor* InActor, const AActor* InParent);

		/** Called by the engine when an actor is being requested to be renamed */
		void OnLevelActorsRequestRename(const AActor* InActor);

		/** Called by the engine when an actor's folder is changed */
		void OnLevelActorFolderChanged(const AActor* InActor, FName OldPath);

		/** Handler for when a property changes on any object */
		void OnActorLabelChanged(AActor* ChangedActor);

		/** Called when the map has changed*/
		void OnMapChange(uint32 MapFlags);

		/** Called when the current level has changed */
		void OnNewCurrentLevel();

		/** Called when a folder is to be created */
		void OnBroadcastFolderCreate(UWorld& InWorld, FName NewPath);

		/** Called when a folder is to be moved */
		void OnBroadcastFolderMove(UWorld& InWorld, FName OldPath, FName NewPath);

		/** Called when a folder is to be deleted */
		void OnBroadcastFolderDelete(UWorld& InWorld, FName Path);

	private:
		/** Miscellaneous bindings required by the UI */

		/** Called by the editable text control when the filter text is changed by the user */
		void OnFilterTextChanged( const FText& InFilterText );

		/** Called by the editable text control when a user presses enter or commits their text change */
		void OnFilterTextCommitted( const FText& InFilterText, ETextCommit::Type CommitInfo );

		/** Called by the filter button to get the image to display in the button */
		const FSlateBrush* GetFilterButtonGlyph() const;

		/** @return	The filter button tool-tip text */
		FString GetFilterButtonToolTip() const;

		/** @return	Returns whether the filter status line should be drawn */
		EVisibility GetFilterStatusVisibility() const;

		/** @return	Returns the filter status text */
		FText GetFilterStatusText() const;

		/** @return Returns color for the filter status text message, based on success of search filter */
		FSlateColor GetFilterStatusTextColor() const;

		/**	Returns the current visibility of the Empty label */
		EVisibility GetEmptyLabelVisibility() const;

		/** @return the border brush */
		const FSlateBrush* OnGetBorderBrush() const;

		/** @return the the color and opacity of the border brush; green if in PIE/SIE mode */
		FSlateColor OnGetBorderColorAndOpacity() const;

		/** @return the selection mode; disabled entirely if in PIE/SIE mode */
		ESelectionMode::Type GetSelectionMode() const;

		/** @return the content for the view button */
		TSharedRef<SWidget> GetViewButtonContent(bool bWorldPickerOnly);

		/** Build the content for the world picker submenu */
		void BuildWorldPickerContent(FMenuBuilder& MenuBuilder);

		/** @return the foreground color for the view button */
		FSlateColor GetViewButtonForegroundColor() const;

		/** @return the foreground color for the world picker button */
		FSlateColor GetWorldPickerForegroundColor() const;

		/** The brush to use when in Editor mode */
		const FSlateBrush* NoBorder;
		/** The brush to use when in PIE mode */
		const FSlateBrush* PlayInEditorBorder;
		/** The brush to use when in SIE mode */
		const FSlateBrush* SimulateBorder;

	private:

		/** Open a context menu for this scene outliner */
		TSharedPtr<SWidget> OnOpenContextMenu();

		/** Build a context menu for right-clicking an item in the tree */
		TSharedPtr<SWidget> BuildDefaultContextMenu();
		void FillFoldersSubMenu(FMenuBuilder& MenuBuilder) const;
		void AddMoveToFolderOutliner(FMenuBuilder& MenuBuilder) const;
		void FillSelectionSubMenu(FMenuBuilder& MenuBuilder) const;
		TSharedRef<TSet<FName>> GatherInvalidMoveToDestinations() const;

	private:

		/** Select the immediate children of the currently selected folders */
		void SelectFoldersImmediateChildren() const;

		/** Called to select all the descendants of the currently selected folders */
		void SelectFoldersDescendants() const;

		/** Move the selected items to the specified parent */
		void MoveSelectionTo(FTreeItemRef NewParent);

		/** Moves the current selection to the specified folder path */
		void MoveSelectionTo(FName NewParent);

		/** Called when the user has clicked the button to add a new folder */
		FReply OnCreateFolderClicked();

		/** Create a new folder under the specified parent name (NAME_None for root) */
		void CreateFolder();

	private:
		/** FILTERS */

		/** @return whether we are displaying only selected Actors */
		bool IsShowingOnlySelected() const;
		/** Toggles whether we are displaying only selected Actors */
		void ToggleShowOnlySelected();
		/** Enables/Disables whether the SelectedActorFilter is applied */
		void ApplyShowOnlySelectedFilter(bool bShowOnlySelected);

		/** @return whether we are hiding temporary Actors */
		bool IsHidingTemporaryActors() const;
		/** Toggles whether we are hiding temporary Actors */
		void ToggleHideTemporaryActors();
		/** Enables/Disables whether the HideTemporaryActorsFilter is applied */
		void ApplyHideTemporaryActorsFilter(bool bHideTemporaryActors);
		
		/** @return whether we are showing only Actors that are in the Current Level */
		bool IsShowingOnlyCurrentLevel() const;
		/** Toggles whether we are hiding Actors that aren't in the current level */
		void ToggleShowOnlyCurrentLevel();
		/** Enables/Disables whether the ShowOnlyActorsInCurrentLevelFilter is applied */
		void ApplyShowOnlyCurrentLevelFilter(bool bShowOnlyActorsInCurrentLevel);

		/** When applied, only selected Actors are displayed */
		TSharedPtr< FOutlinerFilter > SelectedActorFilter;

		/** When applied, temporary and run-time actors are hidden */
		TSharedPtr< FOutlinerFilter > HideTemporaryActorsFilter;

		/** When applied, only Actors that are in the current level are displayed */
		TSharedPtr< FOutlinerFilter > ShowOnlyActorsInCurrentLevelFilter;

	private:

		/** Context menu opening delegate provided by the client */
		FOnContextMenuOpening OnContextMenuOpening;

		/** Callback that's fired when an item is selected while in 'picking' mode */
		FOnSceneOutlinerItemPicked OnItemPicked;

		/** Shared data required by the tree and its items */
		TSharedRef<FSharedOutlinerData> SharedData;

		/** List of pending operations to be applied to the tree */
		TArray<FPendingTreeOperation> PendingOperations;

		/** Map of actions to apply to new tree items */
		TMap<FTreeItemID, uint8> NewItemActions;

		/** Our tree view */
		TSharedPtr< SOutlinerTreeView > OutlinerTreeView;

		/** A map of all items we have in the tree */
		FTreeItemMap TreeItemMap;

		/** Pending tree items that are yet to be added the tree */
		FTreeItemMap PendingTreeItemMap;

		/** Root level tree items */
		TArray<FTreeItemPtr> RootTreeItems;

		/** A set of all actors that pass the non-text filters in the representing world */
		TSet<TWeakObjectPtr<AActor>> ApplicableActors;

		/** The button that displays view options */
		TSharedPtr<SComboButton> ViewOptionsComboButton;

	private:

		/** Structure containing information relating to the expansion state of parent items in the tree */
		typedef TMap<FTreeItemID, bool> FParentsExpansionState;

		/** Gets the current expansion state of parent items */
		FParentsExpansionState GetParentsExpansionState() const;

		/** Updates the expansion state of parent items after a repopulate, according to the previous state */
		void SetParentsExpansionState(const FParentsExpansionState& ExpansionStateInfo) const;

	private:

		/** Number of actors that passed the search filter */
		int32 FilteredActorCount;

		/** True if the outliner needs to be repopulated at the next appropriate opportunity, usually because our
		    actor set has changed in some way. */
		bool bNeedsRefresh;

		/** true if the Scene Outliner should do a full refresh. */
		bool bFullRefresh;

		/** true when the actor selection state in the world does not match the selection state of the tree */
		bool bActorSelectionDirty;

		/** Reentrancy guard */
		bool bIsReentrant;

		/* Widget containing the filtering text box */
		TSharedPtr< SSearchBox > FilterTextBoxWidget;

		/** A collection of filters used to filter the displayed actors and folders in the scene outliner */
		TSharedPtr< FOutlinerFilters > Filters;
		
		/** The TextFilter attached to the SearchBox widget of the Scene Outliner */
		TSharedPtr< TreeItemTextFilter > SearchBoxFilter;

		/** True if the search box will take keyboard focus next frame */
		bool bPendingFocusNextFrame;

		/** The tree item that is currently pending a rename */
		TWeakPtr<ITreeItem> PendingRenameItem;

	private:
		/** Functions relating to sorting */

		/** Timer for PIE/SIE mode to sort the outliner. */
		float SortOutlinerTimer;

		/** true if the outliner currently needs to be sorted */
		bool bSortDirty;

		/** Specify which column to sort with */
		FName SortByColumn;

		/** Currently selected sorting mode */
		EColumnSortMode::Type SortMode;

		/** Handles column sorting mode change */
		void OnColumnSortModeChanged( const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode );

		/** @return Returns the current sort mode of the specified column */
		EColumnSortMode::Type GetColumnSortMode( const FName ColumnId ) const;

		/** Request that the tree be sorted at a convenient time */
		void RequestSort();

		/** Sort the specified array of items based on the current sort column */
		void SortItems(TArray<FTreeItemPtr>& Items) const;

		/** Select the world we want to view */
		void OnSelectWorld(TWeakObjectPtr<UWorld> InWorld);

		/** Display a checkbox next to the world we are viewing */
		bool IsWorldChecked(TWeakObjectPtr<UWorld> InWorld);

		/** Handler for recursively expanding/collapsing items */
		void SetItemExpansionRecursive(FTreeItemPtr Model, bool bInExpansionState);
	};

}		// namespace SceneOutliner
