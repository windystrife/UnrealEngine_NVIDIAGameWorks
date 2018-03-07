// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "PropertyPath.h"
#include "IPropertyUtilities.h"
#include "IDetailTreeNode.h"

class FDetailCategoryImpl;
class IDetailsViewPrivate;
class ITableRow;
class STableViewBase;
class FDetailWidgetRow;
struct FDetailColumnSizeData;
struct FDetailFilter;

enum class ENodeVisibility : uint8
{
	// Hidden but can be visible if parent is visible due to filtering
	HiddenDueToFiltering,
	// Never visible no matter what
	ForcedHidden,
	// Always visible
	Visible,
};

typedef TArray<TSharedRef<class FDetailTreeNode>> FDetailNodeList;


class FDetailTreeNode : public IDetailTreeNode
{
public:

	/** IDetailTreeNode interface */
	virtual FNodeWidgets CreateNodeWidgets() const;
	virtual void GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren);

	/** @return The details view that this node is in */
	virtual IDetailsViewPrivate* GetDetailsView() const = 0;

	/**
	 * Generates the widget representing this node
	 *
	 * @param OwnerTable		The table owner of the widget being generated
	 * @param PropertyUtilities	Property utilities to help generate widgets
	 */
	virtual TSharedRef< ITableRow > GenerateWidgetForTableView(const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem) = 0;

	virtual bool GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const = 0;

	/**
	 * Filters this nodes visibility based on the provided filter strings
	 */
	virtual void FilterNode(const FDetailFilter& InFilter) = 0;

	/**
	 * Gets child tree nodes
	 *
	 * @param OutChildren	The array to add children to
	 */
	virtual void GetChildren(FDetailNodeList& OutChildren) = 0;

	/**
	 * Called when the item is expanded in the tree
	 */
	virtual void OnItemExpansionChanged(bool bIsExpanded, bool bShouldSaveState) = 0;

	/**
	 * @return Whether or not the tree node should be expanded
	 */
	virtual bool ShouldBeExpanded() const = 0;

	/**
	 * @return the visibility of this node in the tree
	 */
	virtual ENodeVisibility GetVisibility() const = 0;

	/**
	 * Called each frame if the node requests that it should be ticked
	 */
	virtual void Tick(float DeltaTime) = 0;

	/**
	 * @return true to ignore this node for visibility in the tree and only examine children
	 */
	virtual bool ShouldShowOnlyChildren() const = 0;

	/**
	 * The identifier name of the node
	 */
	virtual FName GetNodeName() const = 0;

	/**
	 * @return The category node that this node is nested in, if any:
	 */
	virtual TSharedPtr<FDetailCategoryImpl> GetParentCategory() { return TSharedPtr<FDetailCategoryImpl>(); }

	/**
	 * @return The property path that this node is associate with, if any:
	 */
	virtual FPropertyPath GetPropertyPath() const { return FPropertyPath(); }

	/**
	 * Called when the node should appear 'highlighted' to draw the users attention to it
	 */
	virtual void SetIsHighlighted(bool bInIsHighlighted) {}

	/**
	 * @return true if the node has been highlighted
	 */
	virtual bool IsHighlighted() const { return false; }

	/**
	 * @return true if this is a leaf node:
	 */
	virtual bool IsLeaf() { return false; }

	/**
	 * @return TAttribute indicating whether editing is enabled or whether the property is readonly:
	 */
	virtual TAttribute<bool> IsPropertyEditingEnabled() const { return false; }
};

