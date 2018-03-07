// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SHeadingBlock.h"


/**
 * Constructor
 *
 * @param	InHeadingText	Heading text
 */
FHeadingBlock::FHeadingBlock( const FName& InExtensionHook, const TAttribute< FText >& InHeadingText )
	: FMultiBlock( NULL, NULL, InExtensionHook, EMultiBlockType::Heading )
	, HeadingText( InHeadingText )
{
}


/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FHeadingBlock::ConstructWidget() const
{
	return SNew( SHeadingBlock );
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SHeadingBlock::Construct( const FArguments& InArgs )
{
}



/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SHeadingBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedRef< const FHeadingBlock > HeadingBlock = StaticCastSharedRef< const FHeadingBlock >( MultiBlock.ToSharedRef() );

	// Add this widget to the search list of the multibox
	if (MultiBlock->GetSearchable())
		OwnerMultiBoxWidget.Pin()->AddSearchElement(this->AsWidget(), FText::GetEmpty());

	ChildSlot
		.Padding( 2.0f )
		[
			SNew( STextBlock )
				.Text( HeadingBlock->HeadingText )
				.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Heading" ) )
		];
}
