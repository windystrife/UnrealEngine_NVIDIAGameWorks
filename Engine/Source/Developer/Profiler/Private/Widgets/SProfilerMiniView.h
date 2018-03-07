// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ProfilerCommon.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/RenderingCommon.h"

class FPaintArgs;
class FProfilerStatMetaData;
class FSlateWindowElementList;
struct FSlateFontInfo;

struct FFrameThreadTimes
{
	/** Number of the frame. */
	int FrameNumber;
	/** Thread times for the frame. */
	TMap<uint32, float> ThreadTimes;
};

struct FMiniViewSample
{
	/** Frames for one mini-view frame, at least 1. */
	TArray<int32> Frames;

	/** Aggregates thread times for the Frames. */
	TMap<uint32,float> ThreadTimes;

	/**
	 *	Accumulative time for all threads. 
	 *	At this moment only for the game and render threads.
	 */
	float TotalThreadTime;

	/** Accumulative time for the game thread. */
	float GameThreadTime;

	/** Accumulative time for the render thread. */
	float RenderThreadTime;

	FMiniViewSample()
		: TotalThreadTime(0.0f)
		, GameThreadTime(0.0f)
		, RenderThreadTime(0.0f)
	{}

	void AddFrameAndAccumulate( const FFrameThreadTimes& FrameThreadTimes )
	{
		new(Frames) int32( FrameThreadTimes.FrameNumber );
		for( auto It = FrameThreadTimes.ThreadTimes.CreateConstIterator(); It; ++It )
		{
			ThreadTimes.FindOrAdd( It.Key() ) += It.Value();
		}
	}

	void AddFrameAndFindMax( const FFrameThreadTimes& FrameThreadTimes )
	{
		new(Frames) int32( FrameThreadTimes.FrameNumber );
		for( auto It = FrameThreadTimes.ThreadTimes.CreateConstIterator(); It; ++It )
		{
			float& ThreadMS = ThreadTimes.FindOrAdd( It.Key() );
			ThreadMS = FMath::Max( ThreadMS, It.Value() );
		}
	}

	void CalculateTotalThreadTime( const uint32 GameThreadID, const TArray<uint32>& RenderThreadIDs )
	{
		TotalThreadTime = 0.0f;
		GameThreadTime = 0.0f;
		RenderThreadTime = 0.0f;
		for( auto It = ThreadTimes.CreateConstIterator(); It; ++It )
		{
			const float CurrentTimeMS = It.Value();		

			if( It.Key() == GameThreadID )
			{
				TotalThreadTime += CurrentTimeMS;
				GameThreadTime += CurrentTimeMS;
			}
			else if( RenderThreadIDs.Contains(It.Key()) )
			{
				TotalThreadTime += CurrentTimeMS;
				RenderThreadTime += CurrentTimeMS;
			}
		}

		TotalThreadTime /= (float)Frames.Num();
		GameThreadTime /= (float)Frames.Num();
		RenderThreadTime /= (float)Frames.Num();
	}

