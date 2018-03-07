// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "PropertyPath.h"
#include "IPropertyUtilities.h"
#include "DetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "DetailCategoryBuilderImpl.h"

/**
 * A single item in a detail tree                                                              
 */
class FDetailItemNode : public FDetailTreeNode, public TSharedFromThis<FDetailItemNode>
{
public:
	FDetailItemNode( const FDetailLayoutCustomization& InCustomization, TSharedRef<FDetailCategoryImpl> InParentCategory, TAttribute<bool> InIsParentEnabled, TSharedPtr<IDetailGroup> InParentGroup = nullptr);
	~FDetailItemNode();

	/** IDetailTreeNode interface */
	virtual EDetailNodeType GetNodeType() const override;
	virtual TSharedPtr<IPropertyHandle> CreatePropertyHandle() const override;

	/**
	 * Initializes this node                                                              
	 */
	void Initialize();


	void ToggleExpansion();

	void SetExpansionState(bool bWantsExpanded, bool bSaveState);

	/**
	 * Generates children for this node
	 *
	 * @param bUpdateFilteredNodes If true, details panel will re-filter to account for new nodes being added
	 */
	void GenerateChildren( bool bUpdateFilteredNodes );

	/**
	 * @return TRUE if this node has a widget with multiple columns                                                              
	 */
	bool HasMultiColumnWidget() const;

	/**
	 * @return true if this node has any children (regardless of child visibility)
	 */
	bool HasGeneratedChildren() const { return Children.Num() > 0;}

	/** IDetailTreeNode interface */
	virtual IDetailsViewPrivate* GetDetailsView() const override{ return ParentCategory.Pin()->GetDetailsView(); }
	virtual TSharedRef< ITableRow > GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem) override;
	virtual bool GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const override;
	virtual void GetChildren( FDetailNodeList& OutChildren )  override;
	virtual void OnItemExpansionChanged( bool bInIsExpanded, bool bShouldSaveState) override;
	virtual bool ShouldBeExpanded() const override;
	virtual ENodeVisibility GetVisibility() const override;
	virtual void FilterNode( const FDetailFilter& InFilter ) override;
	virtual void Tick( float DeltaTime ) override;
	virtual bool ShouldShowOnlyChildren() const override;
	virtual FName GetNodeName() const override;
	virtual TSharedPtr<FDetailCategoryImpl> GetParentCategory() override { return ParentCategory.Pin(); }
	virtual FPropertyPath GetPropertyPath() const override;
	virtual void SetIsHighlighted(bool bInIsHighlighted) override { bIsHighlighted = bInIsHighlighted; }
	virtual bool IsHighlighted() const override { return bIsHighlighted; }
	virtual bool IsLeaf() override { return true; }
	virtual TAttribute<bool> IsPropertyEditingEnabled() const override { return IsParentEnabled; }

private:

	/**
	 * Initializes the property editor on this node                                                              
	 */
	void InitPropertyEditor();

	/**
	 * Initializes the custom builder on this node                                                              
	 */
	void InitCustomBuilder();

	/**
	 * Initializes the detail group on this node                                                              
	 */
	void InitGroup();

private:
	/** Customization on this node */
	FDetailLayoutCustomization Customization;
	/** Child nodes of this node */
	FDetailNodeList Children;
	/** Parent categories on this node */
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	/** Parent group on this node */
	TWeakPtr<IDetailGroup> ParentGroup;
	/** Attribute for checking if our parent is enabled */
	TAttribute<bool> IsParentEnabled;
	/** Cached visibility of this node */
	EVisibility CachedItemVisibility;
	/** True if this node passes filtering */
	bool bShouldBeVisibleDueToFiltering;
	/** True if this node is visible because its children are filtered successfully */
	bool bShouldBeVisibleDueToChildFiltering;
	/** True if this node should be ticked */
	bool bTickable;
	/** True if this node is expanded */
	bool bIsExpanded;
	/** True if this node is highlighted */
	bool bIsHighlighted;
};
