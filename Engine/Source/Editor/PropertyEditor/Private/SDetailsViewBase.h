// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "PropertyPath.h"
#include "Input/Reply.h"
#include "AssetThumbnail.h"
#include "IPropertyUtilities.h"
#include "DetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "PropertyNode.h"
#include "Widgets/SWindow.h"
#include "PropertyEditorModule.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/STreeView.h"
#include "IDetailsView.h"
#include "IDetailsViewPrivate.h"
#include "PropertyRowGenerator.h"

class FDetailCategoryImpl;
class FDetailLayoutBuilderImpl;
class FNotifyHook;
class IDetailCustomization;
class IDetailKeyframeHandler;
class IDetailPropertyExtensionHandler;
class SDetailNameArea;



typedef STreeView< TSharedRef<class FDetailTreeNode> > SDetailTree;



/** Represents a filter which controls the visibility of items in the details view */
struct FDetailFilter
{
	FDetailFilter()
		: bShowOnlyModifiedProperties(false)
		, bShowAllAdvanced(false)
		, bShowOnlyDiffering(false)
		, bShowAllChildrenIfCategoryMatches(true)
	{}

	bool IsEmptyFilter() const { return FilterStrings.Num() == 0 && bShowOnlyModifiedProperties == false && bShowAllAdvanced == false && bShowOnlyDiffering == false && bShowAllChildrenIfCategoryMatches == false; }

	/** Any user search terms that items must match */
	TArray<FString> FilterStrings;
	/** If we should only show modified properties */
	bool bShowOnlyModifiedProperties;
	/** If we should show all advanced properties */
	bool bShowAllAdvanced;
	/** If we should only show differing properties */
	bool bShowOnlyDiffering;
	/** If we should show all the children if their category name matches the search */
	bool bShowAllChildrenIfCategoryMatches;
	TSet<FPropertyPath> WhitelistedProperties;
};

struct FDetailColumnSizeData
{
	TAttribute<float> LeftColumnWidth;
	TAttribute<float> RightColumnWidth;
	SSplitter::FOnSlotResized OnWidthChanged;

	void SetColumnWidth(float InWidth) { OnWidthChanged.ExecuteIfBound(InWidth); }
};

class SDetailsViewBase : public IDetailsViewPrivate
{
public:
	SDetailsViewBase()
		: ColumnWidth(.65f)
		, bHasActiveFilter(false)
		, bIsLocked(false)
		, bHasOpenColorPicker(false)
		, bDisableCustomDetailLayouts( false )
		, NumVisbleTopLevelObjectNodes(0)
	{
	}

	virtual ~SDetailsViewBase();

	/**
	* @return true of the details view can be updated from editor selection
	*/
	virtual bool IsUpdatable() const override
	{
		return DetailsViewArgs.bUpdatesFromSelection;
	}

	virtual bool HasActiveSearch() const override
	{
		return CurrentFilter.FilterStrings.Num() > 0;
	}

	virtual int32 GetNumVisibleTopLevelObjects() const override
	{
		return NumVisbleTopLevelObjectNodes;
	}


	/** @return The identifier for this details view, or NAME_None is this view is anonymous */
	virtual FName GetIdentifier() const override
	{
		return DetailsViewArgs.ViewIdentifier;
	}

	/**
	 * Sets the visible state of the filter box/property grid area
	 */
	virtual void HideFilterArea(bool bHide) override;

