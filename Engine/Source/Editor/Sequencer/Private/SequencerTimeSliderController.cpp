// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerTimeSliderController.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Misc/Paths.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "SequencerCommonHelpers.h"
#include "SequencerSettings.h"

#define LOCTEXT_NAMESPACE "TimeSlider"

namespace ScrubConstants
{
	/** The minimum amount of pixels between each major ticks on the widget */
	const int32 MinPixelsPerDisplayTick = 12;

	/**The smallest number of units between between major tick marks */
	const float MinDisplayTickSpacing = 0.001f;

	/**The fraction of the current view range to scroll per unit delta  */
	const float ScrollPanFraction = 0.1f;
}

/** Utility struct for converting between scrub range space and local/absolute screen space */
struct FScrubRangeToScreen
{
	FVector2D WidgetSize;

	TRange<float> ViewInput;
	float ViewInputRange;
	float PixelsPerInput;

	FScrubRangeToScreen(TRange<float> InViewInput, const FVector2D& InWidgetSize )
	{
		WidgetSize = InWidgetSize;

		ViewInput = InViewInput;
		ViewInputRange = ViewInput.Size<float>();
		PixelsPerInput = ViewInputRange > 0 ? ( WidgetSize.X / ViewInputRange ) : 0;
	}

	/** Local Widget Space -> Curve Input domain. */
	float LocalXToInput(float ScreenX) const
	{
		float LocalX = ScreenX;
		return (LocalX/PixelsPerInput) + ViewInput.GetLowerBoundValue();
	}

	/** Curve Input domain -> local Widget Space */
	float InputToLocalX(float Input) const
	{
		return (Input - ViewInput.GetLowerBoundValue()) * PixelsPerInput;
	}
};


/**
 * Gets the the next spacing value in the series 
 * to determine a good spacing value
 * E.g, .001,.005,.010,.050,.100,.500,1.000,etc
 */
static float GetNextSpacing( uint32 CurrentStep )
{
	if(CurrentStep & 0x01) 
	{
		// Odd numbers
		return FMath::Pow( 10.f, 0.5f*((float)(CurrentStep-1)) + 1.f );
	}
	else 
	{
		// Even numbers
		return 0.5f * FMath::Pow( 10.f, 0.5f*((float)(CurrentStep)) + 1.f );
	}
}

FSequencerTimeSliderController::FSequencerTimeSliderController( const FTimeSliderArgs& InArgs )
	: TimeSliderArgs( InArgs )
	, DistanceDragged( 0.0f )
	, MouseDragType( DRAG_NONE )
	, bPanning( false )
{
	ScrubHandleUp = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleUp" ) ); 
	ScrubHandleDown = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleDown" ) );
	ScrubHandleSize = 13.f;
	ContextMenuSuppression = 0;
}

float FSequencerTimeSliderController::DetermineOptimalSpacing(float InPixelsPerInput, uint32 MinTick, float MinTickSpacing) const
{
	uint32 CurStep = 0;

	// Start with the smallest spacing
	float Spacing = MinTickSpacing;

	if (InPixelsPerInput > 0)
	{
		while( Spacing * InPixelsPerInput < MinTick )
		{
			Spacing = MinTickSpacing * GetNextSpacing( CurStep );
			CurStep++;
		}
	}

	return Spacing;
}

struct FDrawTickArgs
{
	/** Geometry of the area */
	FGeometry AllottedGeometry;
	/** Culling rect of the area */
	FSlateRect CullingRect;
	/** Color of each tick */
	FLinearColor TickColor;
	/** Offset in Y where to start the tick */
	float TickOffset;
	/** Height in of major ticks */
	float MajorTickHeight;
	/** Start layer for elements */
	int32 StartLayer;
	/** Draw effects to apply */
	ESlateDrawEffect DrawEffects;
	/** Whether or not to only draw major ticks */
	bool bOnlyDrawMajorTicks;
	/** Whether or not to mirror labels */
	bool bMirrorLabels;
	
};

