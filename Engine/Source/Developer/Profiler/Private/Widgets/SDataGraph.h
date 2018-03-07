// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "ProfilerDataSource.h"

class FPaintArgs;
class FSlateWindowElementList;
class FTrackedStat;
class SVerticalBox;

namespace EDataGraphViewModes
{
	enum Type
	{
		/** Time based view mode. */
		Time,

		/** Index based view mode. */
		Index,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax
	};
}


namespace EDataGraphMultiModes
{
	enum Type
	{
		/** Combined graph data source is displayed as area line graph with minimum, average and maximum value. */
		Combined,

		/** Combined graph data source is displayed as one line graph for each graph data source. */
		OneLinePerDataSource,

		InvalidOrMax,
	};
}


/** The delegate to be invoked when the frame offset has been changed. */
DECLARE_DELEGATE_OneParam( FGraphOffsetChangedDelegate, int /*InFrameOffset*/ );

/** The delegate to be invoked when the data graph summary widget wants to know index of the frame currently being hovered by the mouse. */
DECLARE_DELEGATE_RetVal( int32, FGetHoveredFrameIndexDelegate );

/** The delegate to be invoked when the data graph view mode has changed. */
DECLARE_DELEGATE_OneParam( FViewModeChangedDelegate, EDataGraphViewModes::Type );

// @TODO: Add a generic proxy class to support index and time based view modes at the same time.

/** The delegate to be invoked when the selected frames have been changed, for index based view mode. */
DECLARE_DELEGATE_TwoParams( FSelectionChangedForTimeDelegate, float /*FrameStartTimeMS*/, float /*FrameEndTimeMS*/ );


/** A custom widget used to display graphs. */
// Rename to SDataChart ?
class SDataGraph : public SCompoundWidget
{
public:
	/** Default constructor. */
	SDataGraph();

	/** Virtual destructor. */
	virtual ~SDataGraph();

	SLATE_BEGIN_ARGS( SDataGraph )
		: _OnGraphOffsetChanged()
		, _OnViewModeChanged()
		{
			_Clipping = EWidgetClipping::ClipToBounds;
		}

		SLATE_EVENT( FGraphOffsetChangedDelegate, OnGraphOffsetChanged )
		SLATE_EVENT( FViewModeChangedDelegate, OnViewModeChanged )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	
	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	 *
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	
	/**
	 * Called when the mouse wheel is spun. This event is bubbled.
	 *
	 * @param  MouseEvent  Mouse event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	/**
	 * Called when a mouse button is double clicked.  Override this in derived classes.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  InMouseEvent  Mouse button event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

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
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )  override;
	
	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent )  override;

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )  override;

	/**
	 * Called when the system wants to know which cursor to display for this Widget.  This event is bubbled.
	 *
	 * @return  The cursor requested (can be None.)
	 */
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;


	/*-----------------------------------------------------------------------------
		Events
	-----------------------------------------------------------------------------*/

	/**
	 * The event to execute when the selected frames have been changed, for index based view mode.
	 *
	 * @param FrameStartIndex	- <describe FrameStartIndex>
	 * @param FrameEndIndex		- <describe FrameEndIndex>
	 *
	 */
	DECLARE_EVENT_TwoParams( SDataGraph, FSelectionChangedForIndexEvent, uint32 /*FrameStartIndex*/, uint32 /*FrameEndIndex*/ );
	FSelectionChangedForIndexEvent& OnSelectionChangedForIndex()
	{
		return SelectionChangedForIndexEvent;
	}

	void EventGraph_OnRestoredFromHistory( uint32 FrameStartIndex, uint32 FrameEndIndex );

protected:
	/** The event to execute when the selected frames have been changed, for index based view mode. */
	FSelectionChangedForIndexEvent SelectionChangedForIndexEvent;

public:
	void AddInnerGraph( const TSharedPtr<FTrackedStat>& TrackedStat );
	void RemoveInnerGraph( const uint32 StatID );

	/**
	 * Scrolls this data graph widget to the specified offset.
	 */
	void ScrollTo( int32 InGraphOffset )
	{
		GraphOffset = FMath::Clamp( InGraphOffset, 0, FMath::Max(NumDataPoints-NumVisiblePoints,0) );
	}

	/**
	 * @return the number of data graph points that can be displayed at once in this widget.
	 */
	const uint32 GetNumVisiblePoints() const
	{
		return NumVisiblePoints;
	}

	/**
	 * @return the number of data graph points.
	 */
	const uint32 GetNumDataPoints() const
	{
		return NumDataPoints;
	}

