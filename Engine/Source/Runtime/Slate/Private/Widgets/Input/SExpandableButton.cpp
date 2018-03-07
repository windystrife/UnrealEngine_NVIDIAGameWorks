// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SExpandableButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"


//////////////////////////////////////////////////////////////////////////
// SExpandableButton

EVisibility SExpandableButton::GetCollapsedVisibility() const
{
	return IsExpanded.Get() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SExpandableButton::GetExpandedVisibility() const
{
	return IsExpanded.Get() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SExpandableButton::Construct(const FArguments& InArgs)
{
	IsExpanded = InArgs._IsExpanded;

	// Set up the collapsed button content
	TSharedRef<SWidget> CollapsedButtonContent = (InArgs._CollapsedButtonContent.Widget == SNullWidget::NullWidget)
		? StaticCastSharedRef<SWidget>( SNew(STextBlock) .Text( InArgs._CollapsedText ) )
		: InArgs._CollapsedButtonContent.Widget;

	// Set up the expanded button content
	TSharedRef<SWidget> ExpandedButtonContent = (InArgs._ExpandedButtonContent.Widget == SNullWidget::NullWidget)
		? StaticCastSharedRef<SWidget>( SNew(STextBlock) .Text( InArgs._ExpandedText ) )
		: InArgs._ExpandedButtonContent.Widget;
		
	// The child content will be optionally visible depending on the state of the expandable button.
	InArgs._ExpandedChildContent.Widget->SetVisibility( TAttribute<EVisibility>(this, &SExpandableButton::GetExpandedVisibility) );

	SBorder::Construct(SBorder::FArguments()
		.BorderImage( FCoreStyle::Get().GetBrush( "ExpandableButton.Background" ) )
		.Padding( FCoreStyle::Get().GetMargin("ExpandableButton.Padding") )
		[
			SNew(SHorizontalBox)

			// Toggle button (closed)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.Visibility( this, &SExpandableButton::GetCollapsedVisibility )
				.OnClicked( InArgs._OnExpansionClicked )
				.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
				.ContentPadding( 0 )
				[
					CollapsedButtonContent
				]
			]

			// Toggle button (expanded)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Visibility( this, &SExpandableButton::GetExpandedVisibility )
				.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
				.ContentPadding( 0 )
				.VAlign(VAlign_Center)
				[
					ExpandedButtonContent
				]
			]

			// Expansion-only box
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				InArgs._ExpandedChildContent.Widget
			]

			// Right side of expansion arrow
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				// Close expansion button
				SNew(SButton)
				.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
				.ContentPadding( 0 )
				.Visibility( this, &SExpandableButton::GetExpandedVisibility )
				.OnClicked(InArgs._OnCloseClicked)
				[
					SNew(SImage)
					.Image( FCoreStyle::Get().GetBrush("ExpandableButton.CloseButton") )
				]
			]
		]
	);
}
