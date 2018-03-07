// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailCategoryGroupNode.h"

void SDetailCategoryTableRow::Construct( const FArguments& InArgs, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	OwnerTreeNode = InOwnerTreeNode;

	bIsInnerCategory = InArgs._InnerCategory;
	bShowBorder = InArgs._ShowBorder;

	const FDetailColumnSizeData* OptionalSizeData = InArgs._ColumnSizeData;

	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;

	float MyContentTopPadding = 2.0f;
	float MyContentBottomPadding = 2.0f;

	float ChildSlotPadding = 2.0f;
	float BorderVerticalPadding = 3.0f;

	if (OptionalSizeData != nullptr)
	{
		// If we're going to draw a splitter, move any padding from the child slot and the border
		// to the content widget instead

		MyContentTopPadding += ChildSlotPadding + 2 * BorderVerticalPadding;
		MyContentBottomPadding += 2 * BorderVerticalPadding;

		ChildSlotPadding = 0.0f;
		BorderVerticalPadding = 0.0f;
	}

	TSharedPtr<SHorizontalBox> MyContent = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f, MyContentTopPadding, 2.0f, MyContentBottomPadding)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(InArgs._DisplayName)
				.Font(FEditorStyle::GetFontStyle(bIsInnerCategory ? "PropertyWindow.NormalFont" : "DetailsView.CategoryFontStyle"))
				.ShadowOffset(bIsInnerCategory ? FVector2D::ZeroVector : FVector2D(1.0f, 1.0f))
			];

	// If column size data was specified, add a separator
	if (OptionalSizeData != nullptr)
	{
		Widget = SNew(SSplitter)
			.Style(FEditorStyle::Get(), "DetailsView.Splitter")
			.PhysicalSplitterHandleSize(1.0f)
			.HitDetectionSplitterHandleSize(5.0f)

			+ SSplitter::Slot()
			.Value(OptionalSizeData->LeftColumnWidth)
			.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SDetailCategoryTableRow::OnColumnResized))
			[
				MyContent.ToSharedRef()
			]
			
			+SSplitter::Slot()
			.Value(OptionalSizeData->RightColumnWidth)
			.OnSlotResized(OptionalSizeData->OnWidthChanged)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					SNullWidget::NullWidget
				]
			];
	}
	else
	{
		Widget = MyContent;
	}

	ChildSlot
	.Padding( 0.0f, bIsInnerCategory ? 0.0f : ChildSlotPadding, 0.0f, 0.0f )
	[	
		SNew( SBorder )
		.BorderImage( this, &SDetailCategoryTableRow::GetBackgroundImage )
		.Padding( FMargin( 0.0f, BorderVerticalPadding, SDetailTableRowBase::ScrollbarPaddingSize, BorderVerticalPadding ) )
		.BorderBackgroundColor( FLinearColor( .6,.6,.6, 1.0f ) )
		[
			Widget.ToSharedRef()
		]
	];

	if( InArgs._HeaderContent.IsValid() )
	{
		MyContent->AddSlot()
		.VAlign(VAlign_Center)
		[	
			InArgs._HeaderContent.ToSharedRef()
		];
	}

	STableRow< TSharedPtr< FDetailTreeNode > >::ConstructInternal(
		STableRow::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
		InOwnerTableView
	);
}

EVisibility SDetailCategoryTableRow::IsSeparatorVisible() const
{
	return bIsInnerCategory || IsItemExpanded() ? EVisibility::Collapsed : EVisibility::Visible;
}

const FSlateBrush* SDetailCategoryTableRow::GetBackgroundImage() const
{
	if (bShowBorder)
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

	return nullptr;
}

FReply SDetailCategoryTableRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ToggleExpansion();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDetailCategoryTableRow::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown(InMyGeometry, InMouseEvent);
}

void SDetailCategoryTableRow::OnColumnResized(float InNewWidth)
{
	// Intentionally do nothing
}


FDetailCategoryGroupNode::FDetailCategoryGroupNode( const FDetailNodeList& InChildNodes, FName InGroupName, FDetailCategoryImpl& InParentCategory )
	: ChildNodes( InChildNodes )
	, ParentCategory( InParentCategory )
	, GroupName( InGroupName )
	, bShouldBeVisible( false )
	, bShowBorder(true)
	, bHasSplitter(false)
{
}

TSharedRef< ITableRow > FDetailCategoryGroupNode::GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem)
{
	const FDetailColumnSizeData* SizeData = bHasSplitter ? &ColumnSizeData : nullptr;

	return
		SNew( SDetailCategoryTableRow, AsShared(), OwnerTable )
		.DisplayName( FText::FromName(GroupName) )
		.InnerCategory( true )
		.ShowBorder( bShowBorder )
		.ColumnSizeData(SizeData);
}


bool FDetailCategoryGroupNode::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	OutRow.NameContent()
	[
		SNew(STextBlock)
		.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
		.Text(FText::FromName(GroupName))
	];

	return true;
}

void FDetailCategoryGroupNode::GetChildren(FDetailNodeList& OutChildren)
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

void FDetailCategoryGroupNode::FilterNode( const FDetailFilter& InFilter )
{
	bShouldBeVisible = false;
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = ChildNodes[ChildIndex];

		Child->FilterNode( InFilter );

		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			bShouldBeVisible = true;

			ParentCategory.RequestItemExpanded( Child, Child->ShouldBeExpanded() );
		}
	}
}
