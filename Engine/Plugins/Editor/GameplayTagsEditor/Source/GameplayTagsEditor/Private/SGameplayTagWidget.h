// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/Object.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "GameplayTagsManager.h"

class IPropertyHandle;
class SAddNewGameplayTagWidget;

/** Determines the behavior of the gameplay tag UI depending on where it's used */
enum class EGameplayTagUIMode : uint8
{
	SelectionMode,
	ManagementMode,
};

/** Widget allowing user to tag assets with gameplay tags */
class SGameplayTagWidget : public SCompoundWidget
{
public:

	/** Called when a tag status is changed */
	DECLARE_DELEGATE( FOnTagChanged )

	SLATE_BEGIN_ARGS( SGameplayTagWidget )
		: _Filter()
		, _NewTagName(TEXT(""))
		, _ReadOnly( false )
		, _TagContainerName( TEXT("") )
		, _MultiSelect( true )
		, _NewTagControlsInitiallyExpanded( false )
		, _PropertyHandle( NULL )
		, _GameplayTagUIMode( EGameplayTagUIMode::SelectionMode )
		, _MaxHeight(260.0f)
	{}
		SLATE_ARGUMENT( FString, Filter ) // Comma delimited string of tag root names to filter by
		SLATE_ARGUMENT( FString, NewTagName ) // String that will initially populate the New Tag Name field
		SLATE_ARGUMENT( bool, ReadOnly ) // Flag to set if the list is read only
		SLATE_ARGUMENT( FString, TagContainerName ) // The name that will be used for the settings file
		SLATE_ARGUMENT( bool, MultiSelect ) // If we can select multiple entries
		SLATE_ARGUMENT( bool, NewTagControlsInitiallyExpanded ) // If the create new tag controls are initially expanded
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, PropertyHandle )
		SLATE_EVENT( FOnTagChanged, OnTagChanged ) // Called when a tag status changes
		SLATE_ARGUMENT( EGameplayTagUIMode, GameplayTagUIMode )	// Determines behavior of the menu based on where it's used
		SLATE_ARGUMENT( float, MaxHeight )	// caps the height of the gameplay tag tree
	SLATE_END_ARGS()

	/** Simple struct holding a tag container and its owner for generic re-use of the widget */
	struct FEditableGameplayTagContainerDatum
	{
		/** Constructor */
		FEditableGameplayTagContainerDatum(class UObject* InOwnerObj, struct FGameplayTagContainer* InTagContainer)
			: TagContainerOwner(InOwnerObj)
			, TagContainer(InTagContainer)
		{}

		/** Owning UObject of the container being edited */
		TWeakObjectPtr<class UObject> TagContainerOwner;

		/** Tag container to edit */
		struct FGameplayTagContainer* TagContainer; 
	};

	/** Construct the actual widget */
	void Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagContainerDatum>& EditableTagContainers);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Ensures that this widget will always account for the MaxHeight if it's specified */
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/** Updates the tag list when the filter text changes */
	void OnFilterTextChanged( const FText& InFilterText );

	/** Returns true if this TagNode has any children that match the current filter */
	bool FilterChildrenCheck( TSharedPtr<FGameplayTagNode>  );

	/** Returns true if we're currently adding a new tag to an INI file */
	bool IsAddingNewTag() const;

	/** Refreshes the tags that should be displayed by the widget */
	void RefreshTags();

	/** Forces the widget to refresh its tags on the next tick */
	void RefreshOnNextTick();

