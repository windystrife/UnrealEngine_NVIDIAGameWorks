// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "IPropertyUtilities.h"
#include "DetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "DetailCategoryBuilderImpl.h"

class FAdvancedDropdownNode : public FDetailTreeNode, public TSharedFromThis<FAdvancedDropdownNode>
{
public:
	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, const TAttribute<bool>& InExpanded, const TAttribute<bool>& InEnabled, bool bInShouldShowAdvancedButton, bool bInDisplayShowAdvancedMessage, bool bInShowSplitter )
		: ParentCategory( InParentCategory )
		, IsEnabled( InEnabled )
		, IsExpanded( InExpanded )
		, bShouldShowAdvancedButton( bInShouldShowAdvancedButton )
		, bIsTopNode( false )
		, bDisplayShowAdvancedMessage( bInDisplayShowAdvancedMessage )
		, bShowSplitter( bInShowSplitter )
	{}

	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, bool bInIsTopNode )
		: ParentCategory( InParentCategory )
		, bShouldShowAdvancedButton( false )
		, bIsTopNode( bInIsTopNode )
		, bDisplayShowAdvancedMessage( false  )
		, bShowSplitter( false )
	{}
private:
	/** IDetailTreeNode Interface */
	virtual IDetailsViewPrivate* GetDetailsView() const override{ return ParentCategory.GetDetailsView(); }
	virtual TSharedRef< ITableRow > GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem) override;
	virtual bool GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const override;
	virtual void GetChildren(FDetailNodeList& OutChildren) override {}
	virtual void OnItemExpansionChanged( bool bIsExpanded, bool bShouldSaveState) override {}
	virtual bool ShouldBeExpanded() const override { return false; }
	virtual ENodeVisibility GetVisibility() const override { return ENodeVisibility::Visible; }
	virtual void FilterNode( const FDetailFilter& InFilter ) override {}
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override { return false; }
	virtual FName GetNodeName() const override { return NAME_None; }

	virtual EDetailNodeType GetNodeType() const override { return EDetailNodeType::Advanced; }
	virtual TSharedPtr<IPropertyHandle> CreatePropertyHandle() const override { return nullptr; }

	/** Called when the advanced drop down arrow is clicked */
	FReply OnAdvancedDropDownClicked();
private:
	FDetailCategoryImpl& ParentCategory;
	TAttribute<bool> IsEnabled;
	TAttribute<bool> IsExpanded;
	bool bShouldShowAdvancedButton;
	bool bIsTopNode;
	bool bDisplayShowAdvancedMessage;
	bool bShowSplitter;
};