	void CalculateMaxThreadTime( const uint32 GameThreadID, const TArray<uint32>& RenderThreadIDs )
	{
		TotalThreadTime = 0.0f;
		GameThreadTime = 0.0f;
		RenderThreadTime = 0.0f;

		float CurrentGameThreadTime = 0.0f;
		float CurrentRenderThreadTime = 0.0f;

		for( auto It = ThreadTimes.CreateConstIterator(); It; ++It )
		{
			const float CurrentTimeMS = It.Value();

			if( It.Key() == GameThreadID )
			{
				CurrentGameThreadTime = CurrentTimeMS;
			}
			else if( RenderThreadIDs.Contains( It.Key() ) )
			{
				CurrentRenderThreadTime = CurrentTimeMS;
			}

			const float CurrentTotalTile = CurrentGameThreadTime + CurrentRenderThreadTime;
			if( CurrentTotalTile > TotalThreadTime )
			{
				TotalThreadTime = CurrentTotalTile;
				GameThreadTime = CurrentRenderThreadTime;
				RenderThreadTime = CurrentGameThreadTime;
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** Widget used to present thread data in the mini-view. */
class SProfilerMiniView : public SCompoundWidget
{
	enum 
	{
		/** Minimum width of the one rendered sample. */
		MIN_NUM_PIXELS_PER_SAMPLE = 4,

		/** Number of pixels. */
		MOUSE_SNAP_DISTANCE = 4,

		/**
		 *	Maximum total thread time that will be visible on the mini-view.
		 *	This is enough to see performance issues on the mini-view.
		 *	Everything else will be clamped to that value.
		 */
		MAX_VISIBLE_THREADTIME = 150,
	};

	struct EMiniviewCursor
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
			, MyCullingRect( InMyCullingRect )
			, WidgetStyle( InWidgetStyle )
			, OutDrawElements( InOutDrawElements )
			, LayerId( InLayerId )
			, DrawEffects( InDrawEffects )
		{}

#if	0
		/** Set operation. */
		void Set( const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect, FSlateWindowElementList& InOutDrawElements, int32& InLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect InDrawEffects )
		{
			_AllottedGeometry = &InAllottedGeometry;
			_MyCullingRect = &InMyCullingRect;
			_WidgetStyle = &InWidgetStyle;
			_OutDrawElements = &InOutDrawElements;
			_LayerId = &InLayerId;
			_DrawEffects = InDrawEffects;
		}
#endif // 0

		/** Accessors. */
		const FGeometry& AllottedGeometry; 
		const FSlateRect& MyCullingRect;
		const FWidgetStyle& WidgetStyle;
		 
		FSlateWindowElementList& OutDrawElements;
		int32& LayerId;
		const ESlateDrawEffect DrawEffects;
	};

public:
	SProfilerMiniView();
	~SProfilerMiniView();

	SLATE_BEGIN_ARGS( SProfilerMiniView )	
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}
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

	/** For the specified frame ands a thread data. */
	void AddThreadTime( int32 InFrameIndex, const TMap<uint32, float>& InThreadMS, const TSharedRef<FProfilerStatMetaData>& InStatMetaData );

	/**
	 *	Moves the selection box to the location specified by FrameStart and FrameEnd. 
	 *	Called by external widgets.
	 */
	void MoveWithoutZoomSelectionBox( int32 FrameStart, int32 FrameEnd );
	void MoveAndZoomSelectionBox( int32 FrameStart, int32 FrameEnd );

	void DrawText( const FString& Text, const FSlateFontInfo& FontInfo, FVector2D Position, const FColor& TextColor, const FColor& ShadowColor, FVector2D ShadowOffset ) const;

protected:
	/**
	 *	Moves the selection box to the location specified by FrameIndex.
	 *	Centers it at the same time.
	 */
	void MoveSelectionBox( int32 FrameIndex );

public:
	/** The event to execute when the selection box has been changed. */
	DECLARE_EVENT_TwoParams( SProfilerMiniView, FSelectionBoxChangedEvent, int32 /*FrameStart*/, int32 /*FrameEnd*/ );
	FSelectionBoxChangedEvent& OnSelectionBoxChanged()
	{
		return SelectionBoxChangedEvent;
	}
	
protected:
	/** The event to execute when the selection box has been changed. */
	FSelectionBoxChangedEvent SelectionBoxChangedEvent;

	/**
	 * @return Index of the frame currently being hovered by the mouse
	 */
	int32 GetHoveredFrameIndex() const
	{
		return HoveredFrameIndex;
	}

	/**
	* @return calculates frame index based on the specified mouse position.
	*/
	const int32 PositionToFrameIndex( const float InPositionX ) const;

	/**
	* @return calculates position based on the specified frame index.
	*/
	const float FrameIndexToPosition( const int32 FrameIndex ) const
	{
		return FrameIndex*NumPixelsPerFrame;
	}

protected:
	void ProcessData();

	void UpdateNumPixelsPerSample()
	{
		NumPixelsPerSample = FMath::Max( (float)MIN_NUM_PIXELS_PER_SAMPLE, ThisGeometry.GetLocalSize().X / AllFrames.Num() );
	}

	const float GetNumPixelsPerSample() const
	{
		return NumPixelsPerSample;
	}

	const int32 FindMiniViewSampleIndex( const int32 FrameIndex ) const
	{
		// #Profiler: 2014-04-07 Naive implementation, consider using binary search.
		for( int32 SampleIndex = 0; SampleIndex < MiniViewSamples.Num(); ++SampleIndex )
		{
			const FMiniViewSample& MiniViewSample = MiniViewSamples[SampleIndex];
			for( const int32& Frame : MiniViewSample.Frames )
			{
				if( FrameIndex == Frame )
				{
					return SampleIndex;
				}
			}
		}

		return 0;
	}


	/**
	 * @return True, if the widget is ready to use, also means that contains at least one frame of the thread data.
	 */
	bool IsReady() const
	{
		return AllFrames.Num() > 0;
	}

	bool ShouldUpdateData() const
	{
		return bUpdateData;
	}

protected:

	/*-----------------------------------------------------------------------------
		Data variables
	-----------------------------------------------------------------------------*/

	/** Processed data used to render the mini-view samples. */
	TArray<FMiniViewSample> MiniViewSamples;

	/**
	 *	All mini-view frames history
	 *	60fps*60s*60m=216k samples for one hour, 
	 *	For 16 threads it's about 16mb of data.
	 */
	TArray<FFrameThreadTimes> AllFrames;

	/** Recently added thread times, not processed yet. */
	TArray<FFrameThreadTimes> RecentlyAddedFrames;

	/** Maximum total mini-view frame time seen so far. */
	float MaxFrameTime;

	/** Shared pointer to the stats' metadata. */
	TSharedPtr<FProfilerStatMetaData> StatMetadata;

	/*-----------------------------------------------------------------------------
		UI variables
	-----------------------------------------------------------------------------*/

	FGeometry ThisGeometry;

	/** Current Slate OnPaint state. */
	uint8 PaintStateMemory[sizeof(FSlateOnPaintState)];
	mutable FSlateOnPaintState*	PaintState;

	/** Mouse position during the call on mouse button down. */
	FVector2D MousePositionOnButtonDown;

	/** Frame start and frame end for the selection box. */
	int32 SelectionBoxFrameStart;
	int32 SelectionBoxFrameEnd;

	/** Index of the frame currently being hovered by the mouse. */
	int32 HoveredFrameIndex;

	/** Distance dragged. */
	float DistanceDragged;

	/** Width of the one rendered sample, min value is 4. */
	float NumPixelsPerSample;

	/** Number of pixels for one frame. */
	float NumPixelsPerFrame;

	bool bIsLeftMousePressed;
	bool bIsRightMousePressed;

	bool bCanBeStartDragged;
	bool bCanBeEndDragged;

	/** Whether to allow zooming through the selection box. */
	bool bAllowSelectionBoxZooming;

	/** Whether to updated data, set to true during window resize or if new data has been added. */
	bool bUpdateData;

	/** Cursor type. */
	EMiniviewCursor::Type CursorType;

private:
	/** Actively ticks the widget to update to process new frame data as long as any frames have been added recently */
	EActiveTimerReturnType EnsureDataUpdateDuringPreview( double InCurrentTime, float InDeltaTime );

	/** True if the active timer is currently registered */
	bool bIsActiveTimerRegistered;
};
