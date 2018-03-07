// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SRealtimeProfilerFrame.h"



/*	
void SRealtimeProfilerFrame::Construct( const FArguments& InArgs )
{
	TSharedPtr<SDockNode> InitialContent;
	InitialContent = SAssignNew( PrimaryTabStack, SDockTabStack ).IsDocumentArea(true);

	ChildSlot
	[
		SAssignNew( MainDockArea, SDockArea )
			.InitialContent
			(
				InitialContent
			)
			.InlineContentLeft
			(
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
					.FillHeight(1)
					.VAlign(VAlign_Center)
				[
					MakeMainMenu()
				]
			)
	];

	// Create initial layout
	AddVisualizer( NULL );
}

bool SRealtimeProfilerFrame::IsProfiling()
{
	return Visualizer->IsProfiling();
}


TSharedRef< SWidget > SRealtimeProfilerFrame::MakeMainMenu()
{
	FMenuBarBuilder MenuBuilder( NULL );

	// Create the menu bar
	TSharedRef< SWidget > MenuBarWidget = MenuBuilder.MakeWidget();

	return MenuBarWidget;
}

void SRealtimeProfilerFrame::AddVisualizer( TSharedPtr< SDockTabStack > TabStack )
{
	TSharedRef<SRealtimeProfilerVisualizer> NewVisualizer = SNew( SRealtimeProfilerVisualizer );

	Visualizers.Add( NewVisualizer );

	AddTab( NewVisualizer, "Stats", NSLOCTEXT("TaskGraph", "ProfileVisualizerToolTip", "Profile Visualizer.").ToString(), TabStack );


	Visualizer = NewVisualizer;
}

void SRealtimeProfilerFrame::Update(TSharedPtr< FVisualizerEvent > InProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame)
{
	Visualizer->Update(InProfileData,InFPSChartFrame);
}

void SRealtimeProfilerFrame::AddTab( TSharedRef<SWidget> InTabContents, TAttribute< FString > InLabel, const FString& InTooltipText, TSharedPtr< SDockTabStack > TabStack )
{
	TSharedRef<SDockableTab> NewDockTab = SNew( SDockableTab )
					.TabRole( ETabRole::MajorTab )
					.Label( InLabel )
					.ToolTipText( InTooltipText )
					.OnTabClosed( SDockableTab::FOnTabClosedCallback::CreateSP( this, &SRealtimeProfilerFrame::OnTabClosed, InTabContents ) )
					[
						InTabContents
					];

	if( TabStack.IsValid() == false )
	{
		TabStack = PrimaryTabStack;
	}

	TabStack->AddTab( NewDockTab );
}

void SRealtimeProfilerFrame::OnTabClosed( TSharedRef<SDockableTab> ClosedTab, TSharedRef<SWidget> ClosedTabContents  )
{
	for( int32 Index = 0; Index < Visualizers.Num(); Index++ )
	{
		if( Visualizers[ Index ] == ClosedTabContents )
		{
			Visualizers.RemoveAt( Index );
			return;
		}
	}
}
*/
