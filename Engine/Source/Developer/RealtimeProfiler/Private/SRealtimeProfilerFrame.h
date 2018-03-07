// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

//
//class SRealtimeProfilerFrame : public SCompoundWidget
//{
//public:
//
//	SLATE_BEGIN_ARGS(SRealtimeProfilerFrame)
//		: _LayoutString()
//		{}
//
//		SLATE_ARGUMENT( FString, LayoutString )
//
//	SLATE_END_ARGS()
//
//	
//	/**
//	 * Constructs this widget
//	 *
//	 * @param InArgs    Declaration from which to construct the widget
//	 */
//	void Construct( const FArguments& InArgs );
//
//	void Update(TSharedPtr< FVisualizerEvent > InProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame);
//
//	bool IsProfiling();
//
//private:
//	
//
//
//	/** Construct main menu */
//	TSharedRef< SWidget > MakeMainMenu();
//
//
//	/** Adds a new bar visualizer tab */
//	void AddVisualizer( TSharedPtr< SDockTabStack > TabStack );
//
//	/** Given a widget, adds it as a tab to the visualizer main frame */
//	void AddTab( TSharedRef<SWidget> InTabContents, TAttribute< FString > InLabel, const FString& InTooltipText, TSharedPtr< SDockTabStack > TabStack );
//
//	/** Callback when a tab is closed to removed its contents from BarVisualizers or EventsVisualizers list */
//	void OnTabClosed( TSharedRef<SDockableTab> ClosedTab, TSharedRef<SWidget> ClosedTabContents );
//
//	/** Pointer to the main dock area of this widget */
//	TSharedPtr< SDockArea > MainDockArea;
//
//	/** Pointer to primary tab stack of this widget */
//	TSharedPtr< SDockTabStack > PrimaryTabStack;
//
//
//	/** List of all visualizers created for this window */
//	TArray< TSharedPtr< SRealtimeProfilerVisualizer > > Visualizers;
//
//	TSharedPtr<SRealtimeProfilerVisualizer> Visualizer;
//};
//
