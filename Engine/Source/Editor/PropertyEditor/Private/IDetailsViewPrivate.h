// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetThumbnail.h"
#include "DetailTreeNode.h"
#include "PropertyNode.h"
#include "IDetailsView.h"

class FNotifyHook;
class IDetailPropertyExtensionHandler;
class IDetailRootObjectCustomization;

class IDetailsViewPrivate : public IDetailsView
{
public:
	/**
	 * Sets the expansion state for a node and optionally all of its children
	 *
	 * @param InTreeNode		The node to change expansion state on
	 * @param bIsItemExpanded	The new expansion state
	 * @param bRecursive		Whether or not to apply the expansion change to any children
	 */
	virtual void SetNodeExpansionState(TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded, bool bRecursive) = 0;

	/**
	 * Requests that an item in the tree be expanded or collapsed
	 *
	 * @param TreeNode	The tree node to expand
	 * @param bExpand	true if the item should be expanded, false otherwise
	 */
	virtual void RequestItemExpanded(TSharedRef<FDetailTreeNode> TreeNode, bool bExpand) = 0;

	/**
	 * Refreshes the detail's treeview
	 */
	virtual void RefreshTree() = 0;

	/** Returns the notify hook to use when properties change */
	virtual FNotifyHook* GetNotifyHook() const = 0;

	/**
	 * Returns the property utilities for this view
	 */
	virtual TSharedPtr<IPropertyUtilities> GetPropertyUtilities() = 0;

	/** Causes the details view to be refreshed (new widgets generated) with the current set of objects */
	virtual void ForceRefresh() = 0;

	/** Causes the details view to move the scroll offset (by item)
	 * @param DeltaOffset	We add this value to the current scroll offset if the result is in the scrolling range
	 */
	virtual void MoveScrollOffset(int32 DeltaOffset) = 0;

	/**
	 * Saves the expansion state of a tree node
	 *
	 * @param NodePath	The path to the detail node to save
	 * @param bIsExpanded	true if the node is expanded, false otherwise
	 */
	virtual void SaveCustomExpansionState(const FString& NodePath, bool bIsExpanded) = 0;

	/**
	 * Gets the saved expansion state of a tree node in this category
	 *
	 * @param NodePath	The path to the detail node to get
	 * @return true if the node should be expanded, false otherwise
	 */
	virtual bool GetCustomSavedExpansionState(const FString& NodePath) const = 0;

	/**
	 * @return True if the property is visible
	 */
	virtual bool IsPropertyVisible( const struct FPropertyAndParent& PropertyAndParent ) const = 0;

	/**
	 * @return True if the property is visible
	 */
	virtual bool IsPropertyReadOnly( const struct FPropertyAndParent& PropertyAndParent ) const = 0;

	virtual TSharedPtr<IDetailPropertyExtensionHandler> GetExtensionHandler() = 0;

	/**
	 * @return The thumbnail pool that should be used for thumbnails being rendered in this view
	 */
	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const = 0;

	/**
	 * Creates the color picker window for this property view.
	 *
	 * @param PropertyEditor				The slate property node to edit.
	 * @param bUseAlpha			Whether or not alpha is supported
	 */
	virtual void CreateColorPickerWindow(const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha) = 0;

	/**
	 * Adds an action to execute next tick
	 */
	virtual void EnqueueDeferredAction(FSimpleDelegate& DeferredAction) = 0;

	/**
	 * Called when properties have finished changing (after PostEditChange is called)
	 */
	virtual void NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) = 0;

	/**
	 * Reruns the current search filter applied to the details panel to account for any new changes                                                              
	 */
	virtual void RerunCurrentFilter() = 0;

	/** If a customization standalone widget is used, the value should be update only once, when its window is closed */
	virtual bool DontUpdateValueWhileEditing() const = 0;

	/** @return Whether or not the details panel was created with multiple unrelated objects visible at once in the details panel */
	virtual bool ContainsMultipleTopLevelObjects() const = 0;

	/** @return the customization instance that defines how the display for a root object looks */
	virtual TSharedPtr<class IDetailRootObjectCustomization> GetRootObjectCustomization() const = 0;

	/** Runs the details customization update on a root property node */
	virtual void UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode> InRootPropertyNode, struct FDetailLayoutData& LayoutData) = 0;

	virtual const FCustomPropertyTypeLayoutMap& GetCustomPropertyTypeLayoutMap() const = 0;

	/** Saves the expansion state of property nodes for the selected object set */
	virtual void SaveExpandedItems(TSharedRef<FPropertyNode> StartNode) = 0;
};
