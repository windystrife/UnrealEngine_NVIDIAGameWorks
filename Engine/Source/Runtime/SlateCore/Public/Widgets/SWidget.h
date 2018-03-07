// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Stats/Stats.h"
#include "Styling/SlateColor.h"
#include "Layout/SlateRect.h"
#include "Layout/Visibility.h"
#include "Layout/Clipping.h"
#include "Layout/Geometry.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/LayoutGeometry.h"
#include "Layout/Margin.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/NavigationReply.h"
#include "Input/PopupMethodReply.h"
#include "Types/ISlateMetaData.h"
#include "Types/WidgetActiveTimerDelegate.h"

class FActiveTimerHandle;
class FArrangedChildren;
class FCachedWidgetNode;
class FChildren;
class FPaintArgs;
class FSlateWindowElementList;
class FSlotBase;
class FWeakWidgetPath;
class FWidgetPath;
class IToolTip;
class SWidget;
struct FSlateBrush;

/** Delegate type for handling mouse events */
DECLARE_DELEGATE_RetVal_TwoParams(
	FReply, FPointerEventHandler,
	/** The geometry of the widget*/
	const FGeometry&,
	/** The Mouse Event that we are processing */
	const FPointerEvent&)

DECLARE_DELEGATE_TwoParams(
	FNoReplyPointerEventHandler,
	/** The geometry of the widget*/
	const FGeometry&,
	/** The Mouse Event that we are processing */
	const FPointerEvent&)

DECLARE_DELEGATE_OneParam(
	FSimpleNoReplyPointerEventHandler,
	/** The Mouse Event that we are processing */
	const FPointerEvent&)

enum class EPopupMethod : uint8;

namespace SharedPointerInternals
{
	template <typename ObjectType>
	class TIntrusiveReferenceController;
}

class SLATECORE_API FSlateControlledConstruction
{
public:
	FSlateControlledConstruction(){}
	virtual ~FSlateControlledConstruction(){}
	
private:
	/** UI objects cannot be copy-constructed */
	FSlateControlledConstruction(const FSlateControlledConstruction& Other) = delete;
	
	/** UI objects cannot be copied. */
	void operator= (const FSlateControlledConstruction& Other) = delete;

	/** Widgets should only ever be constructed via SNew or SAssignNew */
	void* operator new ( const size_t InSize )
	{
		return FMemory::Malloc(InSize);
	}

	/** Widgets should only ever be constructed via SNew or SAssignNew */
	void* operator new ( const size_t InSize, void* Addr )
	{
		return Addr;
	}

	template<class WidgetType, bool bIsUserWidget>
	friend struct TWidgetAllocator;

	template <typename ObjectType>
	friend class SharedPointerInternals::TIntrusiveReferenceController;

public:
	void operator delete(void* mem)
	{
		FMemory::Free(mem);
	}
};


/**
 * The different types of invalidation that are possible for a widget.
 */
enum class EInvalidateWidget
{
	/**
	 * Use Layout invalidation if you're changing a normal property involving painting or sizing.
	 */
	Layout,
	/**
	 * Use Layout invalidation if you're changing a normal property involving painting or sizing.
	 * Additionally if the property that was changed affects Volatility in anyway, it's important
	 * that you invalidate volatility so that it can be recalculated and cached.
	 */
	LayoutAndVolatility
};



/**
 * An ILayoutCache implementor is responsible for caching a the hierarchy of widgets it is drawing.
 * The shipped implementation of this is SInvalidationPanel.
 */
class ILayoutCache
{
public:
	virtual ~ILayoutCache() { }
	virtual void InvalidateWidget(class SWidget* InvalidateWidget) = 0;
	virtual FCachedWidgetNode* CreateCacheNode() const = 0;
};


/**
 * An FPopupLayer hosts the pop-up content which could be anything you want to appear on top of a widget.
 * The widget must understand how to host pop-ups to make use of this.
 */
class FPopupLayer : public TSharedFromThis<FPopupLayer>
{
public:
	FPopupLayer(const TSharedRef<SWidget>& InitHostWidget, const TSharedRef<SWidget>& InitPopupContent)
		: HostWidget(InitHostWidget)
		, PopupContent(InitPopupContent)
	{
	}

	virtual ~FPopupLayer() { }
	
	virtual TSharedRef<SWidget> GetHost() { return HostWidget; }
	virtual TSharedRef<SWidget> GetContent() { return PopupContent; }
	virtual FSlateRect GetAbsoluteClientRect() = 0;

	virtual void Remove() = 0;

private:
	TSharedRef<SWidget> HostWidget;
	TSharedRef<SWidget> PopupContent;
};


class IToolTip;

/**
 * HOW TO DEPRECATE SLATE_ATTRIBUTES
 * 
 * SLATE_ATTRIBUTE(ECheckBoxState, IsChecked)
 *
 * DEPRECATED(4.xx, "Please use IsChecked(TAttribute<ECheckBoxState>)")
 * FArguments& IsChecked(bool InIsChecked)
 * {
 * 		_IsChecked = InIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
 * 		return Me();
 * }
 *
 * // This version would prevent ambiguous conversions.
 * FArguments& IsChecked(ECheckBoxState InIsChecked)
 * {
 *		_IsChecked = InIsChecked;
 * 		return Me();
 * }
 */


/**
 * Abstract base class for Slate widgets.
 *
 * STOP. DO NOT INHERIT DIRECTLY FROM WIDGET!
 *
 * Inheritance:
 *   Widget is not meant to be directly inherited. Instead consider inheriting from LeafWidget or Panel,
 *   which represent intended use cases and provide a succinct set of methods which to override.
 *
 *   SWidget is the base class for all interactive Slate entities. SWidget's public interface describes
 *   everything that a Widget can do and is fairly complex as a result.
 * 
 * Events:
 *   Events in Slate are implemented as virtual functions that the Slate system will call
 *   on a Widget in order to notify the Widget about an important occurrence (e.g. a key press)
 *   or querying the Widget regarding some information (e.g. what mouse cursor should be displayed).
 *
 *   Widget provides a default implementation for most events; the default implementation does nothing
 *   and does not handle the event.
 *
 *   Some events are able to reply to the system by returning an FReply, FCursorReply, or similar
 *   object. 
 */