void FSequencerTimeSliderController::DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs ) const
{
	// The math here breaks down when pixels per input is near zero or zero, so just skip drawing ticks to avoid an infinite loop.
	if (FMath::IsNearlyZero(RangeToScreen.PixelsPerInput))
	{
		return;
	}

	float MinDisplayTickSpacing = ScrubConstants::MinDisplayTickSpacing;
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.TimeSnapInterval.Get()) && TimeSliderArgs.TimeSnapInterval.Get())
	{
		MinDisplayTickSpacing = TimeSliderArgs.TimeSnapInterval.Get();
	}

	const float Spacing = DetermineOptimalSpacing( RangeToScreen.PixelsPerInput, ScrubConstants::MinPixelsPerDisplayTick, MinDisplayTickSpacing );

	// Sub divisions
	// @todo Sequencer may need more robust calculation
	const int32 Divider = 10;
	// For slightly larger halfway tick mark
	const int32 HalfDivider = Divider / 2;
	// Find out where to start from
	int32 OffsetNum = FMath::FloorToInt(RangeToScreen.ViewInput.GetLowerBoundValue() / Spacing);
	
	FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );

	TArray<FVector2D> LinePoints;
	LinePoints.AddUninitialized(2);

	float Seconds = 0;
	while( (Seconds = OffsetNum*Spacing) < RangeToScreen.ViewInput.GetUpperBoundValue() )
	{
		// X position local to start of the widget area
		float XPos = RangeToScreen.InputToLocalX( Seconds );
		uint32 AbsOffsetNum = FMath::Abs(OffsetNum);

		if ( AbsOffsetNum % Divider == 0 )
		{
			FVector2D Offset( XPos, InArgs.TickOffset );
			FVector2D TickSize( 0.0f, InArgs.MajorTickHeight );

			LinePoints[0] = FVector2D( 0.0f,1.0f);
			LinePoints[1] = TickSize;

			// lines should not need anti-aliasing
			const bool bAntiAliasLines = false;

			// Draw each tick mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				InArgs.AllottedGeometry.ToPaintGeometry( Offset, TickSize ),
				LinePoints,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAliasLines
				);

			if( !InArgs.bOnlyDrawMajorTicks )
			{
				FString FrameString;
				if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.TimeSnapInterval.Get()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
				{
					FrameString = FString::Printf( TEXT("%d"), TimeToFrame(Seconds));
				}
				else
				{
					FrameString = Spacing == ScrubConstants::MinDisplayTickSpacing ? FString::Printf( TEXT("%.3f"), Seconds ) : FString::Printf( TEXT("%.2f"), Seconds );
				}

				// Space the text between the tick mark but slightly above
				const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);
				FVector2D TextOffset( XPos + 5.f, InArgs.bMirrorLabels ? 3.f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - (InArgs.MajorTickHeight+3.f) ) );
				FSlateDrawElement::MakeText(
					OutDrawElements,
					InArgs.StartLayer+1, 
					InArgs.AllottedGeometry.ToPaintGeometry( TextOffset, TextSize ), 
					FrameString, 
					SmallLayoutFont,
					InArgs.DrawEffects,
					InArgs.TickColor*0.65f 
				);
			}
		}
		else if( !InArgs.bOnlyDrawMajorTicks )
		{
			// Compute the size of each tick mark.  If we are half way between to visible values display a slightly larger tick mark
			const float MinorTickHeight = AbsOffsetNum % HalfDivider == 0 ? 6.0f : 2.0f;

			FVector2D Offset(XPos, InArgs.bMirrorLabels ? 0.0f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - MinorTickHeight ) );
			FVector2D TickSize(0.0f, MinorTickHeight);

			LinePoints[0] = FVector2D(0.0f,1.0f);
			LinePoints[1] = TickSize;

			const bool bAntiAlias = false;
			// Draw each sub mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				InArgs.AllottedGeometry.ToPaintGeometry( Offset, TickSize ),
				LinePoints,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAlias
			);
		}
		// Advance to next tick mark
		++OffsetNum;
	}
}


