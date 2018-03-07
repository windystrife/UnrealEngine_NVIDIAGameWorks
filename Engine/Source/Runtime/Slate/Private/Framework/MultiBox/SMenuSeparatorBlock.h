// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/MultiBox/MultiBox.h"

/**
 * Menu separator MultiBlock
 */
class FMenuSeparatorBlock
	: public FMultiBlock
{

public:

	/**
	 * Constructor
	 */
	FMenuSeparatorBlock(const FName& InExtensionHook);


private:

	/** FMultiBlock private interface */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const override;


private:

	// Friend our corresponding widget class
	friend class SMenuSeparatorBlock;

};



/**
 * Menu separator MultiBlock widget
 */
class SLATE_API SMenuSeparatorBlock
	: public SMultiBlockBaseWidget
{

public:

	SLATE_BEGIN_ARGS( SMenuSeparatorBlock ){}

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


private:

};
