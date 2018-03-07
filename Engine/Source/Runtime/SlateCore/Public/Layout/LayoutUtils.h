// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Layout/SlateRect.h"
#include "Layout/ArrangedChildren.h"

struct AlignmentArrangeResult
{
	AlignmentArrangeResult( float InOffset, float InSize )
	: Offset(InOffset)
	, Size(InSize)
	{
	}
	
	float Offset;
	float Size;
};

namespace ArrangeUtils
{
	/** Gets the alignment of an axis-agnostic int32 so that we can do aligment on an axis without caring about its orientation */
	template<EOrientation Orientation>
	struct GetChildAlignment
	{
		template<typename SlotType>
		static int32 AsInt( const SlotType& InSlot );
	};

	template<>
	struct GetChildAlignment<Orient_Horizontal>
	{
		template<typename SlotType>
		static int32 AsInt( const SlotType& InSlot )
		{
			return static_cast<int32>(InSlot.HAlignment);
		}
	};

	template<>
	struct GetChildAlignment<Orient_Vertical>
	{
		template<typename SlotType>
		static int32 AsInt( const SlotType& InSlot )
		{
			return static_cast<int32>(InSlot.VAlignment);
		}
	};

}


/**
 * Helper method to BoxPanel::ArrangeChildren.
 * 
 * @param AllottedSize         The size available to arrange the widget along the given orientation
 * @param ChildToArrange       The widget and associated layout information
 * @param SlotPadding          The padding to when aligning the child
 * @param ContentScale         The scale to apply to the child before aligning it.
 * @param bClampToParent       If true the child's size is clamped to the allotted size before alignment occurs, if false, the child's desired size is used, even if larger than the allotted size.
 * 
 * @return  Offset and Size of widget
 */
template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(float AllottedSize, float ChildDesiredSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	const FMargin& Margin = SlotPadding;
	const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
	const float MarginPre = ( Orientation == Orient_Horizontal ) ? Margin.Left : Margin.Top;
	const float MarginPost = ( Orientation == Orient_Horizontal ) ? Margin.Right : Margin.Bottom;

	const int32 Alignment = ArrangeUtils::GetChildAlignment<Orientation>::AsInt(ChildToArrange);

	switch ( Alignment )
	{
	case HAlign_Fill:
		return AlignmentArrangeResult(MarginPre, ( AllottedSize - TotalMargin ) * ContentScale);
	}

	const float ChildSize = bClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize;

	switch( Alignment )
	{
	case HAlign_Left: // same as Align_Top
		return AlignmentArrangeResult(MarginPre, ChildSize);
	case HAlign_Center:
		return AlignmentArrangeResult(( AllottedSize - ChildSize ) / 2.0f + MarginPre - MarginPost, ChildSize);
	case HAlign_Right: // same as Align_Bottom		
		return AlignmentArrangeResult(AllottedSize - ChildSize - MarginPost, ChildSize);
	}

	// Same as Fill
	return AlignmentArrangeResult(MarginPre, ( AllottedSize - TotalMargin ) * ContentScale);
}

template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(float AllottedSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	const FMargin& Margin = SlotPadding;
	const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
	const float MarginPre = ( Orientation == Orient_Horizontal ) ? Margin.Left : Margin.Top;
	const float MarginPost = ( Orientation == Orient_Horizontal ) ? Margin.Right : Margin.Bottom;

	const int32 Alignment = ArrangeUtils::GetChildAlignment<Orientation>::AsInt(ChildToArrange);

	switch ( Alignment )
	{
	case HAlign_Fill:
		return AlignmentArrangeResult(MarginPre, ( AllottedSize - TotalMargin ) * ContentScale);
	}

	const float ChildDesiredSize = ( Orientation == Orient_Horizontal )
		? ( ChildToArrange.GetWidget()->GetDesiredSize().X * ContentScale )
		: ( ChildToArrange.GetWidget()->GetDesiredSize().Y * ContentScale );

	const float ChildSize = bClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize;

	switch ( Alignment )
	{
	case HAlign_Left: // same as Align_Top
		return AlignmentArrangeResult(MarginPre, ChildSize);
	case HAlign_Center:
		return AlignmentArrangeResult(( AllottedSize - ChildSize ) / 2.0f + MarginPre - MarginPost, ChildSize);
	case HAlign_Right: // same as Align_Bottom		
		return AlignmentArrangeResult(AllottedSize - ChildSize - MarginPost, ChildSize);
	}

	// Same as Fill
	return AlignmentArrangeResult(MarginPre, ( AllottedSize - TotalMargin ) * ContentScale);
}


/**
 * Arrange a ChildSlot within the AllottedGeometry and populate ArrangedChildren with the arranged result.
 * The code makes certain assumptions about the type of ChildSlot.
 */
template<typename SlotType>
void ArrangeSingleChild( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SlotType& ChildSlot, const TAttribute<FVector2D>& ContentScale )
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FVector2D ThisContentScale = ContentScale.Get();
		const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
		AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding, ThisContentScale.X);
		AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding, ThisContentScale.Y);

		ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XResult.Offset,YResult.Offset),
				FVector2D(XResult.Size, YResult.Size)
		) );
	}

}

/**
* Given information about a popup and the space available for displaying that popup, compute best placement for it.
*
* @param InAnchor          Area relative to which popup is being created (e.g. the button part of a combo box)
* @param PopupRect         Proposed placement of popup; position may require adjustment.
* @param Orientation       Are we trying to show the popup above/below or left/right relative to the anchor?
* @param RectToFit         The space available for showing this popup; we want to fit entirely within it without clipping.
*
* @return A best position within the RectToFit such that none of the popup clips outside of the RectToFit.
*/
SLATECORE_API FVector2D ComputePopupFitInRect(const FSlateRect& InAnchor, const FSlateRect& PopupRect, const EOrientation Orientation, const FSlateRect RectToFit);