	/** 
	 * IDetailsView interface 
	 */
	virtual TArray< FPropertyPath > GetPropertiesInOrderDisplayed() const override;
	virtual void HighlightProperty(const FPropertyPath& Property) override;
	virtual void ShowAllAdvancedProperties() override;
	virtual void SetOnDisplayedPropertiesChanged(FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChangedDelegate) override;
	virtual FOnDisplayedPropertiesChanged& GetOnDisplayedPropertiesChanged() override { return OnDisplayedPropertiesChangedDelegate; }
	virtual void SetDisableCustomDetailLayouts( bool bInDisableCustomDetailLayouts ) override { bDisableCustomDetailLayouts = bInDisableCustomDetailLayouts; }
	virtual void SetIsPropertyVisibleDelegate(FIsPropertyVisible InIsPropertyVisible) override;
	virtual FIsPropertyVisible& GetIsPropertyVisibleDelegate() override { return IsPropertyVisibleDelegate; }
	virtual void SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly InIsPropertyReadOnly) override;
	virtual FIsPropertyReadOnly& GetIsPropertyReadOnlyDelegate() override { return IsPropertyReadOnlyDelegate; }
	virtual void SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled IsPropertyEditingEnabled) override;
	virtual FIsPropertyEditingEnabled& GetIsPropertyEditingEnabledDelegate() override { return IsPropertyEditingEnabledDelegate; }
	virtual bool IsPropertyEditingEnabled() const override;
	virtual void SetKeyframeHandler(TSharedPtr<class IDetailKeyframeHandler> InKeyframeHandler) override;
	virtual TSharedPtr<IDetailKeyframeHandler> GetKeyframeHandler() override;
	virtual void SetExtensionHandler(TSharedPtr<class IDetailPropertyExtensionHandler> InExtensionHandler) override;
	virtual TSharedPtr<IDetailPropertyExtensionHandler> GetExtensionHandler() override;
	virtual bool IsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const override;
	virtual bool IsPropertyReadOnly(const struct FPropertyAndParent& PropertyAndParent) const override;
	virtual void SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance OnGetGenericDetails) override;
	virtual FOnGetDetailCustomizationInstance& GetGenericLayoutDetailsDelegate() override { return GenericLayoutDelegate; }
	virtual bool IsLocked() const override { return bIsLocked; }
	virtual void RefreshRootObjectVisibility() override;
	virtual FOnFinishedChangingProperties& OnFinishedChangingProperties() const override { return OnFinishedChangingPropertiesDelegate; }
	virtual void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate) override;
	virtual void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) override;
	virtual void UnregisterInstancedCustomPropertyLayout(UStruct* Class) override;
	virtual void UnregisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) override;

	/** IDetailsViewPrivate interface */
	virtual void RerunCurrentFilter() override;
	void SetNodeExpansionState(TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded, bool bRecursive) override;
	void SaveCustomExpansionState(const FString& NodePath, bool bIsExpanded) override;
	bool GetCustomSavedExpansionState(const FString& NodePath) const override;
	virtual void NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	void RefreshTree() override;
	TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const override;
	TSharedPtr<IPropertyUtilities> GetPropertyUtilities() override;
	void CreateColorPickerWindow(const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha) override;
	virtual void UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode> InRootPropertyNode, FDetailLayoutData& LayoutData) override;
	virtual FNotifyHook* GetNotifyHook() const override { return DetailsViewArgs.NotifyHook; }
	virtual const FCustomPropertyTypeLayoutMap& GetCustomPropertyTypeLayoutMap() const { return InstancedTypeToLayoutMap; }
	void SaveExpandedItems( TSharedRef<FPropertyNode> StartNode ) override;

	virtual bool IsConnected() const = 0;
	virtual FRootPropertyNodeList& GetRootNodes() = 0;

	/**
	 * Called when the open color picker window associated with this details view is closed
	 */
	void OnColorPickerWindowClosed(const TSharedRef<SWindow>& Window);

	/**
	 * Requests that an item in the tree be expanded or collapsed
	 *
	 * @param TreeNode	The tree node to expand
	 * @param bExpand	true if the item should be expanded, false otherwise
	 */
	void RequestItemExpanded(TSharedRef<FDetailTreeNode> TreeNode, bool bExpand) override;

	/**
	 * Sets the expansion state all root nodes and optionally all of their children
	 *
	 * @param bExpand			The new expansion state
	 * @param bRecurse			Whether or not to apply the expansion change to any children
	 */
	void SetRootExpansionStates(const bool bExpand, const bool bRecurse);

	/** Column width accessibility */
	float OnGetLeftColumnWidth() const { return 1.0f - ColumnWidth; }
	float OnGetRightColumnWidth() const { return ColumnWidth; }
	void OnSetColumnWidth(float InWidth) { ColumnWidth = InWidth; }

	/**
	 * Adds an action to execute next tick
	 */
	virtual void EnqueueDeferredAction(FSimpleDelegate& DeferredAction) override;


	// SWidget interface
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface


	/**
	* Restores the expansion state of property nodes for the selected object set
	*/
	void RestoreExpandedItems(TSharedRef<FPropertyNode> StartNode);


