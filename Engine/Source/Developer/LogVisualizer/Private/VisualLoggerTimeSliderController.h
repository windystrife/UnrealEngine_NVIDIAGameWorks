// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SScrollBar.h"
#include "ITimeSlider.h"

class FSlateWindowElementList;

struct FVisualLoggerTimeSliderArgs : FTimeSliderArgs
{
	FVisualLoggerTimeSliderArgs()
		: CursorSize(0.05f)
	{
		ViewRange = TRange<float>(0.0f, 5.0f);
	}

	/** Cursor range for data like histogram graphs, etc. */
	TAttribute< float > CursorSize;
};

/**
 * A time slider controller for sequencer
 * Draws and manages time data for a Sequencer
 */
class FVisualLoggerTimeSliderController : public ITimeSliderController
{
public:
	FVisualLoggerTimeSliderController(const FVisualLoggerTimeSliderArgs& InArgs);

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
	void SetTimesliderArgs(const FVisualLoggerTimeSliderArgs& InArgs);

	/** ITimeSliderController Interface */
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override { return FCursorReply::Unhandled(); }

	/**
	 * Draws major tick lines in the section view                                                              
	 */
	int32 OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, bool bDisplayTickLines, bool bDisplayScrubPosition ) const;
	FVisualLoggerTimeSliderArgs& GetTimeSliderArgs() { return TimeSliderArgs; }
	/**
	 * Call this method when the user's interaction has changed the scrub position
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bIsScrubbing			True if done via scrubbing, false if just releasing scrubbing
	 */
	void CommitScrubPosition( float NewValue, bool bIsScrubbing );

	void SetExternalScrollbar(TSharedRef<SScrollBar> Scrollbar);
	void SetTimeRange(float MinValue, float MaxValue);
	void SetClampRange(float MinValue, float MaxValue);
	bool IsPanning() { return bPanning; }

private:

	/**
	 * Draws time tick marks
	 *
	 * @param OutDrawElements	List to add draw elements to
	 * @param RangeToScreen		Time range to screen space converter
	 * @param InArgs			Parameters for drawing the tick lines
	 */
	void DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, struct FDrawTickArgs& InArgs ) const;
	void HorizontalScrollBar_OnUserScrolled(float ScrollOffset);
private:
	FVisualLoggerTimeSliderArgs TimeSliderArgs;
	/** Brush for drawing an upwards facing scrub handle */
	const FSlateBrush* ScrubHandleUp;
	/** Brush for drawing a downwards facing scrub handle */
	const FSlateBrush* ScrubHandleDown;
	/** Brush for drawing cursor background to visualize corsor size */
	const FSlateBrush* CursorBackground;
	/** Total mouse delta during dragging **/
	float DistanceDragged;
	/** If we are dragging the scrubber */
	bool bDraggingScrubber;
	/** If we are currently panning the panel */
	bool bPanning;
	/***/
	TSharedPtr<SScrollBar> Scrollbar;
	FVector2D SoftwareCursorPosition;
};
