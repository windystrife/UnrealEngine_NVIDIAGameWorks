// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

/**
 * Implements an overlay widget.
 *
 * Overlay widgets allow for layering several widgets on top of each other.
 * Each slot of an overlay represents a layer that can contain one widget.
 * The slots will be rendered on top of each other in the order they are declared in code.
 *
 * Usage:
 *		SNew(SOverlay)
 *		+ SOverlay::Slot(SNew(SMyWidget1))
 *		+ SOverlay::Slot(SNew(SMyWidget2))
 *		+ SOverlay::Slot(SNew(SMyWidget3))
 *
 *		Note that SWidget3 will be drawn on top of SWidget2 and SWidget1.
 */
class SLATECORE_API SOverlay
	: public SPanel
{
public:	

	/** A slot that support alignment of content and padding and z-order */
	class SLATECORE_API FOverlaySlot : public TSlotBase<FOverlaySlot>
	{
	public:
		FOverlaySlot()
			: TSlotBase<FOverlaySlot>()
			, ZOrder(0)
			, HAlignment(HAlign_Fill)
			, VAlignment(VAlign_Fill)
			, SlotPadding(0.0f)
		{ }

		FOverlaySlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FOverlaySlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FOverlaySlot& Padding(float Uniform)
		{
			SlotPadding = FMargin(Uniform);
			return *this;
		}

		FOverlaySlot& Padding(float Horizontal, float Vertical)
		{
			SlotPadding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FOverlaySlot& Padding(float Left, float Top, float Right, float Bottom)
		{
			SlotPadding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}

		FOverlaySlot& Padding( const TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}

		/** Slots with larger ZOrder values will draw above slots with smaller ZOrder values.  Slots
		with the same ZOrder will simply draw in the order they were added.  Currently this only
		works for overlay slots that are added dynamically with AddWidget() and RemoveWidget() */
		int32 ZOrder;

		TEnumAsByte<EHorizontalAlignment> HAlignment;
		TEnumAsByte<EVerticalAlignment> VAlignment;

		TAttribute< FMargin > SlotPadding;
	};


	SLATE_BEGIN_ARGS( SOverlay )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		SLATE_SUPPORTS_SLOT( SOverlay::FOverlaySlot )

	SLATE_END_ARGS()

	SOverlay();

	/**
	 * Construct this widget.
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Returns the number of child widgets */
	int32 GetNumWidgets() const;

	/**
	 * Removes a widget from this overlay.
	 *
	 * @param	Widget	The widget content to remove
	 */
	bool RemoveSlot( TSharedRef< SWidget > Widget );

	/** Adds a slot at the specified location (ignores Z-order) */
	FOverlaySlot& AddSlot(int32 ZOrder=INDEX_NONE);

	/** Removes a slot at the specified location */
	void RemoveSlot(int32 ZOrder=INDEX_NONE);

	/** Removes all children from the overlay */
	void ClearChildren();

	/** @return a new slot. Slots contain children for SOverlay */
	static FOverlaySlot& Slot()
	{
		return *(new FOverlaySlot());
	}

	// SWidget interface
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	// End of SWidget interface

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

protected:
	/** The SOverlay's slots; each slot contains a child widget. */
	TPanelChildren<FOverlaySlot> Children;
};