int32 FSequencerTimeSliderController::OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const bool bEnabled = bParentEnabled;
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	const float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	const float LocalSequenceLength = LocalViewRangeMax-LocalViewRangeMin;
	
	FVector2D Scale = FVector2D(1.0f,1.0f);
	if ( LocalSequenceLength > 0)
	{
		FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

		// draw tick marks
		const float MajorTickHeight = 9.0f;
	
		FDrawTickArgs Args;
		{
			Args.AllottedGeometry = AllottedGeometry;
			Args.bMirrorLabels = bMirrorLabels;
			Args.bOnlyDrawMajorTicks = false;
			Args.TickColor = FLinearColor::White;
			Args.CullingRect = MyCullingRect;
			Args.DrawEffects = DrawEffects;
			Args.StartLayer = LayerId;
			Args.TickOffset = bMirrorLabels ? 0.0f : FMath::Abs( AllottedGeometry.Size.Y - MajorTickHeight );
			Args.MajorTickHeight = MajorTickHeight;
		}

		DrawTicks( OutDrawElements, RangeToScreen, Args );

		// draw playback & selection range
		FPaintPlaybackRangeArgs PlaybackRangeArgs(
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_L"),
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_R"),
			6.f
		);

		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		PlaybackRangeArgs.SolidFillOpacity = 0.05f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		float HalfSize = FMath::CeilToFloat(ScrubHandleSize / 2.0f);

		// Draw the scrub handle
		float XPos = RangeToScreen.InputToLocalX( TimeSliderArgs.ScrubPosition.Get() );
		const int32 ArrowLayer = LayerId + 2;
		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( FVector2D( XPos-HalfSize, 0 ), FVector2D( ScrubHandleSize, AllottedGeometry.Size.Y ) );
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		{
			// @todo Sequencer this color should be specified in the style
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.B *= 0.1f;
			ScrubColor.G *= 0.2f;
		}

		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			ArrowLayer, 
			MyGeometry,
			bMirrorLabels ? ScrubHandleUp : ScrubHandleDown,
			DrawEffects, 
			ScrubColor
		);

		// Draw the current time next to the scrub handle
		float Time = TimeSliderArgs.ScrubPosition.Get();
		FString FrameString;

		if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.TimeSnapInterval.Get()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
		{
			float FrameRate = 1.0f/TimeSliderArgs.TimeSnapInterval.Get();
			float FrameTime = Time * FrameRate;
			int32 Frame = SequencerHelpers::TimeToFrame(Time, FrameRate);
			const float FrameTolerance = 0.001f;

			if (FMath::IsNearlyEqual(FrameTime, (float)Frame, FrameTolerance) || TimeSliderArgs.PlaybackStatus.Get() == EMovieScenePlayerStatus::Playing)
			{
				FrameString = FString::Printf( TEXT("%d"), TimeToFrame(Time));
			}
			else
			{
				FrameString = FString::Printf( TEXT("%.3f"), FrameTime);
			}
		}
		else
		{
			FrameString = FString::Printf( TEXT("%.2f"), Time );
		}

		FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

		// Flip the text position if getting near the end of the view range
		if ((AllottedGeometry.Size.X - XPos) < (TextSize.X + 14.f))
		{
			XPos = XPos - TextSize.X - 12.f;
		}
		else
		{
			XPos = XPos + 10.f;
		}

		FVector2D TextOffset( XPos,  Args.bMirrorLabels ? TextSize.Y-6.f : Args.AllottedGeometry.Size.Y - (Args.MajorTickHeight+TextSize.Y) );

		FSlateDrawElement::MakeText(
			OutDrawElements,
			Args.StartLayer+1, 
			Args.AllottedGeometry.ToPaintGeometry( TextOffset, TextSize ), 
			FrameString, 
			SmallLayoutFont,
			Args.DrawEffects,
			Args.TickColor 
		);
		
		if (MouseDragType == DRAG_SETTING_RANGE)
		{
			float MouseStartPosX = RangeToScreen.InputToLocalX(MouseDownRange[0]);
			float MouseEndPosX = RangeToScreen.InputToLocalX(MouseDownRange[1]);

			float RangePosX = MouseStartPosX < MouseEndPosX ? MouseStartPosX : MouseEndPosX;
			float RangeSizeX = FMath::Abs(MouseStartPosX - MouseEndPosX);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry( FVector2D(RangePosX, 0.f), FVector2D(RangeSizeX, AllottedGeometry.Size.Y) ),
				bMirrorLabels ? ScrubHandleDown : ScrubHandleUp,
				DrawEffects,
				MouseStartPosX < MouseEndPosX ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.25f, 0.3f, 0.3f)
			);
		}

		return ArrowLayer;
	}

	return LayerId;
}


int32 FSequencerTimeSliderController::DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	const TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (!SelectionRange.IsEmpty())
	{
		const float SelectionRangeL = RangeToScreen.InputToLocalX(SelectionRange.GetLowerBoundValue()) - 1;
		const float SelectionRangeR = RangeToScreen.InputToLocalX(SelectionRange.GetUpperBoundValue()) + 1;
		const auto DrawColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

		if (Args.SolidFillOpacity > 0.f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeL, 0.f), FVector2D(SelectionRangeR - SelectionRangeL, AllottedGeometry.Size.Y)),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				DrawColor.CopyWithNewOpacity(Args.SolidFillOpacity)
			);
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.StartBrush,
			ESlateDrawEffect::None,
			DrawColor
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.EndBrush,
			ESlateDrawEffect::None,
			DrawColor
		);
	}

	return LayerId + 1;
}


int32 FSequencerTimeSliderController::DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	if (!TimeSliderArgs.PlaybackRange.IsSet())
	{
		return LayerId;
	}

	TOptional<TRange<float>> SubSequenceRangeValue;
	SubSequenceRangeValue = TimeSliderArgs.SubSequenceRange.Get(SubSequenceRangeValue);

	const uint8 OpacityBlend = SubSequenceRangeValue.IsSet() ? 128 : 255;

	const TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	const float PlaybackRangeL = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue()) - 1;
	const float PlaybackRangeR = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue()) + 1;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.StartBrush,
		ESlateDrawEffect::None,
		FColor(32, 128, 32, OpacityBlend)	// 120, 75, 50 (HSV)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.EndBrush,
		ESlateDrawEffect::None,
		FColor(128, 32, 32, OpacityBlend)	// 0, 75, 50 (HSV)
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0.f, 0.f), FVector2D(PlaybackRangeL, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR, 0.f), FVector2D(AllottedGeometry.Size.X - PlaybackRangeR, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	return LayerId + 1;
}

