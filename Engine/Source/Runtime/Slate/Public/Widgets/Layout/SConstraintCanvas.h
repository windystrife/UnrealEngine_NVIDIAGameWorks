// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Margin.h"
#include "Layout/Geometry.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "Widgets/Layout/Anchors.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

/**
 * ConstraintCanvas is a layout widget that allows you to arbitrary position and size child widgets in a 
 * relative coordinate space.  Additionally it permits anchoring widgets.
 */
class SLATE_API SConstraintCanvas : public SPanel
{
public:

	/**
	 * ConstraintCanvas slots allow child widgets to be positioned and sized
	 */
	class FSlot : public TSlotBase<FSlot>
	{
	public:		
		FSlot& Offset( const TAttribute<FMargin>& InOffset )
		{
			OffsetAttr = InOffset;
			return *this;
		}

		FSlot& Anchors( const TAttribute<FAnchors>& InAnchors )
		{
			AnchorsAttr = InAnchors;
			return *this;
		}

		FSlot& Alignment(const TAttribute<FVector2D>& InAlignment)
		{
			AlignmentAttr = InAlignment;
			return *this;
		}

		FSlot& AutoSize(const TAttribute<bool>& InAutoSize)
		{
			AutoSizeAttr = InAutoSize;
			return *this;
		}

		FSlot& ZOrder(const TAttribute<float>& InZOrder)
		{
			ZOrderAttr = InZOrder;
			return *this;
		}

		FSlot& Expose( FSlot*& OutVarToInit )
		{
			OutVarToInit = this;
			return *this;
		}

		/** Offset */
		TAttribute<FMargin> OffsetAttr;

		/** Anchors */
		TAttribute<FAnchors> AnchorsAttr;

		/** Size */
		TAttribute<FVector2D> AlignmentAttr;

		/** Auto-Size */
		TAttribute<bool> AutoSizeAttr;

		/** Z-Order */
		TAttribute<float> ZOrderAttr;

		/** Default values for a slot. */
		FSlot()
			: TSlotBase<FSlot>()
			, OffsetAttr( FMargin( 0, 0, 1, 1 ) )
			, AnchorsAttr( FAnchors( 0.0f, 0.0f ) )
			, AlignmentAttr( FVector2D( 0.5f, 0.5f ) )
			, AutoSizeAttr( false )
			, ZOrderAttr( 0 )
		{ }
	};

	SLATE_BEGIN_ARGS( SConstraintCanvas )
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_SUPPORTS_SLOT( SConstraintCanvas::FSlot )

	SLATE_END_ARGS()

	SConstraintCanvas();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	static FSlot& Slot()
	{
		return *(new FSlot());
	}

	/**
	 * Adds a content slot.
	 *
	 * @return The added slot.
	 */
	FSlot& AddSlot()
	{
		Invalidate(EInvalidateWidget::Layout);

		SConstraintCanvas::FSlot& NewSlot = *(new FSlot());
		this->Children.Add( &NewSlot );
		return NewSlot;
	}

	/**
	 * Removes a particular content slot.
	 *
	 * @param SlotWidget The widget in the slot to remove.
	 */
	int32 RemoveSlot( const TSharedRef<SWidget>& SlotWidget );

	/**
	 * Removes all slots from the panel.
	 */
	void ClearChildren();

public:

	// Begin SWidget overrides
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FChildren* GetChildren() override;
	// End SWidget overrides

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

private:

	/** An array matching the length and order of ArrangedChildren. True means the child must be placed in a layer in front of all previous children. */
	typedef TArray<bool, TInlineAllocator<16>> FArrangedChildLayers;

	/** Like ArrangeChildren but also generates an array of layering information (see FArrangedChildLayers). */
	void ArrangeLayeredChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, FArrangedChildLayers& ArrangedChildLayers) const;

protected:

	/** The ConstraintCanvas widget's children. */
	TPanelChildren< FSlot > Children;
};
