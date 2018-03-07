// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "IPropertyUtilities.h"
#include "DetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "DetailCategoryBuilderImpl.h"
#include "SDetailTableRowBase.h"

class SDetailCategoryTableRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS( SDetailCategoryTableRow )
		: _InnerCategory( false )
		, _ShowBorder( true )
		, _ColumnSizeData(nullptr)
	{}
		SLATE_ARGUMENT( FText, DisplayName )
		SLATE_ARGUMENT( bool, InnerCategory )
		SLATE_ARGUMENT( TSharedPtr<SWidget>, HeaderContent )
		SLATE_ARGUMENT( bool, ShowBorder )
		SLATE_ARGUMENT( const FDetailColumnSizeData*, ColumnSizeData )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView );
private:
	EVisibility IsSeparatorVisible() const;
	const FSlateBrush* GetBackgroundImage() const;
private:
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	void OnColumnResized(float InNewWidth);
private:
	bool bIsInnerCategory;
	bool bShowBorder;
};


class FDetailCategoryGroupNode : public FDetailTreeNode, public TSharedFromThis<FDetailCategoryGroupNode>
{
public:
	FDetailCategoryGroupNode( const FDetailNodeList& InChildNodes, FName InGroupName, FDetailCategoryImpl& InParentCategory );

public:

	void SetShowBorder(bool bInShowBorder) { bShowBorder = bInShowBorder; }
	bool GetShowBorder() const { return bShowBorder; }

	void SetHasSplitter(bool bInHasSplitter) { bHasSplitter = bInHasSplitter; }
	bool GetHasSplitter() const { return bHasSplitter; }

private:
	virtual IDetailsViewPrivate* GetDetailsView() const override{ return ParentCategory.GetDetailsView(); }
	virtual void OnItemExpansionChanged( bool bIsExpanded, bool bShouldSaveState) override {}
	virtual bool ShouldBeExpanded() const override { return true; }
	virtual ENodeVisibility GetVisibility() const override { return bShouldBeVisible ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering; }
	virtual TSharedRef< ITableRow > GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem) override;
	virtual bool GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const override;

	virtual EDetailNodeType GetNodeType() const override { return EDetailNodeType::Category; }
	virtual TSharedPtr<IPropertyHandle> CreatePropertyHandle() const override { return nullptr; }

	virtual void GetChildren(FDetailNodeList& OutChildren )  override;
	virtual void FilterNode( const FDetailFilter& InFilter ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override { return false; }
	virtual FName GetNodeName() const override { return NAME_None; }
private:
	FDetailNodeList ChildNodes;
	FDetailCategoryImpl& ParentCategory;
	FName GroupName;
	bool bShouldBeVisible;

	bool bShowBorder;
	bool bHasSplitter;
};
