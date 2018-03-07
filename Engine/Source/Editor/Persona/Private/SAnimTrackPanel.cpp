// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimTrackPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "AnimTrackPanel"

//////////////////////////////////////////////////////////////////////////
// S2ColumnWidget

void S2ColumnWidget::Construct(const FArguments& InArgs)
{
	this->ChildSlot
		[
			SNew( SBorder )
			.Padding( FMargin(2.f, 2.f) )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1)
				[
					SAssignNew(LeftColumn, SVerticalBox)
				]

				+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						SNew( SBox )
						.WidthOverride(InArgs._WidgetWidth)
						.HAlign(HAlign_Center)
						[
							SAssignNew(RightColumn,SVerticalBox)
						]
					]
			]
		];
}

//////////////////////////////////////////////////////////////////////////
// SAnimTrackPanel

void SAnimTrackPanel::Construct(const FArguments& InArgs)
{
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	InputMin = InArgs._InputMin;
	InputMax = InArgs._InputMax;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	WidgetWidth = InArgs._WidgetWidth;

	bPanning = false;
	PanningDistance = 0.f;
}

TSharedRef<class S2ColumnWidget> SAnimTrackPanel::Create2ColumnWidget( TSharedRef<SVerticalBox> Parent )
{
	TSharedPtr<S2ColumnWidget> NewTrack;
	Parent->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew (NewTrack, S2ColumnWidget)
			.WidgetWidth(WidgetWidth)
		];

	return NewTrack.ToSharedRef();
}

FReply SAnimTrackPanel::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const float ZoomDelta = -0.1f * MouseEvent.GetWheelDelta();

	const FVector2D MouseWidgetPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float ZoomRatio = FMath::Clamp((MouseWidgetPos.X / (MyGeometry.Size.X - WidgetWidth)), 0.f, 1.f);

	{
		const float InputViewSize = ViewInputMax.Get() - ViewInputMin.Get();
		const float InputChange = InputViewSize * ZoomDelta;

		float ViewMinInput = ViewInputMin.Get() - (InputChange * ZoomRatio);
		float ViewMaxInput = ViewInputMax.Get() + (InputChange * (1.f - ZoomRatio));
		
		InputViewRangeChanged(ViewMinInput, ViewMaxInput);
	}

	return FReply::Handled();
}

FReply SAnimTrackPanel::OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	const bool bRightMouseButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	if(bRightMouseButton)
	{
		UE_LOG(LogAnimation, Log, TEXT("MouseButtonDown %d, %0.5f"), bPanning, PanningDistance);
		PanningDistance = 0.f;
	}

	return FReply::Unhandled();
}

FReply SAnimTrackPanel::OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	const bool bRightMouseButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	if(bRightMouseButton && this->HasMouseCapture())
	{
		bPanning = false;
		PanningDistance = 0.f;
		UE_LOG(LogAnimation, Log, TEXT("MouseButtonUp (Releasing Mouse) %d, %0.5f"), bPanning, PanningDistance);
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SAnimTrackPanel::OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	const bool bRightMouseButtonDown = InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);

	// When mouse moves, if we are moving a key, update its 'input' position
	if(bRightMouseButtonDown)
	{
		if( !bPanning )
		{
			PanningDistance += FMath::Abs(InMouseEvent.GetCursorDelta().X);
			if ( PanningDistance > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				bPanning = true;
				UE_LOG(LogAnimation, Log, TEXT("MouseMove (Capturing Mouse) %d, %0.5f"), bPanning, PanningDistance);
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
		else
		{
			FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, InMyGeometry.Size);
			FVector2D ScreenDelta = InMouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X/ScaleInfo.PixelsPerInput;
			InputDelta.Y = -ScreenDelta.Y/ScaleInfo.PixelsPerOutput;

			float NewViewInputMin = ViewInputMin.Get() - InputDelta.X;
			float NewViewInputMax = ViewInputMax.Get() - InputDelta.X;
			// we'd like to keep  the range if outside when panning
			if ( NewViewInputMin < InputMin.Get() )
			{
				NewViewInputMin = InputMin.Get();
				NewViewInputMax = ScaleInfo.ViewInputRange;
			}
			else if ( NewViewInputMax > InputMax.Get() )
			{
				NewViewInputMax = InputMax.Get();
				NewViewInputMin = NewViewInputMax - ScaleInfo.ViewInputRange;
			}

			InputViewRangeChanged(NewViewInputMin, NewViewInputMax);

			UE_LOG(LogAnimation, Log, TEXT("MouseMove (Panning) %0.2f, %0.2f"), ViewInputMin.Get(), ViewInputMax.Get());
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SAnimTrackPanel::PanInputViewRange(int32 ScreenDelta, FVector2D ScreenViewSize)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, ScreenViewSize);

	float InputDeltaX = ScreenDelta/ScaleInfo.PixelsPerInput;

	float NewViewInputMin = ViewInputMin.Get() + InputDeltaX;
	float NewViewInputMax = ViewInputMax.Get() + InputDeltaX;

	// we'd like to keep  the range if outside when panning
	float SequenceLength = GetSequenceLength();
	if ( NewViewInputMin < 0.f )
	{
		NewViewInputMin = 0.f;
		NewViewInputMax = ScaleInfo.ViewInputRange;
	}
	else if ( NewViewInputMax >SequenceLength )
	{
		NewViewInputMax = SequenceLength;
		NewViewInputMin = NewViewInputMax - ScaleInfo.ViewInputRange;
	}

	InputViewRangeChanged(NewViewInputMin, NewViewInputMax);
}

void SAnimTrackPanel::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	OnSetInputViewRange.Execute(ViewMin, ViewMax);
}

#undef LOCTEXT_NAMESPACE
