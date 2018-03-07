// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/SlateWidgetStyleAsset.h"

class SScrollBarTrack;
class SSpacer;

DECLARE_DELEGATE_OneParam(
	FOnUserScrolled,
	float );	/** ScrollOffset as a fraction between 0 and 1 */


class SScrollBarTrack;

class SLATE_API SScrollBar : public SBorder
{
public:

	SLATE_BEGIN_ARGS( SScrollBar )
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar") )
		, _OnUserScrolled()
		, _AlwaysShowScrollbar(false)
#if PLATFORM_UI_HAS_MOBILE_SCROLLBARS
		, _HideWhenNotInUse(true)
#else
		, _HideWhenNotInUse(false)
#endif
		, _Orientation( Orient_Vertical )
		, _Thickness( FVector2D(12.0f, 12.0f) )
		{}

		/** The style to use for this scrollbar */
		SLATE_STYLE_ARGUMENT( FScrollBarStyle, Style )
		SLATE_EVENT( FOnUserScrolled, OnUserScrolled )
		SLATE_ARGUMENT( bool, AlwaysShowScrollbar )
		SLATE_ARGUMENT( bool, HideWhenNotInUse )
		SLATE_ARGUMENT( EOrientation, Orientation )
		/** The thickness of the scrollbar thumb */
		SLATE_ATTRIBUTE( FVector2D, Thickness )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Set the handler to be invoked when the user scrolls.
	 *
	 * @param InHandler   Method to execute when the user scrolls the scrollbar
	 */
	void SetOnUserScrolled( const FOnUserScrolled& InHandler );

	/**
	 * Set the offset and size of the track's thumb.
	 * Note that the maximum offset is 1.0-ThumbSizeFraction.
	 * If the user can view 1/3 of the items in a single page, the maximum offset will be ~0.667f
	 *
	 * @param InOffsetFraction     Offset of the thumbnail from the top as a fraction of the total available scroll space.
	 * @param InThumbSizeFraction  Size of thumbnail as a fraction of the total available scroll space.
	 */
	void SetState( float InOffsetFraction, float InThumbSizeFraction );

	// SWidget
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// SWidget

	/** @return true if scrolling is possible; false if the view is big enough to fit all the content */
	bool IsNeeded() const;

	/** @return normalized distance from top */
	float DistanceFromTop() const;

	/** @return normalized distance from bottom */
	float DistanceFromBottom() const;

	/** @return the scrollbar's visibility as a product of internal rules and user-specified visibility */
	EVisibility ShouldBeVisible() const;

	/** @return True if the user is scrolling by dragging the scroll bar thumb. */
	bool IsScrolling() const;

	/** @return the orientation in which the scrollbar is scrolling. */
	EOrientation GetOrientation() const; 

	/** See argument Style */
	void SetStyle(const FScrollBarStyle* InStyle);

	/** See UserVisibility attribute */
	void SetUserVisibility(TAttribute<EVisibility> InUserVisibility) { UserVisibility = InUserVisibility; }

	/** See Thickness attribute */
	void SetThickness(TAttribute<FVector2D> InThickness);

	/** See ScrollBarAlwaysVisible attribute */
	void SetScrollBarAlwaysVisible(bool InAlwaysVisible);

	/** Allows external scrolling panels to notify the scrollbar when scrolling begins. */
	virtual void BeginScrolling();

	/** Allows external scrolling panels to notify the scrollbar when scrolling ends. */
	virtual void EndScrolling();

	SScrollBar();

protected:
	
	/** Execute the on user scrolled delegate */
	void ExecuteOnUserScrolled( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/** We fade the scroll track unless it is being hovered*/
	FSlateColor GetTrackOpacity() const;

	/** We always show a subtle scroll thumb, but highlight it extra when dragging */
	FLinearColor GetThumbOpacity() const;

	/** @return the name of an image for the scrollbar thumb based on whether the user is dragging or hovering it. */
	const FSlateBrush* GetDragThumbImage() const;

	/** The scrollbar's visibility as specified by the user. Will be compounded with internal visibility rules. */
	TAttribute<EVisibility> UserVisibility;

	TSharedPtr<SBorder> DragThumb;
	TSharedPtr<SSpacer> ThicknessSpacer;
	bool bDraggingThumb;
	TSharedPtr<SScrollBarTrack> Track;
	FOnUserScrolled OnUserScrolled;
	float DragGrabOffset;
	EOrientation Orientation;
	bool bHideWhenNotInUse;
	bool bIsScrolling;
	double LastInteractionTime;

	/** Image to use when the scrollbar thumb is in its normal state */
	const FSlateBrush* NormalThumbImage;
	/** Image to use when the scrollbar thumb is in its hovered state */
	const FSlateBrush* HoveredThumbImage;
	/** Image to use when the scrollbar thumb is in its dragged state */
	const FSlateBrush* DraggedThumbImage;
	/** Background brush */
	const FSlateBrush* BackgroundBrush;
	/** Top brush */
	const FSlateBrush* TopBrush;
	/** Bottom brush */
	const FSlateBrush* BottomBrush;
};
