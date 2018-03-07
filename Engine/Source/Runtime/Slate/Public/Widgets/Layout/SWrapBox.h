// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;

/**
 * Arranges widgets left-to-right.
 * When the widgets exceed the PreferredWidth
 * the SWrapBox will place widgets on the next line.
 *
 * Illustration:
 *                      +-----Preferred Width
 *                      |
 *       [-----------][-|-]
 *       [--][------[--]|
 *       [--------------|]
 *       [---]          |
 */
class SLATE_API SWrapBox : public SPanel
{
public:

	/** A slot that support alignment of content and padding */
	class FSlot : public TSlotBase<FSlot>, public TSupportsContentAlignmentMixin<FSlot>, public TSupportsContentPaddingMixin<FSlot>
	{
	public:
		FSlot()
			: TSlotBase<FSlot>()
			, TSupportsContentAlignmentMixin<FSlot>(HAlign_Fill, VAlign_Fill)
			, SlotFillLineWhenWidthLessThan()
			, bSlotFillEmptySpace(false)
		{
		}

		/** If the total available space in the wrap panel drops below this threshold, this slot will attempt to fill an entire line. */
		FSlot& FillLineWhenWidthLessThan(TOptional<float> InFillLineWhenWidthLessThan)
		{
			SlotFillLineWhenWidthLessThan = InFillLineWhenWidthLessThan;
			return *( static_cast<FSlot*>( this ) );
		}

		/** Should this slot fill the remaining space on the line? */
		FSlot& FillEmptySpace(bool bInFillEmptySpace)
		{
			bSlotFillEmptySpace = bInFillEmptySpace;
			return *( static_cast<FSlot*>( this ) );
		}

		TOptional<float> SlotFillLineWhenWidthLessThan;
		bool bSlotFillEmptySpace;
	};


	SLATE_BEGIN_ARGS(SWrapBox)
		: _PreferredWidth( 100.f )
		, _InnerSlotPadding( FVector2D::ZeroVector )
		, _UseAllottedWidth( false )
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		/** The slot supported by this panel */
		SLATE_SUPPORTS_SLOT( FSlot )

		/** The preferred width, if not set will fill the space */
		SLATE_ATTRIBUTE( float, PreferredWidth )

		/** The inner slot padding goes between slots sharing borders */
		SLATE_ARGUMENT( FVector2D, InnerSlotPadding )

		/** if true, the PreferredWidth will always match the room available to the SWrapBox  */
		SLATE_ARGUMENT( bool, UseAllottedWidth )
	SLATE_END_ARGS()

	SWrapBox();

	static FSlot& Slot();

	FSlot& AddSlot();

	/** Removes a slot from this box panel which contains the specified SWidget
	 *
	 * @param SlotWidget The widget to match when searching through the slots
	 * @returns The index in the children array where the slot was removed and -1 if no slot was found matching the widget
	 */
	int32 RemoveSlot( const TSharedRef<SWidget>& SlotWidget );

	void Construct( const FArguments& InArgs );

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	void ClearChildren();

	virtual FVector2D ComputeDesiredSize(float) const override;

	virtual FChildren* GetChildren() override;

	/** See InnerSlotPadding Attribute */
	void SetInnerSlotPadding(FVector2D InInnerSlotPadding);

	/** Set the width at which the wrap panel should wrap its content. */
	void SetWrapWidth( const TAttribute<float>& InWrapWidth );

	/** When true, use the WrapWidth property to determine where to wrap to the next line. */
	void SetUseAllottedWidth(bool bInUseAllottedWidth);

private:

	/** How wide this panel should appear to be. Any widgets past this line will be wrapped onto the next line. */
	TAttribute<float> PreferredWidth;

	/** The slots that contain this panel's children. */
	TPanelChildren<FSlot> Slots;

	/** When two slots end up sharing a border, this will put that much padding between then, but otherwise wont. */
	FVector2D InnerSlotPadding;

	/** If true the box will have a preferred width equal to its alloted width  */
	bool bUseAllottedWidth;

	class FChildArranger;
	friend class SWrapBox::FChildArranger;
};

