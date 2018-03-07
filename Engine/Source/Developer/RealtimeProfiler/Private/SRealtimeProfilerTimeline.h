// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Geometry.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "VisualizerEvents.h"

class SRealtimeProfilerLineGraph;
class SRealtimeProfilerVisualizer;
class SScrollBar;
class STextBlock;
class STimeline;
struct FRealtimeProfilerFPSChartFrame;

class SRealtimeProfilerTimeline : public SCompoundWidget
{
public:


	SLATE_BEGIN_ARGS( SRealtimeProfilerTimeline )
		: _Visualizer()
		{}

		SLATE_ATTRIBUTE( SRealtimeProfilerVisualizer *, Visualizer )

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Handles selection change in the events tree
	 *
	 * @param Selection selected event
	 */
	void HandleEventSelectionChanged( TSharedPtr< FVisualizerEvent > Selection );


	void AppendData(TSharedPtr< FVisualizerEvent > ProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame);

	bool IsProfiling();

protected:

	/**
	 * Gets the maximum scroll offset fraction value for the horizontal scrollbar
	 *
	 * @return Maximum scroll offset fraction
	 */
	float GetMaxScrollOffsetFraction() const
	{
		return 1.0f - 1.0f / GetZoom();
	}

	/**
	 * Gets the maximum graph offset value for the graph bars
	 *
	 * @return Maximum graph offset
	 */
	float GetMaxGraphOffset() const
	{
		return GetZoom() - 1.0f;
	}

	/**
	 * Gets the actual zoom level for the graph bars
	 *
	 * @return Zoom level
	 */
	float GetZoom() const
	{
		const float MinZoom = 1.0f;
		const float MaxZoom = 20.0f;
		return MinZoom + ZoomSliderValue * ( MaxZoom - MinZoom );
	}

	/**
	 * Callback for scrolling the horizontal scrollbar
	 *
	 * @param InScrollOffsetFraction Scrollbar offset fraction
	 */
	void ScrollBar_OnUserScrolled( float InScrollOffsetFraction );

	/**
	 * Constructs the zoom label string based on the current zoom level value.
	 *
	 * @return Zoom label text
	 */
	FText GetZoomLabel() const;

	/**
	 * Callback used to get the current zoom slider value.
	 *
	 * @return Zoom slider value
	 */
	float GetZoomValue() const;

	/**
	 * Callback used to handle zoom slider
	 *
	 * @param NewValue New Zoom slider value
	 */
	void OnSetZoomValue( float NewValue );

	/** 
	 * Sets the current view mode 
	 *
	 * @param InMode New view mode
	 */
	void SetViewMode( EVisualizerViewMode::Type InMode );

	/** 
	 * Given a view mode checks if it's the currently selected one 
	 *
	 * @param InMode View mode to check
	 */
	bool CheckViewMode( EVisualizerViewMode::Type InMode ) const
	{
		return (ViewMode == InMode);
	}


	/** Called when line graph geometry (size) changes */
	void OnLineGraphGeometryChanged( FGeometry Geometry );

	/** Adjusts timeline to match the selected event's start and duration */
	void AdjustTimeline( TSharedPtr< FVisualizerEvent > InEvent );

	/** A pointer to the SRealtimeProfilerLineGraph */
	TSharedPtr< SRealtimeProfilerLineGraph > LineGraph;

	/** Pointer to the visualizer */
	TAttribute< SRealtimeProfilerVisualizer * > Visualizer;

	/** Profiler data view (filtered data) */
	TArray< TSharedPtr< FVisualizerEvent > > ProfileDataView;

	/** A pointer to the Zoom Label widget */
	TSharedPtr< STextBlock > ZoomLabel;

	/** A pointer to the horizontal scrollbar widget */
	TSharedPtr< SScrollBar > ScrollBar;

	/** A pointer to the horizontal scrollbar widget */
	TSharedPtr< STimeline > Timeline;

	/** Zoom slider value */
	float ZoomSliderValue;

	/** Scrollbar offset */
	float ScrollbarOffset;

	/** Bar visualizer view mode */
	EVisualizerViewMode::Type ViewMode;

};
