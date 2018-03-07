// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class FPaintArgs;
class FSlateWindowElementList;

class SLATE_API SColorBlock : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS( SColorBlock )
		: _Color( FLinearColor::White )
		, _ColorIsHSV( false )
		, _IgnoreAlpha( false )
		, _ShowBackgroundForAlpha( false )
		, _UseSRGB(true)
		, _OnMouseButtonDown()
		, _Size( FVector2D(16,16) )
		{}

		/** The color to display for this color block */
		SLATE_ATTRIBUTE( FLinearColor, Color )

		/** Whether the color displayed is HSV or not */
		SLATE_ATTRIBUTE( bool, ColorIsHSV )

		/** Whether to ignore alpha entirely from the input color */
		SLATE_ATTRIBUTE( bool, IgnoreAlpha )

		/** Whether to display a background for viewing opacity. Irrelevant if ignoring alpha */
		SLATE_ATTRIBUTE( bool, ShowBackgroundForAlpha )

		/** Whether to display sRGB color */
		SLATE_ATTRIBUTE( bool, UseSRGB )

		/** A handler to activate when the mouse is pressed. */
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )

		/** How big should this color block be? */
		SLATE_ATTRIBUTE( FVector2D, Size )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

private:

	// SWidget overrides

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;

private:

	/** The color to display for this color block */
	TAttribute< FLinearColor > Color;

	/** Whether the color displayed is HSV or not */
	TAttribute< bool > ColorIsHSV;
	
	/** Whether to ignore alpha entirely from the input color */
	TAttribute< bool > IgnoreAlpha;

	/** Whether to display a background for viewing opacity. Irrelevant if ignoring alpha */
	TAttribute< bool > ShowBackgroundForAlpha;

	/** Whether to display sRGB color */
	TAttribute<bool> bUseSRGB;
	
	/** A handler to activate when the mouse is pressed. */
	FPointerEventHandler MouseButtonDownHandler;

	TAttribute<FVector2D> ColorBlockSize;
};
