// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Layout/IScrollableWidget.h"
#include "Framework/Views/ITypedTableView.h"
#include "Framework/Layout/InertialScrollManager.h"
#include "Framework/Layout/Overscroll.h"

#include "STableViewBase.generated.h"

class FPaintArgs;
class FSlateWindowElementList;
class ITableRow;
class SHeaderRow;
class SListPanel;
class SScrollBar;
enum class EConsumeMouseWheel : uint8;

/** If the list panel is arranging items horizontally, this enum dictates how the items should be aligned (basically, where any extra space is placed) */
UENUM(BlueprintType)
enum class EListItemAlignment : uint8
{
	/** Items are distributed evenly along the row (any extra space is added as padding between the items) */
	EvenlyDistributed UMETA(DisplayName = "Evenly (Padding)"),

	/** Items are distributed evenly along the row (any extra space is used to scale up the size of the item proportionally.) */
	EvenlySize UMETA(DisplayName = "Evenly (Size)"),

	/** Items are distributed evenly along the row, any extra space is used to scale up width of the items proportionally.) */
	EvenlyWide UMETA(DisplayName = "Evenly (Wide)"),

	/** Items are left aligned on the row (any extra space is added to the right of the items) */
	LeftAligned,

	/** Items are right aligned on the row (any extra space is added to the left of the items) */
	RightAligned,

	/** Items are center aligned on the row (any extra space is halved and added to the left of the items) */
	CenterAligned,

	/** Items are evenly horizontally stretched to distribute any extra space */
	Fill,
};


DECLARE_DELEGATE_OneParam(
	FOnTableViewScrolled,
	double );	/** Scroll offset from the beginning of the list in items */


/**
 * Contains ListView functionality that does not depend on the type of data being observed by the ListView.
 */