int32 FSequencerTimeSliderController::DrawSubSequenceRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TOptional<TRange<float>> RangeValue;
	RangeValue = TimeSliderArgs.SubSequenceRange.Get(RangeValue);

	if (!RangeValue.IsSet() || RangeValue->IsEmpty())
	{
		return LayerId;
	}

	const float SubSequenceRangeL = RangeToScreen.InputToLocalX(RangeValue->GetLowerBoundValue()) - 1;
	const float SubSequenceRangeR = RangeToScreen.InputToLocalX(RangeValue->GetUpperBoundValue()) + 1;

	static const FSlateBrush* LineBrushL(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_L"));
	static const FSlateBrush* LineBrushR(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_R"));

	FColor GreenTint(32, 128, 32);	// 120, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		LineBrushL,
		ESlateDrawEffect::None,
		GreenTint
	);

	FColor RedTint(128, 32, 32);	// 0, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		LineBrushR,
		ESlateDrawEffect::None,
		RedTint
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0.f, 0.f), FVector2D(SubSequenceRangeL, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR, 0.f), FVector2D(AllottedGeometry.Size.X - SubSequenceRangeR, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	// Hash applied to the left and right of the sequence bounds
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeL - 16.f, 0.f), FVector2D(16.f, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashL"),
		ESlateDrawEffect::None,
		GreenTint
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR, 0.f), FVector2D(16.f, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashR"),
		ESlateDrawEffect::None,
		RedTint
	);

	return LayerId + 1;
}

FReply FSequencerTimeSliderController::OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	DistanceDragged = 0;

	FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
	MouseDownRange[0] = RangeToScreen.LocalXToInput(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
	MouseDownRange[1] = MouseDownRange[0];

	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && WidgetOwner.HasMouseCapture();
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && WidgetOwner.HasMouseCapture() && TimeSliderArgs.AllowZoom ;

	if ( bHandleRightMouseButton )
	{
		if (!bPanning)
		{
			// Open a context menu if allowed
			if (ContextMenuSuppression == 0 && TimeSliderArgs.PlaybackRange.IsSet())
			{
				FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
				FVector2D CursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

				float MouseValue = RangeToScreen.LocalXToInput( CursorPos.X );
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					MouseValue = SequencerHelpers::SnapTimeToInterval(MouseValue, TimeSliderArgs.TimeSnapInterval.Get());
				}

				TSharedRef<SWidget> MenuContent = OpenSetPlaybackRangeMenu(MouseValue);
				FSlateApplication::Get().PushMenu(
					WidgetOwner.AsShared(),
					MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath(),
					MenuContent,
					MouseEvent.GetScreenSpacePosition(),
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
					);

				return FReply::Handled().SetUserFocus(MenuContent, EFocusCause::SetDirectly).ReleaseMouseCapture();
			}

			// return unhandled in case our parent wants to use our right mouse button to open a context menu
			return FReply::Unhandled().ReleaseMouseCapture();
		}
		
		bPanning = false;
		
		return FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bHandleLeftMouseButton )
	{
		if (MouseDragType == DRAG_PLAYBACK_START)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_PLAYBACK_END)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_START)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_END)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SETTING_RANGE)
		{
			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			float NewValue = RangeToScreen.LocalXToInput(CursorPos.X);

			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
			{
				NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
			}

			float DownValue = MouseDownRange[0];
				
			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
			{
				DownValue = SequencerHelpers::SnapTimeToInterval(DownValue, TimeSliderArgs.TimeSnapInterval.Get());
			}

			// Zoom in
			bool bDoSetRange = false;
			if (NewValue > DownValue)
			{
				// push the current value onto the stack
				RangeStack.Add(FVector2D(TimeSliderArgs.ViewRange.Get().GetLowerBoundValue(), TimeSliderArgs.ViewRange.Get().GetUpperBoundValue()));
				bDoSetRange = true;
			}
			// Zoom out
			else if (RangeStack.Num())
			{
				// pop the stack
				FVector2D LastRange = RangeStack.Pop();
				DownValue = LastRange[0];
				NewValue = LastRange[1];
				bDoSetRange = true;
			}

			if (bDoSetRange)
			{
				TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(TRange<float>(DownValue, NewValue), EViewRangeInterpolation::Immediate);
					
				if( !TimeSliderArgs.ViewRange.IsBound() )
				{	
					// The output is not bound to a delegate so we'll manage the value ourselves
					TimeSliderArgs.ViewRange.Set( TRange<float>( DownValue, NewValue ) );
				}
			}
		}
		else
		{
			TimeSliderArgs.OnEndScrubberMovement.ExecuteIfBound();

			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			float NewValue = RangeToScreen.LocalXToInput(CursorPos.X);

			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
			{				
				if (TimeSliderArgs.Settings->GetSnapPlayTimeToInterval())
				{
					NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
				}
			
				if (MouseDragType == DRAG_SCRUBBING_TIME && TimeSliderArgs.Settings->ShouldKeepCursorInPlayRangeWhileScrubbing())
				{
					TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
					NewValue = FMath::Clamp(NewValue, PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
				}

				if (TimeSliderArgs.Settings->GetSnapPlayTimeToKeys())
				{
					NewValue = SnapTimeToNearestKey(RangeToScreen, CursorPos.X, NewValue);
				}
			}

			CommitScrubPosition( NewValue, /*bIsScrubbing=*/false );
		}

		MouseDragType = DRAG_NONE;
		DistanceDragged = 0.f;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}