class SLATECORE_API SWidget
	: public FSlateControlledConstruction,
	  public TSharedFromThis<SWidget>		// Enables 'this->AsShared()'
{
	friend struct FCurveSequence;

public:
	
	/**
	 * Construct a SWidget based on initial parameters.
	 */
	void Construct(
		const TAttribute<FText> & InToolTipText,
		const TSharedPtr<IToolTip> & InToolTip,
		const TAttribute< TOptional<EMouseCursor::Type> > & InCursor,
		const TAttribute<bool> & InEnabledState,
		const TAttribute<EVisibility> & InVisibility,
		const TAttribute<TOptional<FSlateRenderTransform>>& InTransform,
		const TAttribute<FVector2D>& InTransformPivot,
		const FName& InTag,
		const bool InForceVolatile,
		const EWidgetClipping InClipping,
		const TArray<TSharedRef<ISlateMetaData>>& InMetaData);

	void SWidgetConstruct( const TAttribute<FText> & InToolTipText,
		const TSharedPtr<IToolTip> & InToolTip,
		const TAttribute< TOptional<EMouseCursor::Type> > & InCursor,
		const TAttribute<bool> & InEnabledState,
		const TAttribute<EVisibility> & InVisibility,
		const TAttribute<TOptional<FSlateRenderTransform>>& InTransform,
		const TAttribute<FVector2D>& InTransformPivot,
		const FName& InTag,
		const bool InForceVolatile,
		const EWidgetClipping InClipping,
		const TArray<TSharedRef<ISlateMetaData>>& InMetaData)
	{
		Construct(InToolTipText, InToolTip, InCursor, InEnabledState, InVisibility, InTransform, InTransformPivot, InTag, InForceVolatile, InClipping, InMetaData);
	}

	//
	// GENERAL EVENTS
	//

	/**
	 * Called to tell a widget to paint itself (and it's children).
	 * 
	 * The widget should respond by populating the OutDrawElements array with FDrawElements 
	 * that represent it and any of its children.
	 *
	 * @param Args              All the arguments necessary to paint this widget (@todo umg: move all params into this struct)
	 * @param AllottedGeometry  The FGeometry that describes an area in which the widget should appear.
	 * @param MyCullingRect    The clipping rectangle allocated for this widget and its children.
	 * @param OutDrawElements   A list of FDrawElements to populate with the output.
	 * @param LayerId           The Layer onto which this widget should be rendered.
	 * @param InColorAndOpacity Color and Opacity to be applied to all the descendants of the widget being painted
	 * @param bParentEnabled	True if the parent of this widget is enabled.
	 * @return The maximum layer ID attained by this widget or any of its children.
	 */
	int32 Paint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	/**
	 * Ticks this widget with Geometry.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	//
	// KEY INPUT
	//

	/**
	 * Called when focus is given to this widget.  This event does not bubble.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InFocusEvent  The FocusEvent
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent);

	/**
	 * Called when this widget loses focus.  This event does not bubble.
	 *
	 * @param InFocusEvent The FocusEvent
	 */
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent);

	/** Called whenever a focus path is changing on all the widgets within the old and new focus paths */
	DEPRECATED(4.13, "Please use the newer version of OnFocusChanging that takes a FocusEvent")
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath);

	/** Called whenever a focus path is changing on all the widgets within the old and new focus paths */
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent);

	/**
	 * Called after a character is entered while this widget has keyboard focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InCharacterEvent  Character event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);

	/**
	 * Called after a key is pressed when this widget or a child of this widget has focus
	 * If a widget handles this event, OnKeyDown will *not* be passed to the focused widget.
	 *
	 * This event is primarily to allow parent widgets to consume an event before a child widget processes
	 * it and it should be used only when there is no better design alternative.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InKeyEvent  Key event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/**
	 * Called after a key is pressed when this widget has focus (this event bubbles if not handled)
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InKeyEvent  Key event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/**
	 * Called after a key is released when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InKeyEvent  Key event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	/**
	 * Called when an analog value changes on a button that supports analog
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InAnalogInputEvent Analog input event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent );

	//
	// MOUSE INPUT
	//

	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * Just like OnMouseButtonDown, but tunnels instead of bubbling.
	 * If this even is handled, OnMouseButtonDown will not be sent.
	 * 
	 * Use this event sparingly as preview events generally make UIs more
	 * difficult to reason about.
	 */
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/**
	 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/**
	 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is uses a custom bubble strategy.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is uses a custom bubble strategy.
	 *
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent);
	
	/**
	 * Called when the mouse wheel is spun. This event is bubbled.
	 *
	 * @param  MouseEvent  Mouse event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/**
	 * The system asks each widget under the mouse to provide a cursor. This event is bubbled.
	 * 
	 * @return FCursorReply::Unhandled() if the event is not handled; return FCursorReply::Cursor() otherwise.
	 */
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const;

	/**
	 * After OnCursorQuery has specified a cursor type the system asks each widget under the mouse to map that cursor to a widget. This event is bubbled.
	 * 
	 * @return TOptional<TSharedRef<SWidget>>() if you don't have a mapping otherwise return the Widget to show.
	 */
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) const;

	/**
	 * Called when a mouse button is double clicked.  Override this in derived classes.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  InMouseEvent  Mouse button event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/**
	 * Called when Slate wants to visualize tooltip.
	 * If nobody handles this event, Slate will use default tooltip visualization.
	 * If you override this event, you should probably return true.
	 * 
	 * @param  TooltipContent    The TooltipContent that I may want to visualize.
	 * @return true if this widget visualized the tooltip content; i.e., the event is handled.
	 */
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent);

	/**
	 * Visualize a new pop-up if possible.  If it's not possible for this widget to host the pop-up
	 * content you'll get back an invalid pointer to the layer.  The returned FPopupLayer allows you 
	 * to remove the pop-up when you're done with it
	 * 
	 * @param PopupContent The widget to try and host overlaid on top of the widget.
	 *
	 * @return a valid FPopupLayer if this widget supported hosting it.  You can call Remove() on this to destroy the pop-up.
	 */
	virtual TSharedPtr<FPopupLayer> OnVisualizePopup(const TSharedRef<SWidget>& PopupContent);

	/**
	 * Called when Slate detects that a widget started to be dragged.
	 * Usage:
	 * A widget can ask Slate to detect a drag.
	 * OnMouseDown() reply with FReply::Handled().DetectDrag( SharedThis(this) ).
	 * Slate will either send an OnDragDetected() event or do nothing.
	 * If the user releases a mouse button or leaves the widget before
	 * a drag is triggered (maybe user started at the very edge) then no event will be
	 * sent.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  InMouseEvent  MouseMove that triggered the drag
	 */
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	//
	// DRAG AND DROP (DragDrop)
	//

	/**
	 * Called during drag and drop when the drag enters a widget.
	 *
	 * Enter/Leave events in slate are meant as lightweight notifications.
	 * So we do not want to capture mouse or set focus in response to these.
	 * However, OnDragEnter must also support external APIs (e.g. OLE Drag/Drop)
	 * Those require that we let them know whether we can handle the content
	 * being dragged OnDragEnter.
	 *
	 * The concession is to return a can_handled/cannot_handle
	 * boolean rather than a full FReply.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether the contents of the DragDropEvent can potentially be processed by this widget.
	 */
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	
	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 */
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent);

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	
	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	//
	// TOUCH and GESTURES
	//

	/**
	 * Called when the user performs a gesture on trackpad. This event is bubbled.
	 *
	 * @param  GestureEvent  gesture event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent);
	
	/**
	 * Called when a touchpad touch is started (finger down)
	 * 
	 * @param InTouchEvent	The touch event generated
	 */
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);

	/**
	 * Called when a touchpad touch is moved  (finger moved)
	 * 
	 * @param InTouchEvent	The touch event generated
	 */
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);

	/**
	 * Called when a touchpad touch is ended (finger lifted)
	 * 
	 * @param InTouchEvent	The touch event generated
	 */
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);

	/**
	 * Called when motion is detected (controller or device)
	 * e.g. Someone tilts or shakes their controller.
	 * 
	 * @param InMotionEvent	The motion event generated
	 */
	virtual FReply OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent);

	/**
	 * Called to determine if we should render the focus brush.
	 *
	 * @param InFocusCause	The cause of focus
	 */
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const;

	/**
	 * Popups can manifest in a NEW OS WINDOW or via an OVERLAY in an existing window.
	 * This can be set explicitly on SMenuAnchor, or can be determined by a scoping widget.
	 * A scoping widget can reply to OnQueryPopupMethod() to drive all its descendants'
	 * poup methods.
	 *
	 * e.g. Fullscreen games cannot summon a new window, so game SViewports will reply with
	 *      EPopupMethod::UserCurrentWindow. This makes all the menu anchors within them
	 *      use the current window.
	 */
	virtual FPopupMethodReply OnQueryPopupMethod() const;

	virtual TSharedPtr<FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const;
	
	/**
	 * All the pointer (mouse, touch, stylus, etc.) events from this frame have been routed.
	 * This is a widget's chance to act on any accumulated data.
	 */
	virtual void OnFinishedPointerInput();

	/**
	 * All the key (keyboard, gamepay, joystick, etc.) input from this frame has been routed.
	 * This is a widget's chance to act on any accumulated data.
	 */
	virtual void OnFinishedKeyInput();

	/**
	 * Called when navigation is requested
	 * e.g. Left Joystick, Direction Pad, Arrow Keys can generate navigation events.
	 * 
	 * @param InNavigationEvent	The navigation event generated
	 */
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent);

	/**
	 * Called when the mouse is moved over the widget's window, to determine if we should report whether
	 * OS-specific features should be active at this location (such as a title bar grip, system menu, etc.)
	 * Usually you should not need to override this function.
	 *
	 * @return	The window "zone" the cursor is over, or EWindowZone::Unspecified if no special behavior is needed
	 */
	virtual EWindowZone::Type GetWindowZoneOverride() const;

	//
	// LAYOUT
	//

	public:
	
	/** DEPRECATED version of SlatePrepass that assumes no scaling beyond AppScale*/
	void SlatePrepass();

	/**
	 * Descends to leafmost widgets in the hierarchy and gathers desired sizes on the way up.
	 * i.e. Caches the desired size of all of this widget's children recursively, then caches desired size for itself.
	 */
	void SlatePrepass(float LayoutScaleMultiplier);

