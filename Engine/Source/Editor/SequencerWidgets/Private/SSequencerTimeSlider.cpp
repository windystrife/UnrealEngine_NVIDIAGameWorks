// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTimeSlider.h"

#define LOCTEXT_NAMESPACE "STimeSlider"


void SSequencerTimeSlider::Construct( const SSequencerTimeSlider::FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController )
{
	TimeSliderController = InTimeSliderController;
	bMirrorLabels = InArgs._MirrorLabels;
}

int32 SSequencerTimeSlider::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayer = TimeSliderController->OnPaintTimeSlider( bMirrorLabels, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	return FMath::Max( NewLayer, SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NewLayer, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) ) );
}

FReply SSequencerTimeSlider::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TimeSliderController->OnMouseButtonDown( *this, MyGeometry, MouseEvent );
	return FReply::Handled().CaptureMouse(AsShared()).PreventThrottling();
}

FReply SSequencerTimeSlider::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return TimeSliderController->OnMouseButtonUp( *this,  MyGeometry, MouseEvent );
}

FReply SSequencerTimeSlider::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return TimeSliderController->OnMouseMove( *this, MyGeometry, MouseEvent );
}

FVector2D SSequencerTimeSlider::ComputeDesiredSize( float ) const
{
	return FVector2D(100, 22);
}

FReply SSequencerTimeSlider::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return TimeSliderController->OnMouseWheel( *this, MyGeometry, MouseEvent );
}

FCursorReply SSequencerTimeSlider::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return TimeSliderController->OnCursorQuery( SharedThis(this), MyGeometry, CursorEvent );
}

#undef LOCTEXT_NAMESPACE