FReply FSequencerTimeSliderController::OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton );
	bool bHandleRightMouseButton = MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && TimeSliderArgs.AllowZoom;

	if (bHandleRightMouseButton)
	{
		if (!bPanning)
		{
			DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				bPanning = true;
			}
		}
		else
		{
			TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
			float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
			float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();

			FScrubRangeToScreen ScaleInfo( LocalViewRange, MyGeometry.Size );
			FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X/ScaleInfo.PixelsPerInput;

			float NewViewOutputMin = LocalViewRangeMin - InputDelta.X;
			float NewViewOutputMax = LocalViewRangeMax - InputDelta.X;

			ClampViewRange(NewViewOutputMin, NewViewOutputMax);
			SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Immediate);
		}
	}
	else if (bHandleLeftMouseButton)
	{
		TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
		DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );

		if ( MouseDragType == DRAG_NONE )
		{
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
				const float ScrubPosition = TimeSliderArgs.ScrubPosition.Get();

				TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();
				TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
				float LocalMouseDownPos = RangeToScreen.InputToLocalX(MouseDownRange[0]);
				const bool bIsPlaybackRangeLocked = TimeSliderArgs.IsPlaybackRangeLocked.Get();

				// Disable selection range test if it's empty so that the playback range scrubbing gets priority
				if (!SelectionRange.IsEmpty() && HitTestScrubberEnd(RangeToScreen, SelectionRange, LocalMouseDownPos, ScrubPosition))
				{
					// selection range end scrubber
					MouseDragType = DRAG_SELECTION_END;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!SelectionRange.IsEmpty() && HitTestScrubberStart(RangeToScreen, SelectionRange, LocalMouseDownPos, ScrubPosition))
				{
					// selection range start scrubber
					MouseDragType = DRAG_SELECTION_START;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bIsPlaybackRangeLocked && HitTestScrubberEnd(RangeToScreen, PlaybackRange, LocalMouseDownPos, ScrubPosition))
				{
					// playback range end scrubber
					MouseDragType = DRAG_PLAYBACK_END;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bIsPlaybackRangeLocked && HitTestScrubberStart(RangeToScreen, PlaybackRange, LocalMouseDownPos, ScrubPosition))
				{
					// playback range start scrubber
					MouseDragType = DRAG_PLAYBACK_START;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control))
				{
					MouseDragType = DRAG_SETTING_RANGE;
				}
				else
				{
					MouseDragType = DRAG_SCRUBBING_TIME;
					TimeSliderArgs.OnBeginScrubberMovement.ExecuteIfBound();
				}
			}
		}
		else
		{
			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
			float NewValue = RangeToScreen.LocalXToInput( CursorPos.X );


			// Set the start range time?
			if (MouseDragType == DRAG_PLAYBACK_START)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
				}

				SetPlaybackRangeStart(NewValue);
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_PLAYBACK_END)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
				}
					
				SetPlaybackRangeEnd(NewValue);
			}
			else if (MouseDragType == DRAG_SELECTION_START)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
				}

				SetSelectionRangeStart(NewValue);
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_SELECTION_END)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
				}

				SetSelectionRangeEnd(NewValue);
			}
			else if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
				{
					if (TimeSliderArgs.Settings->GetSnapPlayTimeToInterval())
					{
						NewValue = SequencerHelpers::SnapTimeToInterval(NewValue, TimeSliderArgs.TimeSnapInterval.Get());
					}

					if (TimeSliderArgs.Settings->ShouldKeepCursorInPlayRangeWhileScrubbing())
					{
						TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
						NewValue = FMath::Clamp(NewValue, PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
					}

					if (TimeSliderArgs.Settings->GetSnapPlayTimeToKeys())
					{
						NewValue = SnapTimeToNearestKey(RangeToScreen, CursorPos.X, NewValue);
					}
				}
					
				// Delegate responsibility for clamping to the current viewrange to the client
				CommitScrubPosition( NewValue, /*bIsScrubbing=*/true );
			}
			else if (MouseDragType == DRAG_SETTING_RANGE)
			{
				MouseDownRange[1] = NewValue;
			}
		}
	}

	if ( DistanceDragged != 0.f && (bHandleLeftMouseButton || bHandleRightMouseButton) )
	{
		return FReply::Handled().CaptureMouse(WidgetOwner.AsShared());
	}


	return FReply::Handled();
}