#if SLATE_DEFERRED_DESIRED_SIZE
	private:
	FORCEINLINE void InvalidateDesiredSize(float LayoutScaleMultiplier)
	{
		bCachedDesiredSize = false;
		DesiredSizeScaleMultiplier = LayoutScaleMultiplier;
	}

	public:
	/** @return the DesiredSize that was computed the last time CacheDesiredSize() was called. */
	FORCEINLINE const FVector2D& GetDesiredSize() const
	{
		if ( !bCachedDesiredSize && ensureMsgf(!bUpdatingDesiredSize, TEXT("The layout is cyclically dependent.  A child widget can not ask the desired size of a parent while the parent is asking the desired size of its children.")) )
		{
			bUpdatingDesiredSize = true;

			// Cache this widget's desired size.
			const_cast<SWidget*>(this)->CacheDesiredSize(DesiredSizeScaleMultiplier);

			bUpdatingDesiredSize = false;
		}

		return DesiredSize;
	}
#else
	public:
	FORCEINLINE const FVector2D& GetDesiredSize() const { return DesiredSize; }
#endif

	/**
	 * The system calls this method. It performs a breadth-first traversal of every visible widget and asks
	 * each widget to cache how big it needs to be in order to present all of its content.
	 */
	protected: virtual void CacheDesiredSize(float);
	
	/**
	 * Explicitly set the desired size. This is highly advanced functionality that is meant
	 * to be used in conjunction with overriding CacheDesiredSize. Use ComputeDesiredSize() instead.
	 */
	protected: void Advanced_SetDesiredSize(const FVector2D& InDesiredSize)
	{
		DesiredSize = InDesiredSize;
#if SLATE_DEFERRED_DESIRED_SIZE
		bCachedDesiredSize = true;
#endif
	}

	/**
	 * Compute the ideal size necessary to display this widget. For aggregate widgets (e.g. panels) this size should include the
	 * size necessary to show all of its children. CacheDesiredSize() guarantees that the size of descendants is computed and cached
	 * before that of the parents, so it is safe to call GetDesiredSize() for any children while implementing this method.
	 *
	 * Note that ComputeDesiredSize() is meant as an aide to the developer. It is NOT meant to be very robust in many cases. If your
	 * widget is simulating a bouncing ball, you should just return a reasonable size; e.g. 160x160. Let the programmer set up a reasonable
	 * rule of resizing the bouncy ball simulation.
	 *
	 * @param  LayoutScaleMultiplier    This parameter is safe to ignore for almost all widgets; only really affects text measuring.
	 *
	 * @return The desired size.
	 */
private:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const = 0;

public:
	/**
	 * Calculates what if any clipping state changes need to happen when drawing this widget.
	 * @return the culling rect that should be used going forward.
	 */
	FSlateRect CalculateCullingAndClippingRules(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, bool& bClipToBounds, bool& bAlwaysClip, bool& bIntersectClipBounds) const;

private:
	void CreateStatID() const;

public:

	FORCEINLINE TStatId GetStatID() const
	{
#if STATS
		// this is done to avoid even registering stats for a disabled group (unless we plan on using it later)
		if (FThreadStats::IsCollectingData())
		{
			if (!StatID.IsValidStat())
			{
				CreateStatID();
			}
			return StatID;
		}
#endif
		return TStatId(); // not doing stats at the moment, or ever
	}

	/** What is the Child's scale relative to this widget. */
	DEPRECATED(4.15, "This version is no longer used, please use the new version which also provides the incoming prepass scale, in case that has bearing on the relative layout scale.")
	virtual float GetRelativeLayoutScale(const FSlotBase& Child) const { return 1.0f; }

	/** What is the Child's scale relative to this widget. */
	virtual float GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const;

	/**
	 * Non-virtual entry point for arrange children. ensures common work is executed before calling the virtual
	 * ArrangeChildren function.
	 * Compute the Geometry of all the children and add populate the ArrangedChildren list with their values.
	 * Each type of Layout panel should arrange children based on desired behavior.
	 *
	 * @param AllottedGeometry    The geometry allotted for this widget by its parent.
	 * @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
	 */
	FORCEINLINE void ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
	{
		OnArrangeChildren(AllottedGeometry, ArrangedChildren);
	}

	/**
	 * Every widget that has children must implement this method. This allows for iteration over the Widget's
	 * children regardless of how they are actually stored.
	 */
	// @todo Slate: Consider renaming to GetVisibleChildren  (not ALL children will be returned in all cases)
	virtual FChildren* GetChildren() = 0;

	/**
	 * Checks to see if this widget supports keyboard focus.  Override this in derived classes.
	 *
	 * @return  True if this widget can take keyboard focus
	 */
	virtual bool SupportsKeyboardFocus() const;

	/**
	 * Checks to see if this widget currently has the keyboard focus
	 *
	 * @return  True if this widget has keyboard focus
	 */
	virtual bool HasKeyboardFocus() const;

	/**
	 * Gets whether or not the specified users has this widget focused, and if so the type of focus.
	 *
	 * @return The optional will be set with the focus cause, if unset this widget doesn't have focus.
	 */
	TOptional<EFocusCause> HasUserFocus(int32 UserIndex) const;

	/**
	 * Gets whether or not any users have this widget focused, and if so the type of focus (first one found).
	 *
	 * @return The optional will be set with the focus cause, if unset this widget doesn't have focus.
	 */
	TOptional<EFocusCause> HasAnyUserFocus() const;

	/**
	 * Gets whether or not the specified users has this widget or any descendant focused.
	 *
	 * @return The optional will be set with the focus cause, if unset this widget doesn't have focus.
	 */
	bool HasUserFocusedDescendants(int32 UserIndex) const;

	/**
	 * @return Whether this widget has any descendants with keyboard focus
	 */
	bool HasFocusedDescendants() const;

	/**
	 * @return whether or not any users have this widget focused, or any descendant focused.
	 */
	bool HasAnyUserFocusOrFocusedDescendants() const;

	/**
	 * Checks to see if this widget is the current mouse captor
	 *
	 * @return  True if this widget has captured the mouse
	 */
	bool HasMouseCapture() const;

	/**
	 * Checks to see if this widget has mouse capture from the provided user.
	 *
	 * @return  True if this widget has captured the mouse
	 */
	bool HasMouseCaptureByUser(int32 UserIndex, TOptional<int32> PointerIndex = TOptional<int32>()) const;

	/** Called when this widget had captured the mouse, but that capture has been revoked for some reason. */
	virtual void OnMouseCaptureLost();

	/**
	 * Ticks this widget and all of it's child widgets.  Should not be called directly.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	void TickWidgetsRecursively( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	/**
	 * Sets the enabled state of this widget
	 *
	 * @param InEnabledState	An attribute containing the enabled state or a delegate to call to get the enabled state.
	 */
	void SetEnabled( const TAttribute<bool>& InEnabledState )
	{
		if ( !EnabledState.IdenticalTo(InEnabledState) )
		{
			EnabledState = InEnabledState;
			Invalidate(EInvalidateWidget::LayoutAndVolatility);
		}
	}

	/** @return Whether or not this widget is enabled */
	FORCEINLINE bool IsEnabled() const 
	{
		return EnabledState.Get();
	}

	/** @return Is this widget interactive or not? Defaults to false */
	virtual bool IsInteractable() const
	{
		return false;
	}

	/** @return The tool tip associated with this widget; Invalid reference if there is not one */
	virtual TSharedPtr<IToolTip> GetToolTip();

	/** Called when a tooltip displayed from this widget is being closed */
	virtual void OnToolTipClosing();

	/**
	 * Sets whether this widget is a "tool tip force field".  That is, tool-tips should never spawn over the area
	 * occupied by this widget, and will instead be repelled to an outside edge
	 *
	 * @param	bEnableForceField	True to enable tool tip force field for this widget
	 */
	void EnableToolTipForceField( const bool bEnableForceField );

	/** @return True if a tool tip force field is active on this widget */
	bool HasToolTipForceField() const
	{
		return bToolTipForceFieldEnabled;
	}

	/** @return True if this widget hovered */
	virtual bool IsHovered() const
	{
		return bIsHovered;
	}

	/** @return True if this widget is directly hovered */
	virtual bool IsDirectlyHovered() const;

	/** @return is this widget visible, hidden or collapsed */
	FORCEINLINE EVisibility GetVisibility() const { return Visibility.Get(); }

	/** @param InVisibility  should this widget be */
	virtual void SetVisibility( TAttribute<EVisibility> InVisibility )
	{
		if ( !Visibility.IdenticalTo(InVisibility) )
		{
			Visibility = InVisibility;
			Invalidate(EInvalidateWidget::LayoutAndVolatility);
		}
	}

	/**
	 * When performing a caching pass, volatile widgets are not cached as part of everything
	 * else, instead they and their children are drawn as normal standard widgets and excluded
	 * from the cache.  This is extremely useful for things like timers and text that change
	 * every frame.
	 */
	FORCEINLINE bool IsVolatile() const { return bCachedVolatile; }

	/**
	 * Was this widget painted as part of a volatile pass previously.  This may mean it was the
	 * widget directly responsible for making a hierarchy volatile, or it may mean it was simply
	 * a child of a volatile panel.
	 */
	FORCEINLINE bool IsVolatileIndirectly() const { return bInheritedVolatility; }

	/**
	 * Should this widget always appear as volatile for any layout caching host widget.  A volatile
	 * widget's geometry and layout data will never be cached, and neither will any children.
	 * @param bForce should we force the widget to be volatile?
	 */
	FORCEINLINE void ForceVolatile(bool bForce)
	{
		bForceVolatile = bForce;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}

	/**
	 * Invalidates the widget from the view of a layout caching widget that may own this widget.
	 * will force the owning widget to redraw and cache children on the next paint pass.
	 */
	FORCEINLINE void Invalidate(EInvalidateWidget Invalidate)
	{
		const bool bWasVolatile = IsVolatileIndirectly() || IsVolatile();
		const bool bVolatilityChanged = Invalidate == EInvalidateWidget::LayoutAndVolatility ? Advanced_InvalidateVolatility() : false;

		if ( bWasVolatile == false || bVolatilityChanged )
		{
			Advanced_ForceInvalidateLayout();
		}
	}

	/**
	 * Recalculates volatility of the widget and caches the result.  Should be called any time 
	 * anything examined by your implementation of ComputeVolatility is changed.
	 */
	FORCEINLINE void CacheVolatility()
	{
		bCachedVolatile = bForceVolatile || ComputeVolatility();
	}

