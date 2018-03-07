// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailMultiTopLevelObjectRootNode.h"
#include "IDetailRootObjectCustomization.h"
#include "DetailWidgetRow.h"

void SDetailMultiTopLevelObjectTableRow::Construct( const FArguments& InArgs, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<SWidget>& InCustomizedWidgetContents, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	OwnerTreeNode = InOwnerTreeNode;
	bShowExpansionArrow = InArgs._ShowExpansionArrow;

	ChildSlot
	[	
		SNew( SBox )
		.Padding( FMargin( 0.0f, 0.0f, SDetailTableRowBase::ScrollbarPaddingSize, 0.0f ) )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 2.0f, 2.0f, 2.0f)
			.AutoWidth()
			[
				SNew( SExpanderArrow, SharedThis(this) )
				.Visibility(bShowExpansionArrow ? EVisibility::Visible : EVisibility::Collapsed)
			]
			+SHorizontalBox::Slot()
			[
				InCustomizedWidgetContents
			]
		]
	];

	STableRow< TSharedPtr< FDetailTreeNode > >::ConstructInternal(
		STableRow::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
		InOwnerTableView
	);
}


const FSlateBrush* SDetailMultiTopLevelObjectTableRow::GetBackgroundImage() const
{
	if (IsHovered())
	{
		return IsItemExpanded() ? FEditorStyle::GetBrush("DetailsView.CategoryTop_Hovered") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory_Hovered");
	}
	else
	{
		return IsItemExpanded() ? FEditorStyle::GetBrush("DetailsView.CategoryTop") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory");
	}
}

FReply SDetailMultiTopLevelObjectTableRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (bShowExpansionArrow && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ToggleExpansion();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDetailMultiTopLevelObjectTableRow::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown(InMyGeometry, InMouseEvent);
}


FDetailMultiTopLevelObjectRootNode::FDetailMultiTopLevelObjectRootNode( const FDetailNodeList& InChildNodes, const TSharedPtr<IDetailRootObjectCustomization>& InRootObjectCustomization, IDetailsViewPrivate* InDetailsView, const UObject& InRootObject )
	: ChildNodes(InChildNodes)
	, DetailsView(InDetailsView)
	, RootObjectCustomization(InRootObjectCustomization)
	, RootObject(&InRootObject)
	, NodeName(InRootObject.GetFName())
	, bShouldBeVisible(false)
{
}

ENodeVisibility FDetailMultiTopLevelObjectRootNode::GetVisibility() const
{
	ENodeVisibility FinalVisibility = ENodeVisibility::Visible;
	if(RootObjectCustomization.IsValid() && RootObject.IsValid() && !RootObjectCustomization.Pin()->IsObjectVisible(RootObject.Get()))
	{
		FinalVisibility = ENodeVisibility::ForcedHidden;
	}
	else
	{
		FinalVisibility = bShouldBeVisible ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering;
	}

	return FinalVisibility;
}

TSharedRef< ITableRow > FDetailMultiTopLevelObjectRootNode::GenerateWidgetForTableView(const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem)
{
	FDetailWidgetRow Row;
	GenerateStandaloneWidget(Row);

	return SNew(SDetailMultiTopLevelObjectTableRow, AsShared(), Row.NameWidget.Widget, OwnerTable);
}


bool FDetailMultiTopLevelObjectRootNode::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	TSharedPtr<SWidget> HeaderWidget;
	if (RootObjectCustomization.IsValid() && RootObject.IsValid())
	{
		HeaderWidget = RootObjectCustomization.Pin()->CustomizeObjectHeader(RootObject.Get());
	}

	if (!HeaderWidget.IsValid())
	{
		// no customization was supplied or was passed back from the interface as invalid
		// just make a text block with the name
		HeaderWidget =
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
			.Text(FText::FromName(NodeName));
	}

	OutRow.NameContent()
	[
		HeaderWidget.ToSharedRef()
	];

	return true;
}


void FDetailMultiTopLevelObjectRootNode::GetChildren(FDetailNodeList& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = ChildNodes[ChildIndex];
		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			if( Child->ShouldShowOnlyChildren() )
			{
				Child->GetChildren( OutChildren );
			}
			else
			{
				OutChildren.Add( Child );
			}
		}
	}
}

void FDetailMultiTopLevelObjectRootNode::FilterNode( const FDetailFilter& InFilter )
{
	bShouldBeVisible = false;
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = ChildNodes[ChildIndex];

		Child->FilterNode( InFilter );

		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			bShouldBeVisible = true;

			if (DetailsView)
			{
				DetailsView->RequestItemExpanded(Child, Child->ShouldBeExpanded());
			}
		}
	}
}

bool FDetailMultiTopLevelObjectRootNode::ShouldShowOnlyChildren() const
{
	return RootObjectCustomization.IsValid() && RootObject.IsValid() ? !RootObjectCustomization.Pin()->ShouldDisplayHeader(RootObject.Get()) : false;
}
