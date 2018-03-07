// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "ITimeSlider.h"

class FSlateWindowElementList;
struct FContextMenuSuppressor;
struct FScrubRangeToScreen;
struct FSlateBrush;

struct FPaintPlaybackRangeArgs
{
	FPaintPlaybackRangeArgs()
		: StartBrush(nullptr), EndBrush(nullptr), BrushWidth(0.f), SolidFillOpacity(0.f)
	{}

	FPaintPlaybackRangeArgs(const FSlateBrush* InStartBrush, const FSlateBrush* InEndBrush, float InBrushWidth)
		: StartBrush(InStartBrush), EndBrush(InEndBrush), BrushWidth(InBrushWidth)
	{}
	/** Brush to use for the start bound */
	const FSlateBrush* StartBrush;
	/** Brush to use for the end bound */
	const FSlateBrush* EndBrush;
	/** The width of the above brushes, in slate units */
	float BrushWidth;
	/** level of opacity for the fill color between the range markers */
	float SolidFillOpacity;
};

struct FPaintSectionAreaViewArgs
{
	FPaintSectionAreaViewArgs()
		: bDisplayTickLines(false), bDisplayScrubPosition(false)
	{}

	/** Whether to display tick lines */
	bool bDisplayTickLines;
	/** Whether to display the scrub position */
	bool bDisplayScrubPosition;
	/** Optional Paint args for the playback range*/
	TOptional<FPaintPlaybackRangeArgs> PlaybackRangeArgs;
};

/**
 * A time slider controller for sequencer
 * Draws and manages time data for a Sequencer
 */
class FSequencerTimeSliderController : public ITimeSliderController
{
public:
	FSequencerTimeSliderController( const FTimeSliderArgs& InArgs );

	/**
	* Determines the optimal spacing between tick marks in the slider for a given pixel density
	* Increments until a minimum amount of slate units specified by MinTick is reached
	*
	* @param InPixelsPerInput	The density of pixels between each input
	* @param MinTick			The minimum slate units per tick allowed
	* @param MinTickSpacing	The minimum tick spacing in time units allowed
	* @return the optimal spacing in time units
	*/
	float DetermineOptimalSpacing(float InPixelsPerInput, uint32 MinTick, float MinTickSpacing) const;

	/** ITimeSliderController Interface */
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	/** End ITimeSliderController Interface */

	/** Get the current view range for this controller */
	virtual FAnimatedRange GetViewRange() const override { return TimeSliderArgs.ViewRange.Get(); }

	/** Get the current clamp range for this controller */
	virtual FAnimatedRange GetClampRange() const override { return TimeSliderArgs.ClampRange.Get(); }

	/** Get the current play range for this controller */
	virtual TRange<float> GetPlayRange() const override { return TimeSliderArgs.PlaybackRange.Get(TRange<float>()); }

	/** Convert time to frame */
	virtual int32 TimeToFrame(float Time) const override;

	/** Convert time to frame */
	virtual float FrameToTime(int32 Frame) const override;

	/**
	 * Clamp the given range to the clamp range 
	 *
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 */	
	void ClampViewRange(float& NewRangeMin, float& NewRangeMax);

	/**
	 * Set a new range based on a min, max and an interpolation mode
	 * 
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 * @param Interpolation		How to set the new range (either immediately, or animated)
	 */
	virtual void SetViewRange( float NewRangeMin, float NewRangeMax, EViewRangeInterpolation Interpolation ) override;

	/**
	 * Set a new clamp range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the clamp range
	 * @param NewRangeMax		The new upper bound of the clamp range
	 */
	virtual void SetClampRange( float NewRangeMin, float NewRangeMax ) override;

	/**
	 * Set a new playback range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the playback range
	 * @param NewRangeMax		The new upper bound of the playback range
	 */
	virtual void SetPlayRange( float NewRangeMin, float NewRangeMax ) override;

