// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "VisualizerEvents.h"

class SBarVisualizer;
class SEventsTree;
class SRealtimeProfilerTimeline;
class SSplitter;
struct FRealtimeProfilerFPSChartFrame;

class SRealtimeProfilerVisualizer : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SRealtimeProfilerVisualizer)
		: _LayoutString()
		{}

		SLATE_ARGUMENT( FString, LayoutString )

	SLATE_END_ARGS()

	
	/**
	 * Constructs this widget
	 *
	 * @param InArgs    Declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	void Update(TSharedPtr< FVisualizerEvent > InProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame);

	void DisplayFrameDetails(TSharedPtr< FVisualizerEvent > InProfileData);

	bool IsProfiling();

private:

	/** Construct main menu */
	TSharedRef< SWidget > MakeMainMenu();

	/** Routes events from events tree tab to bar visualizer tabs */
	void RouteEventSelectionChanged( TSharedPtr< FVisualizerEvent > Selection );

	/** Routes events from bar visualizer tab to events tree tabs */
	void RouteBarGraphSelectionChanged( TSharedPtr< FVisualizerEvent > Selection );

	/** Routes events from bar visualizer tab to events tree tabs */
	void RouteBarGraphExpansionChanged( TSharedPtr< FVisualizerEvent > Selection );

	/** Routes events from bar visualizer tab to events tree tabs */
	void RouteBarEventSelectionChanged( int32 Thread, TSharedPtr<FVisualizerEvent> Selection );

	/** Opens a context menu when a bar is right clicked */
	void OnBarGraphContextMenu( TSharedPtr< FVisualizerEvent > Selection, const FPointerEvent& InputEvent );

	/** Creates bar visualizer context menu for opening new events tree tabs */
	TSharedRef<SWidget> MakeBarVisualizerContextMenu();

	/** Handles creating a new events tree tab from the bar visualizer context menu */
	void ShowGraphBarInEventsWindow( int32 WindowIndex );

	/** Pointer to the main dock area of this widget */
	TSharedPtr< SSplitter > MainSplitter;

	/** Pointer to the Left dock area of this widget */
	TSharedPtr< SSplitter > LeftSplitter;

	/** Pointer to the Right dock area of this widget */
	TSharedPtr< SSplitter > RightSplitter;

	/** Pointer to the bar visualizer */
	TSharedPtr< SBarVisualizer > BarVisualizer;

	/** Pointer to the events visualizer */
	TSharedPtr< SEventsTree > EventsTree;

	/** Pointer to the timeline visualizer */
	TSharedPtr< SRealtimeProfilerTimeline > Timeline;

	/** Currently selected bar graph */
	TSharedPtr< FVisualizerEvent > SelectedBarGraph;


};

