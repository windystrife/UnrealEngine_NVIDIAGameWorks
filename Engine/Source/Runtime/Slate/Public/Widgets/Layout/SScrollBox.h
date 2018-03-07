// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "SlotBase.h"
#include "Layout/Geometry.h"
#include "Styling/SlateColor.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/NavigationReply.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/InertialScrollManager.h"
#include "Framework/Layout/Overscroll.h"

class FPaintArgs;
class FSlateWindowElementList;
class SScrollPanel;

/** Where to scroll the descendant to */
UENUM(BlueprintType)
enum class EDescendantScrollDestination : uint8
{
	/**
	* Scroll the widget into view using the least amount of energy possible.  So if the new item
	* is above the visible set, it will stop as soon as it's in view at the top.  If it's below the
	* visible set, it stop it comes into view at the bottom.
	*/
	IntoView,

	/** Always scroll the widget so it appears at the top/Left of the scrollable area. */
	TopOrLeft,

	/**
	* Always scroll the widget so it appears at the center of the scrollable area, if possible.
	* This won't be possible for the first few items and the last few items, as there's not enough
	* slack.
	*/
	Center,
};

/** SScrollBox can scroll through an arbitrary number of widgets. */
class SLATE_API SScrollBox : public SCompoundWidget
{
public:
	/** A Slot that provides layout options for the contents of a scrollable box. */
	class SLATE_API FSlot : public TSlotBase<FSlot>, public TSupportsContentPaddingMixin<FSlot>
	{
	public:
		FSlot()
			: TSlotBase<FSlot>()
			// Set both vertical and horizontal alignment to fill by default.  During layout, the
			// alignment direction parallel to the scroll direction is assumed to be top, or left
			// since that is how the items are stacked.
			, HAlignment(HAlign_Fill)
			, VAlignment(VAlign_Fill)
		{
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign(EVerticalAlignment InVAlignment)
		{
			VAlignment = InVAlignment;
			return *this;
		}
		
		EHorizontalAlignment HAlignment;
		EVerticalAlignment VAlignment;
	};

