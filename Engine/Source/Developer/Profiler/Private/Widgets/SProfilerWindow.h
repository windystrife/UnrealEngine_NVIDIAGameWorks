// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Guid.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ProfilerManager.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSplitter.h"

class FActiveTimerHandle;
class SFiltersAndPresets;
class SMultiDumpBrowser;
class SProfilerGraphPanel;
class SProfilerMiniView;
class SVerticalBox;

/** Type definition for shared pointers to instances of SNotificationItem. */
typedef TSharedPtr<class SNotificationItem> SNotificationItemPtr;

/** Type definition for shared references to instances of SNotificationItem. */
typedef TSharedRef<class SNotificationItem> SNotificationItemRef;

/** Type definition for weak references to instances of SNotificationItem. */
typedef TWeakPtr<class SNotificationItem> SNotificationItemWeak;


/**
 * Implements the profiler window.
 */
class SProfilerWindow : public SCompoundWidget
{
public:
	/** Default constructor. */
	SProfilerWindow();

	/** Virtual destructor. */
	virtual ~SProfilerWindow();

	SLATE_BEGIN_ARGS(SProfilerWindow){}
	SLATE_END_ARGS()
	
	/**
	 * Constructs this widget.
	 */
	void Construct( const FArguments& InArgs );

	void ManageEventGraphTab( const FGuid ProfilerInstanceID, const bool bCreateFakeTab, const FString TabName );
	void UpdateEventGraph( const FGuid ProfilerInstanceID, const FEventGraphDataRef AverageEventGraph, const FEventGraphDataRef MaximumEventGraph, bool bInitial );

	void ManageLoadingProgressNotificationState( const FString& Filename, const EProfilerNotificationTypes NotificatonType, const ELoadingProgressStates ProgressState, const float DataLoadingProgress );

	void OpenProfilerSettings();
	void CloseProfilerSettings();

protected:
	/** Callback for determining the visibility of the 'Select a session' overlay. */
	EVisibility IsSessionOverlayVissible() const;

	/** Callback for getting the enabled state of the profiler window. */
	bool IsProfilerEnabled() const;

	void SendingServiceSideCapture_Cancel( const FString Filename );

	void SendingServiceSideCapture_Load( const FString Filename );

	void ProfilerManager_OnViewModeChanged( EProfilerViewMode NewViewMode );

private:

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	//virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	 *
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/**
	 * Called after a key is pressed when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )  override;

	/** Updates the amount of time the profiler has been active */
	EActiveTimerReturnType UpdateActiveDuration( double InCurrentTime, float InDeltaTime );

	/** The handle to the active update duration tick */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	/** Holds all widgets for the profiler window like menu bar, toolbar and tabs. */
	TSharedPtr<SVerticalBox> MainContentPanel;

public:
	
	/** Holds all event graphs. */
	TSharedPtr<SVerticalBox> EventGraphPanel;

	TSharedPtr<SMultiDumpBrowser> MultiDumpBrowser;

	/** Holds the filter and presets widget/slot. */
	SSplitter::FSlot* FiltersAndPresetsSlot;
	TSharedPtr<SFiltersAndPresets> FiltersAndPresets;

	/** Widget for the panel which contains all graphs and event graphs. */
	TSharedPtr<SProfilerGraphPanel> GraphPanel;

	/** Widget for the non-intrusive notifications. */
	TSharedPtr<SNotificationList> NotificationList;

	/** Overlay slot which contains the profiler settings widget. */
	SOverlay::FOverlaySlot* OverlaySettingsSlot;

	/** Holds all active and visible notifications, stored as FGuid -> SNotificationItemWeak . */
	TMap< FString, SNotificationItemWeak > ActiveNotifications;

	/** Active event graphs, one event graph for each profiler instance, stored as FGuid -> SEventGraph. */
	TMap< FGuid, TSharedRef<class SEventGraph> > ActiveEventGraphs;

	/** Widget for the profiler mini view */
	TSharedPtr<SProfilerMiniView> ProfilerMiniView;

	/** The number of seconds the profiler has been active */
	float	DurationActive;
};
