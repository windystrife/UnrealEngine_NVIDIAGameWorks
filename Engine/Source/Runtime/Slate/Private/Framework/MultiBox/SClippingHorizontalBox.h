// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SBoxPanel.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
class SComboButton;

/** Specialized control for handling the clipping of toolbars and menubars */
class SClippingHorizontalBox : public SHorizontalBox
{
public:
	SLATE_BEGIN_ARGS(SClippingHorizontalBox) 
		: _BackgroundBrush(NULL)
		, _StyleSet(&FCoreStyle::Get())
		, _StyleName(NAME_None)
		{ }

		SLATE_ARGUMENT(const FSlateBrush*, BackgroundBrush)
		SLATE_ARGUMENT(FOnGetContent, OnWrapButtonClicked)
		SLATE_ARGUMENT(const ISlateStyle*, StyleSet)
		SLATE_ARGUMENT(FName, StyleName)
	SLATE_END_ARGS()

	FORCENOINLINE SClippingHorizontalBox() 
	{
		bCanTick = true;
	}

	/** SWidget interface */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	/** SPanel interface */
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	/** Construct this widget */
	void Construct( const FArguments& InArgs );

	/** Adds the wrap button */
	void AddWrapButton();

	/** Returns to index of the first clipped child/block */
	int32 GetClippedIndex() { return ClippedIdx; }

private:
	/** The button that is displayed when a toolbar or menubar is clipped */
	TSharedPtr<SComboButton> WrapButton;

	/** Brush used for drawing the custom border */
	const FSlateBrush* BackgroundBrush;

	/** Callback for when the wrap button is clicked */
	FOnGetContent OnWrapButtonClicked;

	/** Index of the first clipped child/block */
	int32 ClippedIdx;

	/** The style to use */
	const ISlateStyle* StyleSet;
	FName StyleName;
};
