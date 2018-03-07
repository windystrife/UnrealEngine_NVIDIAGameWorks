// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SRealtimeProfilerTimeline.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SSlider.h"
#include "STimeline.h"

#include "SRealtimeProfilerLineGraph.h"
#include "RealtimeProfiler.h"

void SRealtimeProfilerTimeline::Construct( const FArguments& InArgs )
{
	ZoomSliderValue = 0.0f;
	ScrollbarOffset = 0.0f;
	Visualizer = InArgs._Visualizer;
	ViewMode = EVisualizerViewMode::Hierarchical;


	this->ChildSlot
	[
		SNew( SVerticalBox )

		+SVerticalBox::Slot() .Padding( 2 ) .FillHeight( 1 ) .VAlign( VAlign_Fill )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth() .Padding( 2 ) .FillWidth( 1 ) .HAlign( HAlign_Fill )
			[
				SAssignNew( LineGraph, SRealtimeProfilerLineGraph )
				.OnGeometryChanged( this, &SRealtimeProfilerTimeline::OnLineGraphGeometryChanged )
				.Visualizer(InArgs._Visualizer)
			]
		]

		+SVerticalBox::Slot().AutoHeight() .Padding( 2 ) .VAlign( VAlign_Fill )
		[
			SAssignNew( Timeline, STimeline )
			.MinValue(0.0f)
			.MaxValue(200.0f)
		]
		+SVerticalBox::Slot().AutoHeight() .Padding( 2 ) .VAlign( VAlign_Fill )
		[
			SAssignNew( ScrollBar, SScrollBar )
			.Orientation( Orient_Horizontal )
			.OnUserScrolled( this, &SRealtimeProfilerTimeline::ScrollBar_OnUserScrolled )
		]
		+SVerticalBox::Slot().AutoHeight() .Padding( 2 ) .VAlign( VAlign_Fill )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 2 )
			.FillWidth( 1 )
			.HAlign( HAlign_Fill )
			[
				SAssignNew( ZoomLabel, STextBlock ).Text( this, &SRealtimeProfilerTimeline::GetZoomLabel )
			]
			+SHorizontalBox::Slot()
			.Padding( 2 )
			.FillWidth( 5 )
			.HAlign( HAlign_Fill )
			[
				SNew( SSlider )
				.Value( this, &SRealtimeProfilerTimeline::GetZoomValue )
				.OnValueChanged( this, &SRealtimeProfilerTimeline::OnSetZoomValue )
			]
		]
	];


	ScrollBar->SetState(0.0f, 1.0f);
}

void SRealtimeProfilerTimeline::AppendData(TSharedPtr< FVisualizerEvent > ProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame)
{
	LineGraph->AppendData(ProfileData,InFPSChartFrame);
}


bool SRealtimeProfilerTimeline::IsProfiling()
{
	return LineGraph->bIsProfiling;
}

void SRealtimeProfilerTimeline::ScrollBar_OnUserScrolled( float InScrollOffsetFraction )
{
	if( ZoomSliderValue > 0.0f )
	{
		const float MaxOffset = GetMaxScrollOffsetFraction();
		const float MaxGraphOffset = GetMaxGraphOffset();
		InScrollOffsetFraction = FMath::Clamp( InScrollOffsetFraction, 0.0f, MaxOffset );
		float GraphOffset = -( InScrollOffsetFraction / MaxOffset ) * MaxGraphOffset;

		ScrollBar->SetState( InScrollOffsetFraction, 1.0f / GetZoom() );

		LineGraph->SetOffset(GraphOffset);

		Timeline->SetOffset( GraphOffset );

		ScrollbarOffset = GraphOffset;
	}
}

FText SRealtimeProfilerTimeline::GetZoomLabel() const
{
	static const FNumberFormattingOptions ZoomFormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::Format( NSLOCTEXT("TaskGraph", "ZoomLabelFmt", "Zoom: {0}x"), FText::AsNumber(GetZoom(), &ZoomFormatOptions) );
}

float SRealtimeProfilerTimeline::GetZoomValue() const
{
	return ZoomSliderValue;
}

void SRealtimeProfilerTimeline::OnSetZoomValue( float NewValue )
{
	const float PrevZoom = GetZoom();
	const float PrevVisibleRange = 1.0f / PrevZoom;
		
	ZoomSliderValue = NewValue;
	const float Zoom = GetZoom();

	float GraphOffset = 0.0f;
	float ScrollOffsetFraction = 0.0f;

	const float MaxOffset = GetMaxScrollOffsetFraction();
	const float MaxGraphOffset = GetMaxGraphOffset();
	const float PrevGraphOffset = -LineGraph->GetOffset();
	GraphOffset = FMath::Clamp( -LineGraph->GetOffset(), 0.0f, MaxGraphOffset );
			
	const float VisibleRange = 1.0f / GetZoom();
	const float PrevGraphCenterValue = PrevGraphOffset / PrevZoom + PrevVisibleRange * 0.5f;
	const float NewGraphCenterValue = GraphOffset / Zoom + VisibleRange * 0.5f;
	GraphOffset += ( PrevGraphCenterValue - NewGraphCenterValue ) * Zoom;
	GraphOffset = FMath::Clamp( GraphOffset, 0.0f, MaxGraphOffset );

	ScrollOffsetFraction = FMath::Clamp( MaxOffset * GraphOffset / MaxGraphOffset, 0.0f, MaxOffset );

	ScrollBar->SetState( ScrollOffsetFraction, 1.0f / Zoom );

	LineGraph->SetZoom( Zoom );
	LineGraph->SetOffset( -GraphOffset );

	Timeline->SetZoom( Zoom );
	Timeline->SetOffset( -GraphOffset );

	ScrollbarOffset = -GraphOffset;
}

void SRealtimeProfilerTimeline::OnLineGraphGeometryChanged( FGeometry Geometry )
{
	Timeline->SetDrawingGeometry( Geometry );
}


void SRealtimeProfilerTimeline::AdjustTimeline( TSharedPtr< FVisualizerEvent > InEvent )
{
	check( InEvent.IsValid() );

	const double TotalTimeMs = InEvent->DurationMs / InEvent->Duration;
	const double StartMs = InEvent->Start * TotalTimeMs;
	Timeline->SetMinMaxValues( StartMs, StartMs + InEvent->DurationMs );
}