	/**
	 * Zoom the range by a given delta.
	 * 
	 * @param InDelta		The total amount to zoom by (+ve = zoom out, -ve = zoom in)
	 * @param ZoomBias		Bias to apply to lower/upper extents of the range. (0 = lower, 0.5 = equal, 1 = upper)
	 */
	bool ZoomByDelta( float InDelta, float ZoomBias = 0.5f );

	/**
	 * Pan the range by a given delta
	 * 
	 * @param InDelta		The total amount to pan by (+ve = pan forwards in time, -ve = pan backwards in time)
	 */
	void PanByDelta( float InDelta );

	/**
	 * Draws major tick lines in the section view                                                              
	 */
	int32 OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintSectionAreaViewArgs& Args ) const;

private:
	/**
	 * Call this method when the user's interaction has changed the scrub position
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bIsScrubbing			True if done via scrubbing, false if just releasing scrubbing
	 */
	void CommitScrubPosition( float NewValue, bool bIsScrubbing );

	/**
	 * Draw time tick marks
	 *
	 * @param OutDrawElements	List to add draw elements to
	 * @param RangeToScreen		Time range to screen space converter
	 * @param InArgs			Parameters for drawing the tick lines
	 */
	void DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, struct FDrawTickArgs& InArgs ) const;

	/**
	 * Draw the selection range.
	 *
	 * @return The new layer ID.
	 */
	int32 DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

	/**
	 * Draw the playback range.
	 *
	 * @return the new layer ID
	 */
	int32 DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

	/**
	 * Draw the playback range.
	 *
	 * @return the new layer ID
	 */
	int32 DrawSubSequenceRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;
	
private:

	/**
	 * Hit test the lower bound of the playback range
	 */
	bool HitTestScrubberStart(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const;

	/**
	 * Hit test the upper bound of the playback range
	 */
	bool HitTestScrubberEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const;

	float SnapTimeToNearestKey(const FScrubRangeToScreen& RangeToScreen, float CursorPos, float InTime);

	void SetPlaybackRangeStart(float NewStart);
	void SetPlaybackRangeEnd(float NewEnd);

	void SetSelectionRangeStart(float NewStart);
	void SetSelectionRangeEnd(float NewEnd);

	TSharedRef<SWidget> OpenSetPlaybackRangeMenu(float MouseTime);

private:
	FTimeSliderArgs TimeSliderArgs;

	/** The size of the scrub handle */
	float ScrubHandleSize;
	
	/** Brush for drawing an upwards facing scrub handle */
	const FSlateBrush* ScrubHandleUp;
	
	/** Brush for drawing a downwards facing scrub handle */
	const FSlateBrush* ScrubHandleDown;
	
	/** Total mouse delta during dragging **/
	float DistanceDragged;
	
	/** If we are dragging a scrubber or dragging to set the time range */
	enum DragType
	{
		DRAG_SCRUBBING_TIME,
		DRAG_SETTING_RANGE,
		DRAG_PLAYBACK_START,
		DRAG_PLAYBACK_END,
		DRAG_SELECTION_START,
		DRAG_SELECTION_END,
		DRAG_NONE
	};
	
	DragType MouseDragType;
	
	/** If we are currently panning the panel */
	bool bPanning;
	
	/** Mouse down time range */
	FVector2D MouseDownRange;
	
	/** Range stack */
	TArray<FVector2D> RangeStack;

	/** When > 0, we should not show context menus */
	int32 ContextMenuSuppression;
	
	friend FContextMenuSuppressor;
};

struct FContextMenuSuppressor
{
	FContextMenuSuppressor(TSharedRef<FSequencerTimeSliderController> InTimeSliderController)
		: TimeSliderController(InTimeSliderController)
	{
		++TimeSliderController->ContextMenuSuppression;
	}
	~FContextMenuSuppressor()
	{
		--TimeSliderController->ContextMenuSuppression;
	}

private:
	FContextMenuSuppressor(const FContextMenuSuppressor&);
	FContextMenuSuppressor& operator=(const FContextMenuSuppressor&);

	TSharedRef<FSequencerTimeSliderController> TimeSliderController;
};