protected:
	/**
	 * Called when a color property is changed from a color picker
	 */
	void SetColorPropertyFromColorPicker(FLinearColor NewColor);

	/** Updates the property map for access when customizing the details view.  Generates default layout for properties */
	void UpdatePropertyMaps();

	virtual void CustomUpdatePropertyMap(TSharedPtr<FDetailLayoutBuilderImpl>& InDetailLayout) {}

	/** Called to get the visibility of the tree view */
	EVisibility GetTreeVisibility() const;

	/** Returns the name of the image used for the icon on the filter button */
	const FSlateBrush* OnGetFilterButtonImageResource() const;

	/** Called when the locked button is clicked */
	FReply OnLockButtonClicked();

	/**
	 * Called to recursively expand/collapse the children of the given item
	 *
	 * @param InTreeNode		The node that was expanded or collapsed
	 * @param bIsItemExpanded	True if the item is expanded, false if it is collapsed
	 */
	void SetNodeExpansionStateRecursive( TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded );

	/**
	 * Called when an item is expanded or collapsed in the detail tree
	 *
	 * @param InTreeNode		The node that was expanded or collapsed
	 * @param bIsItemExpanded	True if the item is expanded, false if it is collapsed
	 */
	void OnItemExpansionChanged( TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded );

	/** 
	 * Function called through a delegate on the TreeView to request children of a tree node 
	 * 
	 * @param InTreeNode		The tree node to get children from
	 * @param OutChildren		The list of children of InTreeNode that should be visible 
	 */
	void OnGetChildrenForDetailTree( TSharedRef<FDetailTreeNode> InTreeNode, TArray< TSharedRef<FDetailTreeNode> >& OutChildren );

	/**
	 * Returns an SWidget used as the visual representation of a node in the treeview.                     
	 */
	TSharedRef<ITableRow> OnGenerateRowForDetailTree( TSharedRef<FDetailTreeNode> InTreeNode, const TSharedRef<STableViewBase>& OwnerTable );

	/** @return true if show only modified is checked */
	bool IsShowOnlyModifiedChecked() const { return CurrentFilter.bShowOnlyModifiedProperties; }

	/** @return true if show all advanced is checked */
	bool IsShowAllAdvancedChecked() const { return CurrentFilter.bShowAllAdvanced; }

	/** @return true if show only differing is checked */
	bool IsShowOnlyDifferingChecked() const { return CurrentFilter.bShowOnlyDiffering; }

	/** @return true if show all advanced is checked */
	bool IsShowAllChildrenIfCategoryMatchesChecked() const { return CurrentFilter.bShowAllChildrenIfCategoryMatches; }

	/** Called when show only modified is clicked */
	void OnShowOnlyModifiedClicked();

	/** Called when show all advanced is clicked */
	void OnShowAllAdvancedClicked();

	/** Called when show only differing is clicked */
	void OnShowOnlyDifferingClicked();

	/** Called when show all children if category matches is clicked */
	void OnShowAllChildrenIfCategoryMatchesClicked();

	/**
	* Updates the details with the passed in filter
	*/
	void UpdateFilteredDetails();

	/** Called when the filter text changes.  This filters specific property nodes out of view */
	void OnFilterTextChanged(const FText& InFilterText);

	/** Called when the list of currently differing properties changes */
	virtual void UpdatePropertiesWhitelist(const TSet<FPropertyPath> InWhitelistedProperties) override { CurrentFilter.WhitelistedProperties = InWhitelistedProperties; }

	virtual TSharedPtr<SWidget> GetNameAreaWidget() override;
	virtual TSharedPtr<SWidget> GetFilterAreaWidget() override;
	virtual TSharedPtr<class FUICommandList> GetHostCommandList() const override;
	virtual TSharedPtr<FTabManager> GetHostTabManager() const override;
	virtual void SetHostTabManager(TSharedPtr<FTabManager> InTabManager) override;

	/** 
	 * Hides or shows properties based on the passed in filter text
	 * 
	 * @param InFilterText	The filter text
	 */
	void FilterView( const FString& InFilterText );

	/** Called to get the visibility of the filter box */
	EVisibility GetFilterBoxVisibility() const;

