// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

/**
 * A CompoundWidget is the base from which most non-primitive widgets should be built.
 * CompoundWidgets have a protected member named ChildSlot.
 */
class SLATECORE_API SCompoundWidget : public SWidget
{
public:

	/**
	 * Returns the size scaling factor for this widget.
	 *
	 * @return Size scaling factor.
	 */
	const FVector2D GetContentScale() const
	{
		return ContentScale.Get();
	}

	/**
	 * Sets the content scale for this widget.
	 *
	 * @param InContentScale Content scaling factor.
	 */
	void SetContentScale( const TAttribute< FVector2D >& InContentScale )
	{
		ContentScale = InContentScale;
	}

	/**
	 * Gets the widget's color.
	 */
	FLinearColor GetColorAndOpacity() const
	{
		return ColorAndOpacity.Get();
	}

	/**
	 * Sets the widget's color.
	 *
	 * @param InColorAndOpacity The ColorAndOpacity to be applied to this widget and all its contents.
	 */
	void SetColorAndOpacity( const TAttribute<FLinearColor>& InColorAndOpacity )
	{
		ColorAndOpacity = InColorAndOpacity;
	}

	/**
	 * Sets the widget's foreground color.
	 *
	 * @param InColor The color to set.
	 */
	void SetForegroundColor( const TAttribute<FSlateColor>& InForegroundColor )
	{
		ForegroundColor = InForegroundColor;
	}

public:

	// SWidgetOverrides
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FSlateColor GetForegroundColor() const override;

public:
	virtual void SetVisibility( TAttribute<EVisibility> InVisibility ) override final;

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

protected:

	/** Disallow public construction */
	SCompoundWidget();

	/** The slot that contains this widget's descendants.*/
	FSimpleSlot ChildSlot;

	/** The layout scale to apply to this widget's contents; useful for animation. */
	TAttribute<FVector2D> ContentScale;

	/** The color and opacity to apply to this widget and all its descendants. */
	TAttribute<FLinearColor> ColorAndOpacity;

	/** Optional foreground color that will be inherited by all of this widget's contents */
	TAttribute<FSlateColor> ForegroundColor;
};