void FSequencerTimeSliderController::CommitScrubPosition( float NewValue, bool bIsScrubbing )
{
	// Manage the scrub position ourselves if its not bound to a delegate
	if ( !TimeSliderArgs.ScrubPosition.IsBound() )
	{
		TimeSliderArgs.ScrubPosition.Set( NewValue );
	}

	TimeSliderArgs.OnScrubPositionChanged.ExecuteIfBound( NewValue, bIsScrubbing );
}

FReply FSequencerTimeSliderController::OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TOptional<TRange<float>> NewTargetRange;

	if ( TimeSliderArgs.AllowZoom && MouseEvent.IsControlDown() )
	{
		float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;

		// If zooming on the current time, adjust mouse fractionX
		if (TimeSliderArgs.Settings->GetZoomPosition() == ESequencerZoomPosition::SZP_CurrentTime)
		{
			const float ScrubPosition = TimeSliderArgs.ScrubPosition.Get();
			if (TimeSliderArgs.ViewRange.Get().Contains(ScrubPosition))
			{
				FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
				float TimePosition = RangeToScreen.InputToLocalX(ScrubPosition);
				MouseFractionX = TimePosition / MyGeometry.GetLocalSize().X;
			}
		}

		const float ZoomDelta = -0.2f * MouseEvent.GetWheelDelta();
		if (ZoomByDelta(ZoomDelta, MouseFractionX))
		{
			return FReply::Handled();
		}
	}
	else if (MouseEvent.IsShiftDown())
	{
		PanByDelta(-MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FCursorReply FSequencerTimeSliderController::OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();
	const float ScrubPosition = TimeSliderArgs.ScrubPosition.Get();
	const bool bIsPlaybackRangeLocked = TimeSliderArgs.IsPlaybackRangeLocked.Get();

	// Use L/R resize cursor if we're dragging or hovering a playback range bound
	float HitTestPosition = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()).X;

	if (MouseDragType == DRAG_SCRUBBING_TIME)
	{
		return FCursorReply::Unhandled();
	}

	if ((MouseDragType == DRAG_PLAYBACK_END) ||
		(MouseDragType == DRAG_PLAYBACK_START) ||
		(MouseDragType == DRAG_SELECTION_START) ||
		(MouseDragType == DRAG_SELECTION_END) ||
		(!bIsPlaybackRangeLocked && HitTestScrubberStart(RangeToScreen, PlaybackRange, HitTestPosition, ScrubPosition)) ||
		(!bIsPlaybackRangeLocked && HitTestScrubberEnd(RangeToScreen, PlaybackRange, HitTestPosition, ScrubPosition)) ||
		(!SelectionRange.IsEmpty() && HitTestScrubberStart(RangeToScreen, SelectionRange, HitTestPosition, ScrubPosition)) ||
		(!SelectionRange.IsEmpty() && HitTestScrubberEnd(RangeToScreen, SelectionRange, HitTestPosition, ScrubPosition)))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

int32 FSequencerTimeSliderController::OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintSectionAreaViewArgs& Args ) const
{
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	float LocalScrubPosition = TimeSliderArgs.ScrubPosition.Get();

	float ViewRange = LocalViewRange.Size<float>();
	float PixelsPerInput = ViewRange > 0 ? AllottedGeometry.Size.X / ViewRange : 0;
	float LinePos =  (LocalScrubPosition - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

	FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

	if (Args.PlaybackRangeArgs.IsSet())
	{
		FPaintPlaybackRangeArgs PaintArgs = Args.PlaybackRangeArgs.GetValue();
		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		PaintArgs.SolidFillOpacity = 0.f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
	}

	if( Args.bDisplayTickLines )
	{
		static FLinearColor TickColor(0.f, 0.f, 0.f, 0.3f);
		
		// Draw major tick lines in the section area
		FDrawTickArgs DrawTickArgs;
		{
			DrawTickArgs.AllottedGeometry = AllottedGeometry;
			DrawTickArgs.bMirrorLabels = false;
			DrawTickArgs.bOnlyDrawMajorTicks = true;
			DrawTickArgs.TickColor = TickColor;
			DrawTickArgs.CullingRect = MyCullingRect;
			DrawTickArgs.DrawEffects = DrawEffects;
			// Draw major ticks under sections
			DrawTickArgs.StartLayer = LayerId-1;
			// Draw the tick the entire height of the section area
			DrawTickArgs.TickOffset = 0.0f;
			DrawTickArgs.MajorTickHeight = AllottedGeometry.Size.Y;
		}

		DrawTicks( OutDrawElements, RangeToScreen, DrawTickArgs );
	}

	if( Args.bDisplayScrubPosition )
	{
		// Draw a line for the scrub position
		TArray<FVector2D> LinePoints;
		{
			LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D( 0.0f, 0.0f );
			LinePoints[1] = FVector2D( 0.0f, FMath::RoundToFloat( AllottedGeometry.Size.Y ) );
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry( FVector2D(LinePos, 0.0f ), FVector2D(1.0f,1.0f) ),
			LinePoints,
			DrawEffects,
			FLinearColor::White,
			false
		);
	}

	return LayerId;
}

TSharedRef<SWidget> FSequencerTimeSliderController::OpenSetPlaybackRangeMenu(float MouseTime)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FText CurrentTimeText;
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.TimeSnapInterval.Get()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
	{
		CurrentTimeText = FText::Format(LOCTEXT("FrameTextFormat", "at frame {0}"), FText::AsNumber(TimeToFrame(MouseTime)));
	}
	else
	{
		CurrentTimeText = FText::Format(LOCTEXT("TimeTextFormat", "at {0}s"), FText::AsNumber(MouseTime));
	}

	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	MenuBuilder.BeginSection("SequencerPlaybackRangeMenu", FText::Format(LOCTEXT("PlaybackRangeTextFormat", "Playback Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackStart", "Set Start Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetPlaybackRangeStart(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && MouseTime <= PlaybackRange.GetUpperBoundValue(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackEnd", "Set End Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetPlaybackRangeEnd(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && MouseTime >= PlaybackRange.GetLowerBoundValue(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleLocked", "Locked"),
			LOCTEXT("ToggleLockedTooltip", "Lock/Unlock the playback range"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] { TimeSliderArgs.OnTogglePlaybackRangeLocked.ExecuteIfBound(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([=] { return TimeSliderArgs.IsPlaybackRangeLocked.Get(); })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();
	MenuBuilder.BeginSection("SequencerSelectionRangeMenu", FText::Format(LOCTEXT("SelectionRangeTextFormat", "Selection Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionStart", "Set Selection Start"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetSelectionRangeStart(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || MouseTime <= SelectionRange.GetUpperBoundValue(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionEnd", "Set Selection End"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetSelectionRangeEnd(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || MouseTime >= SelectionRange.GetLowerBoundValue(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearSelectionRange", "Clear Selection Range"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<float>::Empty()); }),
				FCanExecuteAction::CreateLambda([=]{ return !SelectionRange.IsEmpty(); })
			)
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	return MenuBuilder.MakeWidget();
}

int32 FSequencerTimeSliderController::TimeToFrame(float Time) const
{
	float FrameRate = 1.0f/TimeSliderArgs.TimeSnapInterval.Get();
	return SequencerHelpers::TimeToFrame(Time, FrameRate);
}

float FSequencerTimeSliderController::FrameToTime(int32 Frame) const
{
	float FrameRate = 1.0f/TimeSliderArgs.TimeSnapInterval.Get();
	return SequencerHelpers::FrameToTime(Frame, FrameRate);
}

void FSequencerTimeSliderController::ClampViewRange(float& NewRangeMin, float& NewRangeMax)
{
	bool bNeedsClampSet = false;
	float NewClampRangeMin = TimeSliderArgs.ClampRange.Get().GetLowerBoundValue();
	if ( NewRangeMin < TimeSliderArgs.ClampRange.Get().GetLowerBoundValue() )
	{
		NewClampRangeMin = NewRangeMin;
		bNeedsClampSet = true;
	}

	float NewClampRangeMax = TimeSliderArgs.ClampRange.Get().GetUpperBoundValue();
	if ( NewRangeMax > TimeSliderArgs.ClampRange.Get().GetUpperBoundValue() )
	{
		NewClampRangeMax = NewRangeMax;
		bNeedsClampSet = true;
	}

	if (bNeedsClampSet)
	{
		SetClampRange(NewClampRangeMin, NewClampRangeMax);
	}
}

void FSequencerTimeSliderController::SetViewRange( float NewRangeMin, float NewRangeMax, EViewRangeInterpolation Interpolation )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound( NewRange, Interpolation );

	if( !TimeSliderArgs.ViewRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ViewRange.Set( NewRange );
	}
}

void FSequencerTimeSliderController::SetClampRange( float NewRangeMin, float NewRangeMax )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnClampRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.ClampRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ClampRange.Set(NewRange);
	}
}

void FSequencerTimeSliderController::SetPlayRange( float NewRangeMin, float NewRangeMax )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.PlaybackRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.PlaybackRange.Set(NewRange);
	}
}