	const EDataGraphViewModes::Type GetViewMode() const
	{
		return ViewMode;
	}

	

protected:
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(16.0f,16.0f);
	}
	
	/** Called when the data graph summary widget wants to know index of the frame currently being hovered by the mouse. */
	int32 DataGraphSummary_GetHoveredFrameIndex() const
	{
		return HoveredFrameIndex;
	}

	/**
	 * 
	 * @return calculates frame index based on the specified mouse position.
	 */
	const int32 CalculateFrameIndex( const FVector2D MousePosition ) const;


	void ShowContextMenu(const FVector2D& ScreenSpacePosition, const FPointerEvent& MouseEvent);

	/**	Binds our UI commands to delegates. */
	void BindCommands();

	void UpdateState();

	/** Handles FExecuteAction for ViewMode_SetIndexBased. */
	void ViewMode_SetIndexBased_Execute();
	/** Handles FCanExecuteAction for ViewMode_SetIndexBased. */
	bool ViewMode_SetIndexBased_CanExecute() const;
	/** Handles FIsActionChecked for ViewMode_SetIndexBased. */
	bool ViewMode_SetIndexBased_IsChecked() const;


	/** Handles FExecuteAction for DataGraph_ViewMode_SetTimeBased. */
	void ViewMode_SetTimeBased_Execute();
	/** Handles FCanExecuteAction for DataGraph_ViewMode_SetTimeBased. */
	bool ViewMode_SetTimeBased_CanExecute() const;
	/** Handles FIsActionChecked for DataGraph_ViewMode_SetTimeBased. */
	bool ViewMode_SetTimeBased_IsChecked() const;

	const TSharedPtr<FTrackedStat> GetFirstGraph() const;

	/*-----------------------------------------------------------------------------
		Misc
	-----------------------------------------------------------------------------*/

	/** Horizontal box widget where graph descriptions are displayed. */
	TSharedPtr<SVerticalBox> GraphDescriptionsVBox;

	TMap< uint32, TSharedPtr<FTrackedStat> > StatIDToGraphDescriptionMapping;
	TMap< uint32, TSharedRef<SWidget> > StatIDToWidgetMapping;

	/** The current mouse position. */
	FVector2D MousePosition;

	/** Mouse position during the call on mouse button up. */
	FVector2D MousePositionOnButtonDown;

	/** Mouse position during the call on mouse button down. */
	FVector2D MousePositionOnButtonUp;

	/** Accumulated mouse wheel value. */
	float MouseWheelAcc;

	/** Vertical scale of the graph. */
	float ScaleY;

	/** True, if the user is currently interactively scrolling the view by holding the right mouse button and dragging. */
	bool bIsRMB_Scrolling;

	/** True, if the user is currently changing the graph selection by holding the left right mouse button and dragging. */
	bool bIsLMB_SelectionDragging;

	bool bIsLMB_Pressed;
	bool bIsRMB_Pressed;

	FGeometry ThisGeometry;

	/*-----------------------------------------------------------------------------
		Delegates
	-----------------------------------------------------------------------------*/

	/** The delegate to be invoked when the frame offset has been changed. */
	FGraphOffsetChangedDelegate OnGraphOffsetChanged;

	/** The delegate to be invoked when the data graph view mode has changed. */
	FViewModeChangedDelegate OnViewModeChanged;

	FSelectionChangedForTimeDelegate OnSelectionChangedForTime;


	/*-----------------------------------------------------------------------------
		Data graph state
	-----------------------------------------------------------------------------*/

	EDataGraphViewModes::Type ViewMode;
	EDataGraphMultiModes::Type MultiMode;

	/**
	 *	Number of frames needed to display one second of the data graph. The default value is 60.
	 *	Currently this is a constant value.
	 */
	const FTimeAccuracy::Type TimeBasedAccuracy;

	/**
	 *	The distance between each line point.
	 *	Currently this is a constant value.
	 *	@see GraphOneSecondWidth
	 */
	const int DistanceBetweenPoints;

	/*-----------------------------------------------------------------------------
		Frame based
	-----------------------------------------------------------------------------*/

	/** Number of data graph points in the data graph source, it may concern combined data graph source or data graph source. */
	int32 NumDataPoints;

	/** Number of data graph points that can be displayed at once in this widget, depends on the zoom/scale factor. */
	int32 NumVisiblePoints;

	/** Current offset of the graph, index of the first visible graph point. */
	int32 GraphOffset;
	float RealGraphOffset;

	/** Index of the frame currently being hovered by the mouse. */
	int32 HoveredFrameIndex;

	int32 FrameIndices[2];

	/*-----------------------------------------------------------------------------
		Time based
	-----------------------------------------------------------------------------*/

	/** Total time of the data graph source, in milliseconds. */
	float DataTotalTimeMS;

	/** Period of time that can be displayed at once in this widget, depends on the zoom/scale factor. */
	float VisibleTimeMS;

	/** Current offset of the graph, time offset of the first visible graph point, in milliseconds. */
	float GraphOffsetMS;

	/** Start time of the frame being hovered by the mouse, in milliseconds. */
	float HoveredFrameStartTimeMS;

	float FrameTimesMS[2];
};