protected:
	/** The user defined args for the details view */
	FDetailsViewArgs DetailsViewArgs;
	/** A mapping of classes to detail layout delegates, called when querying for custom detail layouts in this instance of the details view only*/
	FCustomDetailLayoutMap InstancedClassToDetailLayoutMap;
	/** A mapping of type names to detail layout delegates, called when querying for custom detail layouts in this instance of the details view only */
	FCustomPropertyTypeLayoutMap InstancedTypeToLayoutMap;
	/** The current detail layout based on objects in this details panel.  There is one layout for each top level object node.*/
	FDetailLayoutList DetailLayouts;
	/** Row for searching and view options */
	TSharedPtr<SHorizontalBox>  FilterRow;
	/** Search box */
	TSharedPtr<SSearchBox> SearchBox;
	/** Customization instances that need to be destroyed when safe to do so */
	TArray< TSharedPtr<IDetailCustomization> > CustomizationClassInstancesPendingDelete;
	/** Map of nodes that are requesting an automatic expansion/collapse due to being filtered */
	TMap< TSharedRef<FDetailTreeNode>, bool > FilteredNodesRequestingExpansionState;
	/** Current set of expanded detail nodes (by path) that should be saved when the details panel closes */
	TSet<FString> ExpandedDetailNodes;
	/** Tree view */
	TSharedPtr<SDetailTree> DetailTree;
	/** Root tree nodes visible in the tree */
	FDetailNodeList RootTreeNodes;
	/** Delegate executed to determine if a property should be visible */
	FIsPropertyVisible IsPropertyVisibleDelegate;
	/** Delegate executed to determine if a property should be read-only */
	FIsPropertyReadOnly IsPropertyReadOnlyDelegate;
	/** Delegate called to see if a property editing is enabled */
	FIsPropertyEditingEnabled IsPropertyEditingEnabledDelegate;
	/** Delegate called when the details panel finishes editing a property (after post edit change is called) */
	mutable FOnFinishedChangingProperties OnFinishedChangingPropertiesDelegate;
	/** Container for passing around column size data to rows in the tree (each row has a splitter which can affect the column size)*/
	FDetailColumnSizeData ColumnSizeData;
	/** The actual width of the right column.  The left column is 1-ColumnWidth */
	float ColumnWidth;
	/** True if there is an active filter (text in the filter box) */
	bool bHasActiveFilter;
	/** True if this property view is currently locked (I.E The objects being observed are not changed automatically due to user selection)*/
	bool bIsLocked;
	/** The property node that the color picker is currently editing. */
	TWeakPtr<FPropertyNode> ColorPropertyNode;
	/** Whether or not this instance of the details view opened a color picker and it is not closed yet */
	bool bHasOpenColorPicker;
	/** Settings for this view */
	TSharedPtr<class IPropertyUtilities> PropertyUtilities;
	/** The name area which is not recreated when selection changes */
	TSharedPtr<class SDetailNameArea> NameArea;
	/** Asset pool for rendering and managing asset thumbnails visible in this view*/
	mutable TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
	/** The current filter */
	FDetailFilter CurrentFilter;
	/** Delegate called to get generic details not specific to an object being viewed */
	FOnGetDetailCustomizationInstance GenericLayoutDelegate;
	/** Actions that should be executed next tick */
	TArray<FSimpleDelegate> DeferredActions;
	/** Root tree nodes that needs to be destroyed when safe */
	FRootPropertyNodeList RootNodesPendingKill;
	/** The handler for the keyframe UI, determines if the key framing UI should be displayed. */
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler;
	/** Property extension handler returns additional UI to apply after the customization is applied to the property. */
	TSharedPtr<IDetailPropertyExtensionHandler> ExtensionHandler;
	/** The tree node that is currently highlighted, may be none: */
	TWeakPtr< FDetailTreeNode > CurrentlyHighlightedNode;

	/** Executed when the tree is refreshed */
	FOnDisplayedPropertiesChanged OnDisplayedPropertiesChangedDelegate;

	/** True if we want to skip generation of custom layouts for displayed object */
	bool bDisableCustomDetailLayouts;

	int32 NumVisbleTopLevelObjectNodes;
};