protected:

	/**
	 * Tests if an arranged widget should be culled.
	 * @param MyCullingRect the culling rect of the widget currently doing the culling.
	 * @param ArrangedChild the arranged widget in the widget currently attempting to cull children.
	 */
	bool IsChildWidgetCulled(const FSlateRect& MyCullingRect, const FArrangedWidget& ArrangedChild) const;

protected:

	/**
	 * Recalculates and caches volatility and returns 'true' if the volatility changed.
	 */
	FORCEINLINE bool Advanced_InvalidateVolatility()
	{
		const bool bWasDirectlyVolatile = IsVolatile();
		CacheVolatility();
		return bWasDirectlyVolatile != IsVolatile();
	}

	/**
	 * Forces invalidation, doesn't check volatility.
	 */
	FORCEINLINE void Advanced_ForceInvalidateLayout()
	{
		TSharedPtr<ILayoutCache> SharedLayoutCache = LayoutCache.Pin();
		if (SharedLayoutCache.IsValid() )
		{
			SharedLayoutCache->InvalidateWidget(this);
		}
	}

public:

	/** @return the render transform of the widget. */
	FORCEINLINE const TOptional<FSlateRenderTransform>& GetRenderTransform() const
	{
		return RenderTransform.Get();
	}

	/** @param InTransform the render transform to set for the widget (transforms from widget's local space). TOptional<> to allow code to skip expensive overhead if there is no render transform applied. */
	FORCEINLINE void SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>> InTransform)
	{
		RenderTransform = InTransform;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}

	/** @return the pivot point of the render transform. */
	FORCEINLINE const FVector2D& GetRenderTransformPivot() const
	{
		return RenderTransformPivot.Get();
	}

	/** @param InTransformPivot Sets the pivot point of the widget's render transform (in normalized local space). */
	FORCEINLINE void SetRenderTransformPivot(TAttribute<FVector2D> InTransformPivot)
	{
		RenderTransformPivot = InTransformPivot;
	}

	/**
	 * Sets the clipping to bounds rules for this widget.
	 */
	FORCEINLINE void SetClipping(EWidgetClipping InClipping)
	{
		if (Clipping != InClipping)
		{
			Clipping = InClipping;
			OnClippingChanged();
			Invalidate(EInvalidateWidget::Layout);
		}
	}

	/** @return The current clipping rules for this widget. */
	FORCEINLINE EWidgetClipping GetClipping() const
	{
		return Clipping;
	}

	/**
	 * Sets an additional culling padding that is added to a widget to give more leeway when culling widgets.  Useful if 
	 * several child widgets have rendering beyond their bounds.
	 */
	FORCEINLINE void SetCullingBoundsExtension(const FMargin& InCullingBoundsExtension)
	{
		if (CullingBoundsExtension != InCullingBoundsExtension)
		{
			CullingBoundsExtension = InCullingBoundsExtension;
			Invalidate(EInvalidateWidget::Layout);
		}
	}

	/** @return CullingBoundsExtension */
	FORCEINLINE FMargin GetCullingBoundsExtension() const
	{
		return CullingBoundsExtension;
	}

	/**
	 * Set the tool tip that should appear when this widget is hovered.
	 *
	 * @param InToolTipText  the text that should appear in the tool tip
	 */
	void SetToolTipText(const TAttribute<FText>& ToolTipText);

	void SetToolTipText( const FText& InToolTipText );

	/**
	 * Set the tool tip that should appear when this widget is hovered.
	 *
	 * @param InToolTip  the widget that should appear in the tool tip
	 */
	void SetToolTip( const TSharedPtr<IToolTip>& InToolTip );

	/**
	 * Set the cursor that should appear when this widget is hovered
	 */
	void SetCursor( const TAttribute< TOptional<EMouseCursor::Type> >& InCursor );

	/**
	 * Used by Slate to set the runtime debug info about this widget.
	 */
	void SetDebugInfo( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine );
	
	/**
	 * Get the metadata of the type provided.
	 * @return the first metadata of the type supplied that we encounter
	 */
	template<typename MetaDataType>
	TSharedPtr<MetaDataType> GetMetaData() const
	{
		for (const auto& MetaDataEntry : MetaData)
		{
			if (MetaDataEntry->IsOfType<MetaDataType>())
			{
				return StaticCastSharedRef<MetaDataType>(MetaDataEntry);
			}
		}
		return TSharedPtr<MetaDataType>();	
	}

	/**
	 * Get all metadata of the type provided.
	 * @return all the metadata found of the specified type.
	 */
	template<typename MetaDataType>
	TArray<TSharedRef<MetaDataType>> GetAllMetaData() const
	{
		TArray<TSharedRef<MetaDataType>> FoundMetaData;
		for (const auto& MetaDataEntry : MetaData)
		{
			if (MetaDataEntry->IsOfType<MetaDataType>())
			{
				FoundMetaData.Add(StaticCastSharedRef<MetaDataType>(MetaDataEntry));
			}
		}
		return FoundMetaData;
	}

	/**
	 * Add metadata to this widget.
	 * @param AddMe the metadata to add to the widget.
	 */
	template<typename MetaDataType>
	void AddMetadata(const TSharedRef<MetaDataType>& AddMe)
	{
		MetaData.Add(AddMe);
	}

	/** See OnMouseButtonDown event */
	void SetOnMouseButtonDown(FPointerEventHandler EventHandler);

	/** See OnMouseButtonUp event */
	void SetOnMouseButtonUp(FPointerEventHandler EventHandler);

	/** See OnMouseMove event */
	void SetOnMouseMove(FPointerEventHandler EventHandler);

	/** See OnMouseDoubleClick event */
	void SetOnMouseDoubleClick(FPointerEventHandler EventHandler);

	/** See OnMouseEnter event */
	void SetOnMouseEnter(FNoReplyPointerEventHandler EventHandler);

	/** See OnMouseLeave event */
	void SetOnMouseLeave(FSimpleNoReplyPointerEventHandler EventHandler);

