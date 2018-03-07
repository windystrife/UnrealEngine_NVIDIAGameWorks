// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/MultiBox/MultiBox.h"

/**
 * Arbitrary Widget MultiBlock
 */
class FWidgetBlock
	: public FMultiBlock
{

public:

	/**
	 * Constructor
	 *
	 * @param	InContent	The widget to place in the block
	 * @param	InLabel		Optional label text to be added to the left of the content
	 * @param	bInNoIndent	If true, removes the padding from the left of the widget that lines it up with other menu items
	 */
	FWidgetBlock( TSharedRef<SWidget> InContent, const FText& InLabel, bool bInNoIndent );

	/** FMultiBlock interface */
	virtual void CreateMenuEntry(class FMenuBuilder& MenuBuilder) const override;


private:

	/** FMultiBlock private interface */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const override;


private:

	// Friend our corresponding widget class
	friend class SWidgetBlock;

	/** Content widget */
	TSharedRef<SWidget> ContentWidget;

	/** Optional label text */
	FText Label;

	/** Remove the padding from the left of the widget that lines it up with other menu items? */
	bool bNoIndent;
};




/**
 * Arbitrary Widget MultiBlock widget
 */
class SLATE_API SWidgetBlock
	: public SMultiBlockBaseWidget
{

public:

	SLATE_BEGIN_ARGS( SWidgetBlock ){}
	SLATE_END_ARGS()


	/**
	 * Builds this MultiBlock widget up from the MultiBlock associated with it
	 */
	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) override;

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};