private:

	/* string that sets the section of the ini file to use for this class*/ 
	static const FString SettingsIniSection;

	/* Holds the Name of this TagContainer used for saving out expansion settings */
	FString TagContainerName;

	/* Filter string used during search box */
	FString FilterString;

	/** root filter (passed in on creation) */
	FString RootFilterString;

	/* Flag to set if the list is read only*/
	bool bReadOnly;

	/* Flag to set if we can select multiple items form the list*/
	bool bMultiSelect;

	/** Tracks if the Add Tag UI is expanded */
	bool bAddTagSectionExpanded;

	/** If true, refreshes tags on the next frame */
	bool bDelayRefresh;

	/** The maximum height of the gameplay tag tree. If 0, the height is unbound. */
	float MaxHeight;

	/* Array of tags to be displayed in the TreeView*/
	TArray< TSharedPtr<FGameplayTagNode> > TagItems;

	/* Array of tags to be displayed in the TreeView*/
	TArray< TSharedPtr<FGameplayTagNode> > FilteredTagItems;

	/** Container widget holding the tag tree */
	TSharedPtr<SBorder> TagTreeContainerWidget;

	/** Tree widget showing the gameplay tag library */
	TSharedPtr< STreeView< TSharedPtr<FGameplayTagNode> > > TagTreeWidget;

	/** The widget that controls how new gameplay tags are added to the config files */
	TSharedPtr<class SAddNewGameplayTagWidget> AddNewTagWidget;

	/** Allows for the user to find a specific gameplay tag in the tree */
	TSharedPtr<SSearchBox> SearchTagBox;

	/** Containers to modify */
	TArray<FEditableGameplayTagContainerDatum> TagContainers;

	/** Called when the Tag list changes*/
	FOnTagChanged OnTagChanged;

	/** Determines behavior of the widget */
	EGameplayTagUIMode GameplayTagUIMode;

	TSharedPtr<IPropertyHandle> PropertyHandle;

	/**
	 * Generate a row widget for the specified item node and table
	 * 
	 * @param InItem		Tag node to generate a row widget for
	 * @param OwnerTable	Table that owns the row
	 * 
	 * @return Generated row widget for the item node
	 */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FGameplayTagNode> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Get children nodes of the specified node
	 * 
	 * @param InItem		Node to get children of
	 * @param OutChildren	[OUT] Array of children nodes, if any
	 */
	void OnGetChildren(TSharedPtr<FGameplayTagNode> InItem, TArray< TSharedPtr<FGameplayTagNode> >& OutChildren);

	/**
	 * Called via delegate when the status of a check box in a row changes
	 * 
	 * @param NewCheckState	New check box state
	 * @param NodeChanged	Node that was checked/unchecked
	 */
	void OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FGameplayTagNode> NodeChanged);

	/**
	 * Called via delegate to determine the checkbox state of the specified node
	 * 
	 * @param Node	Node to find the checkbox state of
	 * 
	 * @return Checkbox state of the specified node
	 */
	ECheckBoxState IsTagChecked(TSharedPtr<FGameplayTagNode> Node) const;

	/**
	 * Helper function called when the specified node is checked
	 * 
	 * @param NodeChecked	Node that was checked by the user
	 */
	void OnTagChecked(TSharedPtr<FGameplayTagNode> NodeChecked);

	/**
	 * Helper function called when the specified node is unchecked
	 * 
	 * @param NodeUnchecked	Node that was unchecked by the user
	 */
	void OnTagUnchecked(TSharedPtr<FGameplayTagNode> NodeUnchecked);

	/**
	 * Recursive function to uncheck all child tags
	 * 
	 * @param NodeUnchecked	Node that was unchecked by the user
	 * @param EditableContainer The container we are removing the tags from
	 */
	void UncheckChildren(TSharedPtr<FGameplayTagNode> NodeUnchecked, FGameplayTagContainer& EditableContainer);

	/** Called when the user clicks the "Clear All" button; Clears all tags */
	FReply OnClearAllClicked();

	/** Called when the user clicks the "Expand All" button; Expands the entire tag tree */
	FReply OnExpandAllClicked();

	/** Called when the user clicks the "Collapse All" button; Collapses the entire tag tree */
	FReply OnCollapseAllClicked();

	/**
	 * Helper function to set the expansion state of the tree widget
	 * 
	 * @param bExpand If true, expand the entire tree; Otherwise, collapse the entire tree
	 */
	void SetTagTreeItemExpansion(bool bExpand);

	/**
	 * Helper function to set the expansion state of a specific node
	 * 
	 * @param Node		Node to set the expansion state of
	 * @param bExapnd	If true, expand the node; Otherwise, collapse the node
	 */
	void SetTagNodeItemExpansion(TSharedPtr<FGameplayTagNode> Node, bool bExpand);
	
	/**
	 * Helper function to ensure the tag assets are only tagged with valid tags from
	 * the global library. Strips any invalid tags.
	 */
	void VerifyAssetTagValidity();

	/** Load settings for the tags*/
	void LoadSettings();

	/** Helper function to determine the visibility of the expandable UI controls */
	EVisibility DetermineExpandableUIVisibility() const;

	/** Helper function to determine the visibility of the Add New Tag widget */
	EVisibility DetermineAddNewTagWidgetVisibility() const;

	/** Helper function to determine the visibility of the Clear Selection button */
	EVisibility DetermineClearSelectionVisibility() const;

	/** Recursive load function to go through all tags in the tree and set the expansion*/
	void LoadTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node );

	/** Recursive function to go through all tags in the tree and set the expansion to default*/
	void SetDefaultTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node );

	/** Expansion changed callback */
	void OnExpansionChanged( TSharedPtr<FGameplayTagNode> InItem, bool bIsExpanded );

	/** Callback for when a new tag is added */
	void OnGameplayTagAdded(const FString& TagName, const FString& TagComment, const FName& TagSource);

	/** Callback when the user wants to add a subtag to an existing tag */
	FReply OnAddSubtagClicked(TSharedPtr<FGameplayTagNode> InTagNode);

	/** Creates a dropdown menu to provide additional functionality for tags (renaming, deletion, search for references, etc.) */
	TSharedRef<SWidget> MakeTagActionsMenu(TSharedPtr<FGameplayTagNode> InTagNode);

	/** Attempts to rename the tag through a dialog box */
	void OnRenameTag(TSharedPtr<FGameplayTagNode> InTagNode);

	/** Attempts to delete the specified tag */
	void OnDeleteTag(TSharedPtr<FGameplayTagNode> InTagNode);

	/** Searches for all references for the selected tag */
	void OnSearchForReferences(TSharedPtr<FGameplayTagNode> InTagNode);

	/** Returns true if the user can select tags from the widget */
	bool CanSelectTags() const;

	/** Determines if the expandable UI that contains the Add New Tag widget should be expanded or collapsed */
	ECheckBoxState GetAddTagSectionExpansionState() const;

	/** Callback for when the state of the expandable UI section changes */
	void OnAddTagSectionExpansionStateChanged(ECheckBoxState NewState);

	void SetContainer(FGameplayTagContainer* OriginalContainer, FGameplayTagContainer* EditedContainer, UObject* OwnerObj);

	/** Opens a dialog window to rename the selected tag */
	void OpenRenameGameplayTagDialog(TSharedPtr<FGameplayTagNode> GameplayTagNode) const;

	/** Delegate that is fired when a tag is successfully renamed */
	void OnGameplayTagRenamed(FString OldTagName, FString NewTagName);
};