public:

	// Widget Inspector and debugging methods

	/** @return A String representation of the widget */
	virtual FString ToString() const;

	/** @return A String of the widget's type */
	FString GetTypeAsString() const;

	/** @return The widget's type as an FName ID */
	FName GetType() const;

	/** @return A String of the widget's code location in readable format "BaseFileName(LineNumber)" */
	virtual FString GetReadableLocation() const;

	/** @return An FName of the widget's code location (full path with number == line number of the file) */
	FName GetCreatedInLocation() const;

	/** @return The name this widget was tagged with */
	virtual FName GetTag() const;

	/** @return the Foreground color that this widget sets; unset options if the widget does not set a foreground color */
	virtual FSlateColor GetForegroundColor() const;

	/**
	 * Gets the last geometry used to Tick the widget.  This data may not exist yet if this call happens prior to 
	 * the widget having been ticked/painted, or it may be out of date, or a frame behind.
	 *
	 * We recommend not to use this data unless there's no other way to solve your problem.  Normally in Slate we
	 * try and handle these issues by making a dependent widget part of the hierarchy, as to avoid frame behind
	 * or what are referred to as hysteresis problems, both caused by depending on geometry from the previous frame
	 * being used to advise how to layout a dependent object the current frame.
	 */
	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

protected:

	/**
	 * Hidden default constructor.
	 *
	 * Use SNew(WidgetClassName) to instantiate new widgets.
	 *
	 * @see SNew
	 */
	SWidget();

	/** 
	 * Find the geometry of a descendant widget. This method assumes that WidgetsToFind are a descendants of this widget.
	 * Note that not all widgets are guaranteed to be found; OutResult will contain null entries for missing widgets.
	 *
	 * @param MyGeometry      The geometry of this widget.
	 * @param WidgetsToFind   The widgets whose geometries we wish to discover.
	 * @param OutResult       A map of widget references to their respective geometries.
	 * @return True if all the WidgetGeometries were found. False otherwise.
	 */
	bool FindChildGeometries( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const;

	/**
	 * Actual implementation of FindChildGeometries.
	 *
	 * @param MyGeometry      The geometry of this widget.
	 * @param WidgetsToFind   The widgets whose geometries we wish to discover.
	 * @param OutResult       A map of widget references to their respective geometries.
	 */
	void FindChildGeometries_Helper( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const;

	/** 
	 * Find the geometry of a descendant widget. This method assumes that WidgetToFind is a descendant of this widget.
	 *
	 * @param MyGeometry   The geometry of this widget.
	 * @param WidgetToFind The widget whose geometry we wish to discover.
	 * @return the geometry of WidgetToFind.
	 */
	FGeometry FindChildGeometry( const FGeometry& MyGeometry, TSharedRef<SWidget> WidgetToFind ) const;

	/** @return The index of the child that the mouse is currently hovering */
	static int32 FindChildUnderMouse( const FArrangedChildren& Children, const FPointerEvent& MouseEvent );

	/** @return The index of the child that is under the specified position */
	static int32 FindChildUnderPosition(const FArrangedChildren& Children, const FVector2D& ArrangedSpacePosition);

	/** 
	 * Determines if this widget should be enabled.
	 * 
	 * @param InParentEnabled	true if the parent of this widget is enabled
	 * @return true if the widget is enabled
	 */
	bool ShouldBeEnabled( bool InParentEnabled ) const
	{
		// This widget should be enabled if its parent is enabled and it is enabled
		return InParentEnabled && IsEnabled();
	}

	/** @return a brush to draw focus, nullptr if no focus drawing is desired */
	virtual const FSlateBrush* GetFocusBrush() const;

	/**
	 * Recomputes the volatility of the widget.  If you have additional state you automatically want to make
	 * the widget volatile, you should sample that information here.
	 */
	virtual bool ComputeVolatility() const
	{
		return Visibility.IsBound() || EnabledState.IsBound() || RenderTransform.IsBound();
	}

	/**
	 * Protected static helper to allow widgets to access the visibility attribute of other widgets directly
	 * 
	 * @param Widget The widget to get the visibility attribute of
	 */
	static const TAttribute<EVisibility>& AccessWidgetVisibilityAttribute(const TSharedRef<SWidget>& Widget)
	{
		return Widget->Visibility;
	}

	/**
	 * Called when clipping is changed.  Should be used to forward clipping states onto potentially
	 * hidden children that actually are responsible for clipping the content.
	 */
	virtual void OnClippingChanged();

private:

	/**
	 * The widget should respond by populating the OutDrawElements array with FDrawElements
	 * that represent it and any of its children. Called by the non-virtual OnPaint to enforce pre/post conditions
	 * during OnPaint.
	 *
	 * @param Args              All the arguments necessary to paint this widget (@todo umg: move all params into this struct)
	 * @param AllottedGeometry  The FGeometry that describes an area in which the widget should appear.
	 * @param MyCullingRect     The rectangle representing the bounds currently being used to completely cull widgets.  Unless IsChildWidgetCulled(...) returns true, you should paint the widget. 
	 * @param OutDrawElements   A list of FDrawElements to populate with the output.
	 * @param LayerId           The Layer onto which this widget should be rendered.
	 * @param InColorAndOpacity Color and Opacity to be applied to all the descendants of the widget being painted
	 * @param bParentEnabled	True if the parent of this widget is enabled.
	 * @return The maximum layer ID attained by this widget or any of its children.
	 */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const = 0;

	/**
	 * Compute the Geometry of all the children and add populate the ArrangedChildren list with their values.
	 * Each type of Layout panel should arrange children based on desired behavior.
	 *
	 * @param AllottedGeometry    The geometry allotted for this widget by its parent.
	 * @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
	 */
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const = 0;

protected:

	/**
	 * Don't call this directly unless you're a layout cache, this is used to recursively set the layout cache on
	 * on invisible children that never get the opportunity to paint and receive the layout cache through the normal
	 * means.  That way if an invisible widget becomes visible, we still properly invalidate the hierarchy.
	 */
	void CachePrepass(const TWeakPtr<ILayoutCache>& LayoutCache);

protected:
	/**
	 * Registers an "active timer" delegate that will execute at some regular interval. TickFunction will not be called until the specified interval has elapsed once.
	 * A widget can register as many delegates as it needs. Be careful when registering to avoid duplicate active timers.
	 * 
	 * An active timer can be UnRegistered in one of three ways:
	 *   1. Call UnRegisterActiveTimer using the active timer handle that is returned here.
	 *   2. Have your delegate return EActiveTimerReturnType::Stop.
	 *   3. Destroying the widget
	 * 
	 * Active Timers
	 * --------------
	 * Slate may go to sleep when there is no user interaction for some time to save power.
	 * However, some UI elements may need to "drive" the UI even when the user is not providing any input
	 * (ie, animations, viewport rendering, async polling, etc). A widget notifies Slate of this by
	 * registering an "Active Timer" that is executed at a specified frequency to drive the UI.
	 * In this way, slate can go to sleep when there is no input and no active timer needs to fire.
	 * When any active timer needs to fire, all of Slate will do a Tick and Paint pass.
	 * 
	 * @param Period The time period to wait between each execution of the timer. Pass zero to fire the timer once per frame.
	 *                      If an interval is missed, the delegate is NOT called more than once.
	 * @param TimerFunction The active timer delegate to call every Period seconds.
	 * @return An active timer handle that can be used to UnRegister later.
	 */
	TSharedRef<FActiveTimerHandle> RegisterActiveTimer( float TickPeriod, FWidgetActiveTimerDelegate TickFunction );

	/**
	 * Unregisters an active timer handle. This is optional, as the delegate can UnRegister itself by returning EActiveTimerReturnType::Stop.
	 */
	void UnRegisterActiveTimer( const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle );

private:

	/** Iterates over the active timer handles on the widget and executes them if their interval has elapsed. */
	void ExecuteActiveTimers(double CurrentTime, float DeltaTime);

protected:
	/** Dtor ensures that active timer handles are UnRegistered with the SlateApplication. */
	virtual ~SWidget();

protected:
	/** Is this widget hovered? */
	uint8 bIsHovered : 1;

	/** Can the widget ever be ticked. */
	uint8 bCanTick : 1;

	/** Can the widget ever support keyboard focus */
	uint8 bCanSupportFocus : 1;

	/**
	 * Can the widget ever support children?  This will be false on SLeafWidgets, 
	 * rather than setting this directly, you should probably inherit from SLeafWidget.
	 */
	uint8 bCanHaveChildren : 1;

	/**
	  * Some widgets might be a complex hierarchy of child widgets you never see.  Some of those widgets
	  * would expose their clipping option normally, but may not personally be responsible for clipping
	  * so even though it may be set to clip, this flag is used to inform painting that this widget doesn't
	  * really do the clipping.
	  */
	uint8 bClippingProxy : 1;

#if SLATE_DEFERRED_DESIRED_SIZE
	/** Has the desired size of the widget been cached? */
	uint8 bCachedDesiredSize : 1;

	/** Are we currently updating the desired size? */
	mutable uint8 bUpdatingDesiredSize : 1;
#endif

private:

	/**
	 * Whether this widget is a "tool tip force field".  That is, tool-tips should never spawn over the area
	 * occupied by this widget, and will instead be repelled to an outside edge
	 */
	uint8 bToolTipForceFieldEnabled : 1;

	/** Should we be forcing this widget to be volatile at all times and redrawn every frame? */
	uint8 bForceVolatile : 1;

	/** The last cached volatility of this widget.  Cached so that we don't need to recompute volatility every frame. */
	uint8 bCachedVolatile : 1;

	/** If we're owned by a volatile widget, we need inherit that volatility and use as part of our volatility, but don't cache it. */
	mutable uint8 bInheritedVolatility : 1;

protected:
	/**
	 * Set to true if all content of the widget should clip to the bounds of this widget.
	 */
	EWidgetClipping Clipping;

	/**
	 * Can be used to enlarge the culling bounds of this widget (pre-intersection), this can be useful if you've got
	 * children that you know are using rendering transforms to render outside their standard bounds, if that happens
	 * it's possible the parent might be culled before the descendant widget is entirely off screen.  For those cases,
	 * you should extend the bounds of the culling area to add a bit more slack to how culling is performed to this panel.
	 */
	FMargin CullingBoundsExtension;

private:

	/**
	 * Stores the ideal size this widget wants to be.
	 * This member is intentionally private, because only the very base class (Widget) can write DesiredSize.
	 * See CacheDesiredSize(), ComputeDesiredSize()
	 */
	FVector2D DesiredSize;

	/**
	 * Stores the cached Tick Geometry of the widget.  This information can and will be outdated, that's the
	 * nature of it.  However, users were found to often need access to the geometry at times inconvenient to always
	 * need to be located in Widget Tick.
	 */
	mutable FGeometry CachedGeometry;

private:
	/** The list of active timer handles for this widget. */
	TArray<TSharedRef<FActiveTimerHandle>> ActiveTimers;

#if SLATE_DEFERRED_DESIRED_SIZE
	float DesiredSizeScaleMultiplier;
#endif

protected:

	/** Whether or not this widget is enabled */
	TAttribute< bool > EnabledState;

	/** Is this widget visible, hidden or collapsed */
	TAttribute< EVisibility > Visibility;

	/** Render transform of this widget. TOptional<> to allow code to skip expensive overhead if there is no render transform applied. */
	TAttribute< TOptional<FSlateRenderTransform> > RenderTransform;

	/** Render transform pivot of this widget (in normalized local space) */
	TAttribute< FVector2D > RenderTransformPivot;

protected:

	/** Debugging information on the type of widget we're creating for the Widget Reflector. */
	FName TypeOfWidget;

#if !UE_BUILD_SHIPPING

	/** Full file path (and line) in which this widget was created */
	FName CreatedInLocation;

#endif

	/** Tag for this widget */
	FName Tag;

	/** Metadata associated with this widget */
	TArray<TSharedRef<ISlateMetaData>> MetaData;

	/** The cursor to show when the mouse is hovering over this widget. */
	TAttribute< TOptional<EMouseCursor::Type> > Cursor;

private:

	/** Tool tip content for this widget */
	TSharedPtr<IToolTip> ToolTip;

	/** The current layout cache that may need to invalidated by changes to this widget. */
	mutable TWeakPtr<ILayoutCache> LayoutCache;

private:
	// Events
	TMap<FName, FPointerEventHandler> PointerEvents;


	FNoReplyPointerEventHandler MouseEnterHandler;
	FSimpleNoReplyPointerEventHandler MouseLeaveHandler;

private:
	STAT(mutable TStatId				StatID;)
};

//=================================================================
// FGeometry Arranged Widget Inlined Functions
//=================================================================

FORCEINLINE_DEBUGGABLE FArrangedWidget FGeometry::MakeChild(const TSharedRef<SWidget>& ChildWidget, const FVector2D& InLocalSize, const FSlateLayoutTransform& LayoutTransform) const
{
	// If there is no render transform set, use the simpler MakeChild call that doesn't bother concatenating the render transforms.
	// This saves a significant amount of overhead since every widget does this, and most children don't have a render transform.
	if ( ChildWidget->GetRenderTransform().IsSet() )
	{
		return FArrangedWidget(ChildWidget, MakeChild(InLocalSize, LayoutTransform, ChildWidget->GetRenderTransform().GetValue(), ChildWidget->GetRenderTransformPivot()));
	}
	else
	{
		return FArrangedWidget(ChildWidget, MakeChild(InLocalSize, LayoutTransform));
	}
}

FORCEINLINE_DEBUGGABLE FArrangedWidget FGeometry::MakeChild(const TSharedRef<SWidget>& ChildWidget, const FLayoutGeometry& LayoutGeometry) const
{
	return MakeChild(ChildWidget, LayoutGeometry.GetSizeInLocalSpace(), LayoutGeometry.GetLocalToParentTransform());
}

FORCEINLINE_DEBUGGABLE FArrangedWidget FGeometry::MakeChild(const TSharedRef<SWidget>& ChildWidget, const FVector2D& ChildOffset, const FVector2D& InLocalSize, float ChildScale) const
{
	// Since ChildOffset is given as a LocalSpaceOffset, we MUST convert this offset into the space of the parent to construct a valid layout transform.
	// The extra TransformPoint below does this by converting the local offset to an offset in parent space.
	return MakeChild(ChildWidget, InLocalSize, FSlateLayoutTransform(ChildScale, TransformPoint(ChildScale, ChildOffset)));
}