bool FSequencerTimeSliderController::ZoomByDelta( float InDelta, float MousePositionFraction )
{
	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();
	float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const float OutputChange = OutputViewSize * InDelta;

	float NewViewOutputMin = LocalViewRangeMin - (OutputChange * MousePositionFraction);
	float NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - MousePositionFraction));

	if( NewViewOutputMin < NewViewOutputMax )
	{
		ClampViewRange(NewViewOutputMin, NewViewOutputMax);
		SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
		return true;
	}

	return false;
}

void FSequencerTimeSliderController::PanByDelta( float InDelta )
{
	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();

	float CurrentMin = LocalViewRange.GetLowerBoundValue();
	float CurrentMax = LocalViewRange.GetUpperBoundValue();

	// Adjust the delta to be a percentage of the current range
	InDelta *= ScrubConstants::ScrollPanFraction * (CurrentMax - CurrentMin);

	float NewViewOutputMin = CurrentMin + InDelta;
	float NewViewOutputMax = CurrentMax + InDelta;

	ClampViewRange(NewViewOutputMin, NewViewOutputMax);
	SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
}


bool FSequencerTimeSliderController::HitTestScrubberStart(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	float LocalPlaybackStartPos = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue());
	float LocalScrubPos = RangeToScreen.InputToLocalX(ScrubPosition);

	// We favor hit testing the scrub bar over hit testing the playback range bounds
	if ((LocalScrubPos - ScrubHandleSize/2.f - MouseTolerance - DragToleranceSlateUnits) < LocalHitPositionX &&
		(LocalScrubPos + ScrubHandleSize/2.f + MouseTolerance + DragToleranceSlateUnits) > LocalHitPositionX)
	{
		return false;
	}

	// Hit test against the brush region to the right of the playback start position, +/- DragToleranceSlateUnits
	return LocalHitPositionX >= LocalPlaybackStartPos - MouseTolerance - DragToleranceSlateUnits &&
		LocalHitPositionX <= LocalPlaybackStartPos + MouseTolerance + BrushSizeInStateUnits + DragToleranceSlateUnits;
}

