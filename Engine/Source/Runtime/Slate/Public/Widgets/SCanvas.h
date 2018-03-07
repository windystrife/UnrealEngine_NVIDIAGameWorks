// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

/**
 * Canvas is a layout widget that allows you to arbitrary position and size child widgets in a relative coordinate space
 */
class SLATE_API SCanvas
	: public SPanel
{
public:

	/**
	 * Canvas slots allow child widgets to be positioned and sized
	 */
	class FSlot : public TSlotBase<FSlot>
	{
	public:		
		FSlot& Position( const TAttribute<FVector2D>& InPosition )
		{
			PositionAttr = InPosition;
			return *this;
		}

		FSlot& Size( const TAttribute<FVector2D>& InSize )
		{
			SizeAttr = InSize;
			return *this;
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		/** Position */
		TAttribute<FVector2D> PositionAttr;

		/** Size */
		TAttribute<FVector2D> SizeAttr;

		/** Horizontal Alignment 
		*  Given a top aligned slot, where '+' represents the 
		*  anchor point defined by PositionAttr.
		*  
		*   Left				Center				Right
			+ _ _ _ _            _ _ + _ _          _ _ _ _ +
			|		  |		   | 		   |	  |		    |
			| _ _ _ _ |        | _ _ _ _ _ |	  | _ _ _ _ |
		* 
		*  Note: FILL is NOT supported.
		*/
		EHorizontalAlignment HAlignment;

		/** Vertical Alignment 
		*   Given a left aligned slot, where '+' represents the 
		*   anchor point defined by PositionAttr.
		*  
		*   Top					Center			  Bottom
		*	+_ _ _ _ _          _ _ _ _ _		 _ _ _ _ _ 
		*	|		  |		   | 		 |		|		  |
		*	| _ _ _ _ |        +		 |		|		  |
		*					   | _ _ _ _ |		+ _ _ _ _ |
		* 
		*  Note: FILL is NOT supported.
		*/
		EVerticalAlignment VAlignment;


		/** Default values for a slot. */
		FSlot()
			: TSlotBase<FSlot>()
			, PositionAttr( FVector2D::ZeroVector )
			, SizeAttr( FVector2D( 1.0f, 1.0f ) )
			, HAlignment(HAlign_Left)
			, VAlignment(VAlign_Top)
		{ }
	};

	SLATE_BEGIN_ARGS( SCanvas )
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_SUPPORTS_SLOT( SCanvas::FSlot )

	SLATE_END_ARGS()

	SCanvas();

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
		SCanvas::FSlot& NewSlot = *new FSlot();
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

	// SWidget overrides

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FChildren* GetChildren() override;

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

protected:

	/** The canvas widget's children. */
	TPanelChildren< FSlot > Children;
};