	SLATE_BEGIN_ARGS(SScrollBox)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox") )
		, _ScrollBarStyle( &FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar") )
		, _ExternalScrollbar()
		, _Orientation(Orient_Vertical)
		, _ScrollBarVisibility(EVisibility::Visible)
		, _ScrollBarAlwaysVisible(false)
		, _ScrollBarThickness(FVector2D(5, 5))
		, _AllowOverscroll(EAllowOverscroll::Yes)
		, _NavigationDestination(EDescendantScrollDestination::IntoView)
		, _NavigationScrollPadding(0.0f)
		, _OnUserScrolled()
		, _ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		{
			_Clipping = EWidgetClipping::ClipToBounds;
		}
		
		SLATE_SUPPORTS_SLOT( FSlot )

		/** Style used to draw this scrollbox */
		SLATE_STYLE_ARGUMENT( FScrollBoxStyle, Style )

		/** Style used to draw this scrollbox's scrollbar */
		SLATE_STYLE_ARGUMENT( FScrollBarStyle, ScrollBarStyle )

		/** Custom scroll bar */
		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		/** The direction that children will be stacked, and also the direction the box will scroll. */
		SLATE_ARGUMENT( EOrientation, Orientation )

		SLATE_ARGUMENT( EVisibility, ScrollBarVisibility )

		SLATE_ARGUMENT( bool, ScrollBarAlwaysVisible )

		SLATE_ARGUMENT( FVector2D, ScrollBarThickness )

		SLATE_ARGUMENT(EAllowOverscroll, AllowOverscroll);

		SLATE_ARGUMENT(EDescendantScrollDestination, NavigationDestination);

		/**
		 * The amount of padding to ensure exists between the item being navigated to, at the edge of the
		 * scrollbox.  Use this if you want to ensure there's a preview of the next item the user could scroll to.
		 */
		SLATE_ARGUMENT(float, NavigationScrollPadding);

		/** Called when the button is clicked */
		SLATE_EVENT(FOnUserScrolled, OnUserScrolled)

		SLATE_ARGUMENT(EConsumeMouseWheel, ConsumeMouseWheel);

	SLATE_END_ARGS()

	SScrollBox();

	/** @return a new slot. Slots contain children for SScrollBox */
	static FSlot& Slot();

	void Construct( const FArguments& InArgs );

	/** Adds a slot to SScrollBox */
	SScrollBox::FSlot& AddSlot();

	/** Removes a slot at the specified location */
	void RemoveSlot( const TSharedRef<SWidget>& WidgetToRemove );

	/** Removes all children from the box */
	void ClearChildren();

	/** @return Returns true if the user is currently interactively scrolling the view by holding
		        the right mouse button and dragging. */
	bool IsRightClickScrolling() const;

	EAllowOverscroll GetAllowOverscroll() const;

	void SetAllowOverscroll( EAllowOverscroll NewAllowOverscroll );

	float GetScrollOffset() const;

	void SetScrollOffset( float NewScrollOffset );

	void ScrollToStart();

	void ScrollToEnd();

	/** 
	 * Attempt to scroll a widget into view, will safely handle non-descendant widgets 
	 *
	 * @param WidgetToFind The widget whose geometry we wish to discover.
	 * @param InAnimateScroll	Whether or not to animate the scroll
	 * @param InDestination		Where we want the child widget to stop.
	 */
	void ScrollDescendantIntoView(const TSharedPtr<SWidget>& WidgetToFind, bool InAnimateScroll = true, EDescendantScrollDestination InDestination = EDescendantScrollDestination::IntoView, float Padding = 0);

	/** Get the current orientation of the scrollbox. */
	EOrientation GetOrientation();

	/** Sets the current orientation of the scrollbox and updates the layout */
	void SetOrientation(EOrientation InOrientation);

	void SetScrollBarVisibility(EVisibility InVisibility);

	void SetScrollBarAlwaysVisible(bool InAlwaysVisible);

	void SetScrollBarThickness(FVector2D InThickness);

	void SetScrollBarRightClickDragAllowed(bool bIsAllowed);
public:

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool ComputeVolatility() const override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual void OnMouseCaptureLost() override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override;
	// End of SWidget interface

protected:
	void OnClippingChanged();

private:

	/** Builds a default Scrollbar */
	TSharedPtr<SScrollBar> ConstructScrollBar();

	/** Constructs internal layout widgets for scrolling vertically using the existing ScrollPanel and ScrollBar. */
	void ConstructVerticalLayout();

	/** Constructs internal layout widgets for scrolling horizontally using the existing ScrollPanel and ScrollBar. */
	void ConstructHorizontalLayout();

	/** Gets the component of a vector in the direction of scrolling based on the Orientation property. */
	FORCEINLINE float GetScrollComponentFromVector(FVector2D Vector) const
	{
		return Orientation == Orient_Vertical ? Vector.Y : Vector.X;
	}

	/** Sets the component of a vector in the direction of scrolling based on the Orientation property. */
	inline void SetScrollComponentOnVector(FVector2D& InVector, float Value) const
	{
		if (Orientation == Orient_Vertical)
		{
			InVector.Y = Value;
		}
		else
		{
			InVector.X = Value;
		}
	}

	/** Scroll offset that the user asked for. We will clamp it before actually scrolling there. */
	float DesiredScrollOffset;

	/** Scrolls or begins scrolling a widget into view, only valid to call when we have layout geometry. */
	bool InternalScrollDescendantIntoView(const FGeometry& MyGeometry, const TSharedPtr<SWidget>& WidgetToFind, bool InAnimateScroll = true, EDescendantScrollDestination InDestination = EDescendantScrollDestination::IntoView, float Padding = 0);

	/**
	 * Scroll the view by ScrollAmount given its currently AllottedGeometry.
	 *
	 * @param AllottedGeometry  The geometry allotted for this SScrollBox by the parent
	 * @param ScrollAmount      
	 * @param InAnimateScroll	Whether or not to animate the scroll
	 * @return Whether or not the scroll was fully handled
	 */
	bool ScrollBy(const FGeometry& AllottedGeometry, float LocalScrollAmount, EAllowOverscroll Overscroll, bool InAnimateScroll);

	/** Invoked when the user scroll via the scrollbar */
	void ScrollBar_OnUserScrolled( float InScrollOffsetFraction );

	/** Does the user need a hint that they can scroll to the start of the list? */
	FSlateColor GetStartShadowOpacity() const;
	
	/** Does the user need a hint that they can scroll to the end of the list? */
	FSlateColor GetEndShadowOpacity() const;

	/** Active timer to update inertial scrolling as needed */
	EActiveTimerReturnType UpdateInertialScroll(double InCurrentTime, float InDeltaTime);

	/** Check whether the current state of the table warrants inertial scroll by the specified amount */
	bool CanUseInertialScroll(float ScrollAmount) const;

	void BeginInertialScrolling();

	void EndInertialScrolling();

private:

	/** The panel which stacks the child slots */
	TSharedPtr<class SScrollPanel> ScrollPanel;

	/** The scrollbar which controls scrolling for the scrollbox. */
	TSharedPtr<SScrollBar> ScrollBar;

	/** The amount we have scrolled this tick cycle */
	float TickScrollDelta;

	/** Did the user start an interaction in this list? */
	TOptional<int32> bFingerOwningTouchInteraction;

	/** How much we scrolled while the rmb has been held */
	float AmountScrolledWhileRightMouseDown;

	/** The current deviation we've accumulated on scrol, once it passes the trigger amount, we're going to begin scrolling. */
	float PendingScrollTriggerAmount;

	/** Helper object to manage inertial scrolling */
	FInertialScrollManager InertialScrollManager;

	/** The overscroll state management structure. */
	FOverscroll Overscroll;

	/** Whether to permit overscroll on this scroll box */
	EAllowOverscroll AllowOverscroll;

	/**
	 * The amount of padding to ensure exists between the item being navigated to, at the edge of the
	 * scrollbox.  Use this if you want to ensure there's a preview of the next item the user could scroll to.
	 */
	float NavigationScrollPadding;

	/**
	 * 
	 */
	EDescendantScrollDestination NavigationDestination;

	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;

	/** Fired when the user scrolls the scrollbox */
	FOnUserScrolled OnUserScrolled;

	/** The scrolling and stacking orientation. */
	EOrientation Orientation;

	/** Style resource for the scrollbox */
	const FScrollBoxStyle* Style;

	/** Style resource for the scrollbar */
	const FScrollBarStyle* ScrollBarStyle;

	/** How we should handle scrolling with the mouse wheel */
	EConsumeMouseWheel ConsumeMouseWheel;

	/** Cached geometry for use with the active timer */
	FGeometry CachedGeometry;

	/** Scroll into view request. */
	TFunction<void(FGeometry)> ScrollIntoViewRequest;

	TSharedPtr<FActiveTimerHandle> UpdateInertialScrollHandle;

	double LastScrollTime;

	/**	Whether the software cursor should be drawn in the viewport */
	bool bShowSoftwareCursor : 1;

	/** Whether or not the user supplied an external scrollbar to control scrolling. */
	bool bScrollBarIsExternal : 1;

	/** Are we actively scrolling right now */
	bool bIsScrolling : 1;

	/** Should the current scrolling be animated or immediately jump to the desired scroll offer */
	bool bAnimateScroll : 1;

	/** If true, will scroll to the end next Tick */
	bool bScrollToEnd : 1;

	/** Whether the active timer to update the inertial scroll is registered */
	bool bIsScrollingActiveTimerRegistered : 1;

	bool bAllowsRightClickDragScrolling : 1;
	
	bool bTouchPanningCapture : 1;
};
