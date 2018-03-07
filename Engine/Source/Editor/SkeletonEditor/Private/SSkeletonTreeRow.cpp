// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSkeletonTreeRow.h"
#include "ISkeletonTree.h"
#include "Preferences/PersonaOptions.h"

void SSkeletonTreeRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	OnDraggingItem = InArgs._OnDraggingItem;
	FilterText = InArgs._FilterText;

	check( Item.IsValid() );

	SMultiColumnTableRow< TSharedPtr<ISkeletonTreeItem> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

void SSkeletonTreeRow::ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent)
{
	STableRow<TSharedPtr<ISkeletonTreeItem>>::Content = InContent;

	TSharedRef<SWidget> InlineEditWidget = Item.Pin()->GenerateInlineEditWidget(FilterText, FIsSelected::CreateSP(this, &STableRow::IsSelected));

	// MultiColumnRows let the user decide which column should contain the expander/indenter item.
	this->ChildSlot
		.Padding(InPadding)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				InContent
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				InlineEditWidget
			]
		];
}

TSharedRef< SWidget > SSkeletonTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ISkeletonTree::Columns::Name )
	{
		TSharedPtr< SHorizontalBox > RowBox;

		SAssignNew( RowBox, SHorizontalBox )
			.Visibility_Lambda([this]()
			{
				return Item.Pin()->GetFilterResult() == ESkeletonTreeFilterResult::ShownDescendant && GetMutableDefault<UPersonaOptions>()->bHideParentsWhenFiltering ? EVisibility::Collapsed : EVisibility::Visible;
			});

		RowBox->AddSlot()
			.AutoWidth()
			[
				SNew( SExpanderArrow, SharedThis(this) )
			];

		Item.Pin()->GenerateWidgetForNameColumn( RowBox, FilterText, FIsSelected::CreateSP(this, &STableRow::IsSelectedExclusively ) );

		return RowBox.ToSharedRef();
	}
	else
	{
		return Item.Pin()->GenerateWidgetForDataColumn(ColumnName);
	}
}

void SSkeletonTreeRow::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	Item.Pin()->HandleDragEnter(DragDropEvent);
}

void SSkeletonTreeRow::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	Item.Pin()->HandleDragLeave(DragDropEvent);
}

FReply SSkeletonTreeRow::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return Item.Pin()->HandleDrop(DragDropEvent);
}

FReply SSkeletonTreeRow::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( OnDraggingItem.IsBound() )
	{
		return OnDraggingItem.Execute( MyGeometry, MouseEvent );
	}
	else
	{
		return FReply::Unhandled();
	}
}

int32 SSkeletonTreeRow::DoesItemHaveChildren() const
{
	if(Item.Pin()->HasInlineEditor())
	{
		return 1;
	}

	return SMultiColumnTableRow<TSharedPtr<ISkeletonTreeItem>>::DoesItemHaveChildren();
}

bool SSkeletonTreeRow::IsItemExpanded() const
{
	return SMultiColumnTableRow<TSharedPtr<ISkeletonTreeItem>>::IsItemExpanded() || Item.Pin()->IsInlineEditorExpanded();
}

void SSkeletonTreeRow::ToggleExpansion()
{
	SMultiColumnTableRow<TSharedPtr<ISkeletonTreeItem>>::ToggleExpansion();
	
	if (Item.Pin()->HasInlineEditor())
	{
		Item.Pin()->ToggleInlineEditorExpansion();
		OwnerTablePtr.Pin()->Private_SetItemExpansion(Item.Pin(), Item.Pin()->IsInlineEditorExpanded());
	}
}
