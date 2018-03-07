// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SRealtimeProfilerVisualizer.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SSplitter.h"
#include "EditorStyleSet.h"
#include "SBarVisualizer.h"
#include "SEventsTree.h"
#include "SRealtimeProfilerTimeline.h"


void SRealtimeProfilerVisualizer::Construct( const FArguments& InArgs )
{
	//TSharedPtr<SDockNode> InitialContent;

	//const FSlateBrush* ContentAreaBrush = FEditorStyle::GetBrush( "Docking.Tab", ".ContentAreaBrush" );

	//ChildSlot
	//[
	//	SAssignNew( MainSplitter, SSplitter )
	//	.Orientation( Orient_Horizontal )

	//	//LEFT PANEL
	//	+ SSplitter::Slot()
	//	.Value(1.0f)
	//	[
	//		SAssignNew( LeftSplitter, SSplitter )
	//		.Orientation( Orient_Vertical )

	//		//TIMELINE
	//		+ SSplitter::Slot()
	//		.Value(1.0f)
	//		[
	//			SNew( SBorder )
	//			.Visibility( EVisibility::Visible )
	//			.BorderImage( ContentAreaBrush )
	//			.Content()
	//			[
	//				SAssignNew( Timeline, SRealtimeProfilerTimeline )
	//				.Visualizer(this)
	//			]
	//		]

	//	]

	//	//RIGHT PANEL
	//	+ SSplitter::Slot()
	//	.Value(1.0f)
	//	[
	//		SAssignNew( RightSplitter, SSplitter )
	//		.Orientation( Orient_Vertical )
	//	]

	//];

	//// Show empty events list.
	//TSharedPtr< FVisualizerEvent > SelectedData(new FVisualizerEvent(0.0, 0.0, 0.0, 0, TEXT("Select Frame")));
	//DisplayFrameDetails(SelectedData);
}

TSharedRef< SWidget > SRealtimeProfilerVisualizer::MakeMainMenu()
{
	FMenuBarBuilder MenuBuilder( NULL );

	// Create the menu bar
	TSharedRef< SWidget > MenuBarWidget = MenuBuilder.MakeWidget();

	return MenuBarWidget;
}

bool SRealtimeProfilerVisualizer::IsProfiling()
{
	return Timeline->IsProfiling();
}


void SRealtimeProfilerVisualizer::RouteEventSelectionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	BarVisualizer->HandleEventSelectionChanged( Selection );
}

void SRealtimeProfilerVisualizer::RouteBarGraphSelectionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	EventsTree->HandleBarGraphSelectionChanged( Selection );
}

void SRealtimeProfilerVisualizer::RouteBarGraphExpansionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	EventsTree->HandleBarGraphExpansionChanged( Selection );
}

void SRealtimeProfilerVisualizer::RouteBarEventSelectionChanged( int32 Thread, TSharedPtr<FVisualizerEvent> Selection )
{
	EventsTree->HandleBarEventSelectionChanged( Thread, Selection );
}

void SRealtimeProfilerVisualizer::OnBarGraphContextMenu( TSharedPtr< FVisualizerEvent > Selection, const FPointerEvent& InputEvent )
{
	SelectedBarGraph = Selection;

	FWidgetPath WidgetPath = InputEvent.GetEventPath() != nullptr ? *InputEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, MakeBarVisualizerContextMenu(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect::ContextMenu);
}

TSharedRef<SWidget> SRealtimeProfilerVisualizer::MakeBarVisualizerContextMenu()
{
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder( bCloseAfterSelection, NULL );
	{
		FUIAction Action( FExecuteAction::CreateSP( this, &SRealtimeProfilerVisualizer::ShowGraphBarInEventsWindow, (int32)INDEX_NONE ) );
		MenuBuilder.AddMenuEntry( NSLOCTEXT("TaskGraph", "GraphBarShowInNew", "Show in New Events Window"), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::Button );
	}
		
	return MenuBuilder.MakeWidget();
}

void SRealtimeProfilerVisualizer::ShowGraphBarInEventsWindow( int32 WindowIndex )
{
	EventsTree->HandleBarGraphExpansionChanged( SelectedBarGraph );
}


void SRealtimeProfilerVisualizer::Update(TSharedPtr< FVisualizerEvent > InProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame)
{
	Timeline->AppendData(InProfileData,InFPSChartFrame);
}

void SRealtimeProfilerVisualizer::DisplayFrameDetails(TSharedPtr< FVisualizerEvent > InProfileData)
{
	while(RightSplitter->GetChildren()->Num() > 0)
	{
		RightSplitter->RemoveAt(0);
	}

	const FSlateBrush* ContentAreaBrush = FEditorStyle::GetBrush( "Docking.Tab", ".ContentAreaBrush" );

	RightSplitter->AddSlot()
		.Value(1.0f)
		[
			SNew( SBorder )
			.Visibility( EVisibility::Visible )
			.BorderImage( ContentAreaBrush )
			[
				SAssignNew( BarVisualizer, SBarVisualizer )
				.ProfileData( InProfileData )
				.OnBarGraphSelectionChanged( this, &SRealtimeProfilerVisualizer::RouteBarGraphSelectionChanged )
				.OnBarGraphExpansionChanged( this, &SRealtimeProfilerVisualizer::RouteBarGraphExpansionChanged )
				.OnBarEventSelectionChanged( this, &SRealtimeProfilerVisualizer::RouteBarEventSelectionChanged )
				.OnBarGraphContextMenu( this, &SRealtimeProfilerVisualizer::OnBarGraphContextMenu )
			]
		];

	RightSplitter->AddSlot()
		.Value(1.0f)
		[
			SNew( SBorder )
			.Visibility( EVisibility::Visible )
			.BorderImage( ContentAreaBrush )
			[
				SAssignNew( EventsTree, SEventsTree )
				.ProfileData( InProfileData )
				.OnEventSelectionChanged( this, &SRealtimeProfilerVisualizer::RouteEventSelectionChanged )
			]
		];

	EventsTree->HandleBarGraphExpansionChanged( InProfileData );
}
