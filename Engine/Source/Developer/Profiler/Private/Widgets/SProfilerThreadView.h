// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "ProfilerStream.h"



/**
 * Widget used to present thread data in the mini-view.
 */
class SProfilerThreadView
	: public SCompoundWidget
{
	enum 
	{
		/** Minimum width of the one rendered sample, if less cycles counter will be combined. */
		MIN_NUM_PIXELS_PER_SAMPLE = 32,

		/**
		 *	Number of milliseconds that can be renderer at once in the window.
		 *	For the default zoom value.
		 */
		NUM_MILLISECONDS_PER_WINDOW = 33,

		/** Number of pixels needed to render one row of cycle counter. */
		NUM_PIXELS_PER_ROW = 16,

		/** Number of pixels. */
		MOUSE_SNAP_DISTANCE = 4,

		/** Wait time in milliseconds before we display a tooltip. */
		TOOLTIP_DELAY = 500,

		/** Width of the thread description windows. */
		WIDTH_THREAD_DESC = 128,

		/**
		 *	Displayed data will be partitioned into smaller batches to avoid long processing times.
		 *	This should help in situation when the user scroll the thread-view, so we don't need to wait for the whole data.
		 *	At the same it adds some overhead to the processing in favor of using massive parallel processing,
		 *	so overall it will be faster and much more responsive.
		 *	One partition must have a least one frame.
		 */
		NUM_DATA_PARTITIONS = 16,
		// #Profiler: 2014-04-25 Dynamic data partitioning

		/**
		 *	Maximum zoom value for time axis.
		 *	Default value mean that one 33ms frame can be rendered at once in the window.
		 *	Maximum zoom allows to see individual cycles.
		 */
		INV_MIN_VISIBLE_RANGE_X = 10000,
		MAX_VISIBLE_RANGE_X = 250,

		/** Number of pixels between each time line. */
		NUM_PIXELS_BETWEEN_TIMELINE = 96,
	};

	struct EThreadViewCursor
	{
		enum Type
		{
			Default,
			Arrow,
			Hand,
		};
	};

	/** Holds current state provided by OnPaint function, used to simplify drawing. */
	struct FSlateOnPaintState : public FNoncopyable
	{
		FSlateOnPaintState( const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect, FSlateWindowElementList& InOutDrawElements, int32& InLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect InDrawEffects )
			: AllottedGeometry( InAllottedGeometry )
			, AbsoluteClippingRect( InMyCullingRect )
			, LocalClippingRect( FVector2D::ZeroVector, InAllottedGeometry.GetLocalSize() )
			, WidgetStyle( InWidgetStyle )
			, OutDrawElements( InOutDrawElements )
			, LayerId( InLayerId )
			, DrawEffects( InDrawEffects )
			, FontMeasureService( FSlateApplication::Get().GetRenderer()->GetFontMeasureService() )
			, SummaryFont8( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 8 )
			, SummaryFont8Height( FontMeasureService->Measure( TEXT( "!" ), SummaryFont8 ).Y )
		{}

		const FVector2D& Size2D() const
		{
			return AllottedGeometry.GetLocalSize();
		}

		/** Accessors. */
		const FGeometry& AllottedGeometry; 
		const FSlateRect& AbsoluteClippingRect;
		const FSlateRect LocalClippingRect;
		const FWidgetStyle& WidgetStyle;
		 
		FSlateWindowElementList& OutDrawElements;
		int32& LayerId;
		const ESlateDrawEffect DrawEffects;

		const TSharedRef< FSlateFontMeasure > FontMeasureService;

		const FSlateFontInfo SummaryFont8;
		const float SummaryFont8Height;
	};

public:
	SProfilerThreadView();
	~SProfilerThreadView();

	SLATE_BEGIN_ARGS( SProfilerThreadView )
		{}
	SLATE_END_ARGS()

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct( const FArguments& InArgs );

	/** Resets internal widget's data to the default one. */
	void Reset();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

	void ShowContextMenu( const FVector2D& ScreenSpacePosition );

	/**	Binds UI commands to delegates. */
	void BindCommands();

	void DrawText( const FString& Text, const FSlateFontInfo& FontInfo, FVector2D Position, const FColor& TextColor, const FColor& ShadowColor, FVector2D ShadowOffset, const FSlateRect* ClippingRect = nullptr ) const;

	void DrawUIStackNodes_Recursively( const FProfilerUIStackNode& UIStackNode ) const;

	void DrawFramesBackgroundAndTimelines() const;
	void DrawUIStackNodes() const;
	void DrawFrameMarkers() const;

public:

	/**
	 *	Changes the position-x of the thread view.
	 *	Called by the horizontal scroll bar.
	 */
	void SetPositionXToByScrollBar( double ScrollOffset );
	void SetPositionX( double NewPositionXMS );

	/**
	 *	Changes the position-y of the thread view.
	 *	Called by the external code.
	 */
	void SetPositonYTo( double ScrollOffset );

	/** Changes the position-x and range-x of the thread view. */
	void SetTimeRange( double StartTimeMS, double EndTimeMS, bool bBroadcast = true );

	/**
	 *	Changes the position-x and range-x of the thread view. 
	 *	Called by the mini-view.
	 */
	void SetFrameRange( int32 FrameStart, int32 FrameEnd );

	/** Attaches profiler stream to the thread-view widgets and displays the first frame of data. */
	void AttachProfilerStream( const FProfilerStream& InProfilerStream );

public:
	/** The event to execute when the position-x of the thread view has been changed. */
	DECLARE_EVENT_FiveParams( SProfilerThreadView, FViewPositionXChangedEvent, double /*StartTimeMS*/, double /*EndTimeMS*/, double /*MaxEndTimeMS*/, int32 /*FrameStart*/, int32 /*FrameEnd*/ );
	FViewPositionXChangedEvent& OnViewPositionXChanged()
	{
		return ViewPositionXChangedEvent;
	}
	
protected:
	/** The event to execute when the position-x of the thread view has been changed. */
	FViewPositionXChangedEvent ViewPositionXChangedEvent;

public:
	/** The event to execute when the position-y of the thread view has been changed. */
	DECLARE_EVENT_ThreeParams( SProfilerThreadView, FViewPositionYChangedEvent, double /*PosYStart*/, double /*PosYEnd*/, double /*MaxPosY*/ );
	FViewPositionYChangedEvent& OnViewPositionYChanged()
	{
		return ViewPositionYChangedEvent;
	}

protected:
	/** The event to execute when the position-y of the thread view has been changed. */
	FViewPositionYChangedEvent ViewPositionYChangedEvent;

protected:
	
	/**
	 * @return True, if the widget is ready to use, also means that contains at least one frame of the thread data.
	 */
	bool IsReady() const;
	void ProcessData();
	bool ShouldUpdateData();
	void UpdateInternalConstants();

protected:

	/*-----------------------------------------------------------------------------
		Data variables
	-----------------------------------------------------------------------------*/

	/** Profiler UI stream, contains data optimized for displaying in this widget. */
	FProfilerUIStream ProfilerUIStream;

	/** Pointer to the profiler stream, used as a source for the UI stream. */
	const FProfilerStream* ProfilerStream;

	/*-----------------------------------------------------------------------------
		UI variables
	-----------------------------------------------------------------------------*/

	FGeometry ThisGeometry;

	/** Current Slate OnPaint state. */
	uint8 PaintStateMemory[sizeof(FSlateOnPaintState)];
	mutable FSlateOnPaintState*	PaintState;

	/** The current mouse position. */
	FVector2D MousePosition;

	/** The last mouse position. */
	FVector2D LastMousePosition;

	/** Mouse position during the call on mouse button down. */
	FVector2D MousePositionOnButtonDown;

	/** Position-X of the thread view, in milliseconds. */
	double PositionXMS;

	/** Position-Y of the thread view, where 1.0 means one row of the data. */
	double PositionY;

	/** Range of the visible data for the current zoom, in milliseconds. */
	double RangeXMS;

	/** Range of the visible data. */
	double RangeY;

	/** Range of the all collected data, in milliseconds. */
	double TotalRangeXMS;

	/** Range of the all collected data. */
	double TotalRangeY;

	/** Current zoom value for X. */
	double ZoomFactorX;

	/** Number of milliseconds that can be renderer at once in the window. */
	double NumMillisecondsPerWindow;

	/** Number of pixels needed to render one millisecond cycle counter. */
	double NumPixelsPerMillisecond;

	/** Number of milliseconds that can be displayed as one cycle counter. */
	double NumMillisecondsPerSample;

	/** Index of the frame currently being hovered by the mouse. */
	int32 HoveredFrameIndex;

	/** Thread ID currently being hovered by the mouse. */
	int32 HoveredThreadID;

	/** Position-Y of the thread view currently being hovered by the mouse, in milliseconds. */
	double HoveredPositionX;

	/** Position-Y of the thread view currently being hovered by the mouse. */
	double HoveredPositionY;

	/** Distance dragged. */
	double DistanceDragged;

	/** Frame indices of the currently visible data. X= FrameStart, Y=FrameEnd+1 */
	FIntPoint FramesIndices;

	bool bIsLeftMousePressed;
	bool bIsRightMousePressed;

	/** Whether to updated data. */
	bool bUpdateData;

	/** Cursor type. */
	EThreadViewCursor::Type CursorType;
};