class SLATE_API STableViewBase
	: public SCompoundWidget
	, public IScrollableWidget
{
public:

	/** Create the child widgets that comprise the list */
	void ConstructChildren( const TAttribute<float>& InItemWidth, const TAttribute<float>& InItemHeight, const TAttribute<EListItemAlignment>& InItemAlignment, const TSharedPtr<SHeaderRow>& InColumnHeaders, const TSharedPtr<SScrollBar>& InScrollBar, const FOnTableViewScrolled& InOnTableViewScrolled );

	/** Sets the item height */
	void SetItemHeight(TAttribute<float> Height);

	/** Sets the item width */
	void SetItemWidth(TAttribute<float> Width);

	/**
	 * Invoked by the scrollbar when the user scrolls.
	 *
	 * @param InScrollOffsetFraction  The location to which the user scrolled as a fraction (between 0 and 1) of total height of the content.
	 */
	void ScrollBar_OnUserScrolled( float InScrollOffsetFraction );

	/** @return The number of Widgets we currently have generated. */
	int32 GetNumGeneratedChildren() const;

	TSharedPtr<SHeaderRow> GetHeaderRow() const;

	/** @return Returns true if the user is currently interactively scrolling the view by holding
		        the right mouse button and dragging. */
	bool IsRightClickScrolling() const;

	/** @return Returns true if the user is currently interactively scrolling the view by holding
		        either mouse button and dragging. */
	bool IsUserScrolling() const;

	/** Mark the list as dirty, so that it will regenerate its widgets on next tick. */
	void RequestListRefresh();

	/** Return true if there is currently a refresh pending, false otherwise */
	bool IsPendingRefresh() const;

	/** Is this list backing a tree or just a standalone list */
	const ETableViewMode::Type TableViewMode;

	/** Scrolls the view to the top */
	void ScrollToTop();

	/** Scrolls the view to the bottom */
	void ScrollToBottom();

	/** Gets the scroll offset of this view (in items) */
	float GetScrollOffset() const;

	/** Set the scroll offset of this view (in items) */
	void SetScrollOffset( const float InScrollOffset );

	/** Add the scroll offset of this view (in items) */
	void AddScrollOffset(const float InScrollOffsetDelta, bool RefreshList = false);

public:

	// SWidget interface

	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual void OnMouseCaptureLost() override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;

public:

	// IScrollableWidget interface

	virtual FVector2D GetScrollDistance() override;
	virtual FVector2D GetScrollDistanceRemaining() override;
	virtual TSharedRef<class SWidget> GetScrollWidget() override;

protected:

	STableViewBase( ETableViewMode::Type InTableViewMode );

	/**
	 * Scroll the list view by some number of screen units.
	 *
	 * @param MyGeometry      The geometry of the ListView at the time
	 * @param ScrollByAmount  The amount to scroll by in Slate Screen Units.
	 * @param AllowOverscroll Should we allow scrolling past the beginning/end of the list?
	 *
	 * @return The amount actually scrolled in items
	 */
	virtual float ScrollBy( const FGeometry& MyGeometry, float ScrollByAmount, EAllowOverscroll InAllowOverscroll );

	/**
	 * Scroll the view to an offset
	 *
	 * @param InScrollOffset  Offset into the total list length to scroll down.
	 *
	 * @return The amount actually scrolled
	 */
	virtual float ScrollTo( float InScrollOffset );

	/** Insert WidgetToInsert at the top of the view. */
	void InsertWidget( const TSharedRef<ITableRow> & WidgetToInset );

	/** Add a WidgetToAppend to the bottom of the view. */
	void AppendWidget( const TSharedRef<ITableRow>& WidgetToAppend );
	
	/**
	 * Remove all the widgets from the view.
	 */
	void ClearWidgets();

	/**
	 * Get the uniform item width.
	 */
	float GetItemWidth() const;

	/**
	 * Get the uniform item height that is enforced by ListViews.
	 */
	float GetItemHeight() const;

	/**
	* Get the uniform item
	*/
	FVector2D GetItemSize() const;

	/** @return the number of items that can fit on the screen */
	virtual float GetNumLiveWidgets() const;

	/**
	 * Get the number of items that can fit in the view horizontally before creating a new row.
	 * Default is 1, but may be more in subclasses (like STileView)
	 */
	virtual int32 GetNumItemsWide() const;

	/**
	 * Opens a context menu as the result of a right click if OnContextMenuOpening is bound and we are not right click scrolling.
	 */
	virtual void OnRightMouseButtonUp(const FPointerEvent& MouseEvent);
	
	/**
	 * Get the scroll rate in items that best approximates a constant physical scroll rate.
	 */
	float GetScrollRateInItems() const;

	/**
	 * Remove any items that are no longer in the list from the selection set.
	 */
	virtual void UpdateSelectionSet() = 0;


	/** Information about the outcome of the WidgetRegeneratePass */
	struct SLATE_API FReGenerateResults
	{
		FReGenerateResults( double InNewScrollOffset, double InHeightGenerated, double InItemsOnScreen, bool AtEndOfList )
		: NewScrollOffset( InNewScrollOffset )
		, HeightOfGeneratedItems( InHeightGenerated )
		, ExactNumRowsOnScreen( InItemsOnScreen )
		, bGeneratedPastLastItem( AtEndOfList )
		{

		}

		/** The scroll offset that we actually use might not be what the user asked for */
		double NewScrollOffset;

		/** The total height of the widgets that we have generated to represent the visible subset of the items*/
		double HeightOfGeneratedItems;

		/** How many rows are fitting on the screen, including fractions */
		double ExactNumRowsOnScreen;

		/** True when we have generated  */
		bool bGeneratedPastLastItem;
	};
	/**
	 * Update generate Widgets for Items as needed and clean up any Widgets that are no longer needed.
	 * Re-arrange the visible widget order as necessary.
	 */
	virtual FReGenerateResults ReGenerateItems( const FGeometry& MyGeometry ) = 0;

	/** @return how many items there are in the TArray being observed */
	virtual int32 GetNumItemsBeingObserved() const = 0;

	enum class EScrollIntoViewResult
	{
		/** The function scrolled an item (if set) into view (or the item was already in view) */
		Success,
		/** The function did not have enough data to scroll the given item into view, so it should be deferred until the next Tick */
		Deferred,
	};

	/**
	 * If there is a pending request to scroll an item into view, do so.
	 * 
	 * @param ListViewGeometry  The geometry of the listView; can be useful for centering the item.
	 */
	virtual EScrollIntoViewResult ScrollIntoView(const FGeometry& ListViewGeometry) = 0;

	/**
	 * Called when an item has entered the visible geometry to check to see if the ItemScrolledIntoView delegate should be fired.
	 */
	virtual void NotifyItemScrolledIntoView() = 0;

	/** Util Function so templates classes don't need to include SlateApplication */
	void NavigateToWidget(const uint32 UserIndex, const TSharedPtr<SWidget>& NavigationDestination, ENavigationSource NavigationSource = ENavigationSource::FocusedWidget) const;

	/** The panel which holds the visible widgets in this list */
	TSharedPtr< SListPanel > ItemsPanel;

	/** The scroll bar widget */
	TSharedPtr< SScrollBar > ScrollBar;

	/** Delegate to call when the table view is scrolled */
	FOnTableViewScrolled OnTableViewScrolled;

	/** Scroll offset from the beginning of the list in items */
	double ScrollOffset;

	/** Did the user start an interaction in this list? */
	bool bStartedTouchInteraction;

	/** How much we scrolled while the rmb has been held */
	float AmountScrolledWhileRightMouseDown;

	/** The amount we have scrolled this tick cycle */
	float TickScrollDelta;

	/** Information about the widgets we generated during the last regenerate pass */
	FReGenerateResults LastGenerateResults;

	/** Last time we scrolled, did we end up at the end of the list. */
	bool bWasAtEndOfList;

	/** What the list's geometry was the last time a refresh occurred. */
	FGeometry PanelGeometryLastTick;

	/** Delegate to invoke when the context menu should be opening. If it is nullptr, a context menu will not be summoned. */
	FOnContextMenuOpening OnContextMenuOpening;

	/** The selection mode that this tree/list is in. Note that it is up to the generated ITableRows to respect this setting. */
	TAttribute<ESelectionMode::Type> SelectionMode;

	/** Column headers that describe which columns this list shows */
	TSharedPtr< SHeaderRow > HeaderRow;

	/** Helper object to manage inertial scrolling */
	FInertialScrollManager InertialScrollManager;

	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;

	/**	Whether the software cursor should be drawn in the viewport */
	bool bShowSoftwareCursor;

	/** How much to scroll when using mouse wheel */
	float WheelScrollMultiplier;

protected:

	/** Check whether the current state of the table warrants inertial scroll by the specified amount */
	bool CanUseInertialScroll( float ScrollAmount ) const;

	/** Active timer to update the inertial scroll */
	EActiveTimerReturnType UpdateInertialScroll(double InCurrentTime, float InDeltaTime);

	/** One-off active timer to refresh the contents of the table as needed */
	EActiveTimerReturnType EnsureTickToRefresh(double InCurrentTime, float InDeltaTime);

	/** Whether the active timer to update the inertial scrolling is currently registered */
	bool bIsScrollingActiveTimerRegistered;

protected:

	FOverscroll Overscroll;

	/** Whether to permit overscroll on this list view */
	EAllowOverscroll AllowOverscroll;

	/** How we should handle scrolling with the mouse wheel */
	EConsumeMouseWheel ConsumeMouseWheel;

private:
	
	/** When true, a refresh should occur the next tick */
	bool bItemsNeedRefresh;
};