bool FSequencerTimeSliderController::HitTestScrubberEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	float LocalPlaybackEndPos = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue());
	float LocalScrubPos = RangeToScreen.InputToLocalX(ScrubPosition);
	
	// We favor hit testing the scrub bar over hit testing the playback range bounds
	if ((LocalScrubPos - ScrubHandleSize/2.f - MouseTolerance - DragToleranceSlateUnits) < LocalHitPositionX &&
		(LocalScrubPos + ScrubHandleSize/2.f + MouseTolerance + DragToleranceSlateUnits) > LocalHitPositionX)
	{
		return false;
	}

	// Hit test against the brush region to the left of the playback end position, +/- DragToleranceSlateUnits
	return LocalHitPositionX >= LocalPlaybackEndPos - MouseTolerance - BrushSizeInStateUnits - DragToleranceSlateUnits &&
		LocalHitPositionX <= LocalPlaybackEndPos + MouseTolerance + DragToleranceSlateUnits;
}

float FSequencerTimeSliderController::SnapTimeToNearestKey(const FScrubRangeToScreen& RangeToScreen, float CursorPos, float InTime)
{
	float OutTime = InTime;
	if (TimeSliderArgs.OnGetNearestKey.IsBound())
	{
		float NearestKey = TimeSliderArgs.OnGetNearestKey.Execute(InTime);
		float LocalKeyPos = RangeToScreen.InputToLocalX( NearestKey );
		static float MouseTolerance = 20.f;

		if (FMath::Abs(LocalKeyPos - CursorPos) < MouseTolerance)
		{
			OutTime = NearestKey;
		}
	}

	return OutTime;
}
					
void FSequencerTimeSliderController::SetPlaybackRangeStart(float NewStart)
{
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewStart <= PlaybackRange.GetUpperBoundValue())
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<float>(NewStart, PlaybackRange.GetUpperBoundValue()));
	}
}

void FSequencerTimeSliderController::SetPlaybackRangeEnd(float NewEnd)
{
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewEnd >= PlaybackRange.GetLowerBoundValue())
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<float>(PlaybackRange.GetLowerBoundValue(), NewEnd));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeStart(float NewStart)
{
	TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<float>(NewStart, NewStart + 1));
	}
	else if (NewStart <= SelectionRange.GetUpperBoundValue())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<float>(NewStart, SelectionRange.GetUpperBoundValue()));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeEnd(float NewEnd)
{
	TRange<float> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<float>(NewEnd - 1, NewEnd));
	}
	else if (NewEnd >= SelectionRange.GetLowerBoundValue())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<float>(SelectionRange.GetLowerBoundValue(), NewEnd));
	}
}

#undef LOCTEXT_NAMESPACE
