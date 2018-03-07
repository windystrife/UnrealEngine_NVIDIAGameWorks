// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailAdvancedDropdownNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "SDetailTableRowBase.h"

class SAdvancedDropdownRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS( SAdvancedDropdownRow )
		: _IsExpanded( false )
		, _IsButtonEnabled( true )
		, _ShouldShowAdvancedButton( false )
	{}
		SLATE_ATTRIBUTE( bool, IsExpanded )
		SLATE_ATTRIBUTE( bool, IsButtonEnabled )
		SLATE_ARGUMENT( bool, ShouldShowAdvancedButton )
		SLATE_ARGUMENT( FDetailColumnSizeData, ColumnSizeData )
		SLATE_EVENT( FOnClicked, OnClicked )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, bool bIsTopNode, bool bInDisplayShowAdvancedMessage, bool bShowSplitter )
	{
		IsExpanded = InArgs._IsExpanded;

		bDisplayShowAdvancedMessage = bInDisplayShowAdvancedMessage;

		TSharedPtr<SWidget> ContentWidget;
		if( bIsTopNode )
		{
			ContentWidget =
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryMiddle") )
				.Padding( FMargin( 0.0f, 3.0f, SDetailTableRowBase::ScrollbarPaddingSize, 0.0f ) )
				[
					SNew( SImage )
					.Image( FEditorStyle::GetBrush("DetailsView.AdvancedDropdownBorder.Open") )
				];
		}
		else if( InArgs._ShouldShowAdvancedButton )
		{
			ContentWidget = 
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush("DetailsView.AdvancedDropdownBorder") )
				.Padding( FMargin( 0.0f, 3.0f, SDetailTableRowBase::ScrollbarPaddingSize, 0.0f ) )
				[
					SNew( SVerticalBox )
					+SVerticalBox::Slot()
					.HAlign( HAlign_Center )
					.AutoHeight()
					[
						SNew( STextBlock )
						.Text( NSLOCTEXT("DetailsView", "NoSimpleProperties", "Click the arrow to display advanced properties") )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
						.Visibility( this, &SAdvancedDropdownRow::OnGetHelpTextVisibility )
						.ColorAndOpacity(FLinearColor(1,1,1,.5))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(ExpanderButton, SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.HAlign(HAlign_Center)
						.ContentPadding(2)
						.OnClicked(InArgs._OnClicked)
						.IsEnabled(InArgs._IsButtonEnabled)
						.ToolTipText(this, &SAdvancedDropdownRow::GetAdvancedPulldownToolTipText )
						[
							SNew(SImage)
							.Image(this, &SAdvancedDropdownRow::GetAdvancedPulldownImage)
						]
					]
				];

		}
		else
		{
			TSharedPtr<SWidget> SplitterArea;
			if( bShowSplitter )
			{
				SplitterArea = 
					SNew( SSplitter )
					.PhysicalSplitterHandleSize( 1.0f )
					.HitDetectionSplitterHandleSize( 5.0f )
					.Style( FEditorStyle::Get(), "DetailsView.Splitter" )
					+ SSplitter::Slot()
					.Value( InArgs._ColumnSizeData.LeftColumnWidth )
					.OnSlotResized( SSplitter::FOnSlotResized::CreateSP( this, &SAdvancedDropdownRow::OnLeftColumnResized ) )
					[
						SNew( SHorizontalBox )
						+ SHorizontalBox::Slot()
						.Padding( 3.0f, 0.0f )
						.HAlign( HAlign_Left )
						.VAlign( VAlign_Center )
						.AutoWidth()
						[
							SNew( SExpanderArrow, SharedThis(this) )
						]
						+ SHorizontalBox::Slot()
						.HAlign( HAlign_Left )
						.Padding( FMargin( 0.0f, 2.5f, 2.0f, 2.5f ) )
						[
							SNew( SSpacer )
						]

					]
					+ SSplitter::Slot()
					.Value( InArgs._ColumnSizeData.RightColumnWidth )
					.OnSlotResized( InArgs._ColumnSizeData.OnWidthChanged )
					[
						SNew( SSpacer )
					];
			}
			else
			{
				SplitterArea = SNew(SSpacer);
			}

			ContentWidget =
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryBottom") )
				.Padding( FMargin( 0.0f, 0.0f, SDetailTableRowBase::ScrollbarPaddingSize, 2.0f ) )
				[
					SplitterArea.ToSharedRef()	
				];
		}
		
		ChildSlot
		[
			ContentWidget.ToSharedRef()
		];

		STableRow< TSharedPtr< FDetailTreeNode > >::ConstructInternal(
			STableRow::FArguments()
				.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
				.ShowSelection(false),
			InOwnerTableView
		);	
	}
private:
	void OnLeftColumnResized( float InNewWidth )
	{
		// This has to be bound or the splitter will take it upon itself to determine the size
		// We do nothing here because it is handled by the column size data
	}

	EVisibility OnGetHelpTextVisibility() const
	{
		return bDisplayShowAdvancedMessage && !IsExpanded.Get() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FText GetAdvancedPulldownToolTipText() const
	{
		return IsExpanded.Get() ? NSLOCTEXT("DetailsView", "HideAdvanced", "Hide Advanced") : NSLOCTEXT("DetailsView", "ShowAdvanced", "Show Advanced");
	}

	const FSlateBrush* GetAdvancedPulldownImage() const
	{
		if( ExpanderButton->IsHovered() )
		{
			return IsExpanded.Get() ? FEditorStyle::GetBrush("DetailsView.PulldownArrow.Up.Hovered") : FEditorStyle::GetBrush("DetailsView.PulldownArrow.Down.Hovered");
		}
		else
		{
			return IsExpanded.Get() ? FEditorStyle::GetBrush("DetailsView.PulldownArrow.Up") : FEditorStyle::GetBrush("DetailsView.PulldownArrow.Down");
		}
	}

private:
	TAttribute<bool> IsExpanded;
	TSharedPtr<SButton> ExpanderButton;
	bool bDisplayShowAdvancedMessage;
};


TSharedRef< ITableRow > FAdvancedDropdownNode::GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem)
{
	return 
		SNew( SAdvancedDropdownRow, OwnerTable, bIsTopNode, bDisplayShowAdvancedMessage, bShowSplitter )
		.OnClicked( this, &FAdvancedDropdownNode::OnAdvancedDropDownClicked )
		.IsButtonEnabled( IsEnabled )
		.IsExpanded( IsExpanded )
		.ShouldShowAdvancedButton( bShouldShowAdvancedButton )
		.ColumnSizeData( ColumnSizeData );
}

bool FAdvancedDropdownNode::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	// Not supported
	return false;
}

FReply FAdvancedDropdownNode::OnAdvancedDropDownClicked()
{
	ParentCategory.OnAdvancedDropdownClicked();

	return FReply::Handled();
}
