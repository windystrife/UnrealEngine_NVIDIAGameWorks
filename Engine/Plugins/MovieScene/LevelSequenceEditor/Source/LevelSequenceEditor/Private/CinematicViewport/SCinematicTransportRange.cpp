// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CinematicViewport/SCinematicTransportRange.h"
#include "Rendering/DrawElements.h"
#include "ISequencerKeyCollection.h"
#include "MovieSceneSequence.h"
#include "EditorStyleSet.h"
#include "Styles/LevelSequenceEditorStyle.h"
#include "MovieScene.h"
#include "ISequencer.h"
#include "CommonMovieSceneTools.h"

#define LOCTEXT_NAMESPACE "SCinematicTransportRange"


void SCinematicTransportRange::Construct(const FArguments& InArgs)
{
	bDraggingTime = false;
}

void SCinematicTransportRange::SetSequencer(TWeakPtr<ISequencer> InSequencer)
{
	WeakSequencer = InSequencer;
}

ISequencer* SCinematicTransportRange::GetSequencer() const
{
	return WeakSequencer.Pin().Get();
}

FVector2D SCinematicTransportRange::ComputeDesiredSize(float) const
{
	static const float MarkerHeight = 6.f;
	static const float TrackHeight = 8.f;
	return FVector2D(100.f, MarkerHeight + TrackHeight);
}

FReply SCinematicTransportRange::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bDraggingTime = true;
	SetTime(MyGeometry, MouseEvent);
		
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
	}
	
	return FReply::Handled().CaptureMouse(AsShared()).PreventThrottling();
}

FReply SCinematicTransportRange::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDraggingTime)
	{
		SetTime(MyGeometry, MouseEvent);
	}

	return FReply::Handled();
}

FReply SCinematicTransportRange::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bDraggingTime = false;

	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	}
	
	return FReply::Handled().ReleaseMouseCapture();
}

void SCinematicTransportRange::SetTime(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		float Lerp = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;
		Lerp = FMath::Clamp(Lerp, 0.f, 1.f);
		
		const TRange<float> WorkingRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;
		
		Sequencer->SetLocalTime(WorkingRange.GetLowerBoundValue() + WorkingRange.Size<float>()*Lerp, ESnapTimeMode::STM_All);
	}
}

void SCinematicTransportRange::OnMouseCaptureLost()
{
	bDraggingTime = false;
}

void SCinematicTransportRange::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		TRange<float> WorkingRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;

		// Anything within 3 pixel's worth of time is a duplicate as far as we're concerned
		FTimeToPixel TimeToPixelConverter(AllottedGeometry, WorkingRange);
		const float DuplicateThreshold = (TimeToPixelConverter.PixelToTime(3.f) - TimeToPixelConverter.PixelToTime(0.f));

		Sequencer->GetKeysFromSelection(ActiveKeyCollection, DuplicateThreshold);
	}
}

int32 SCinematicTransportRange::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	ISequencer* Sequencer = GetSequencer();

	const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	if (!Sequencer)
	{
		return LayerId;
	}

	static const float TrackOffsetY = 6.f;
	const float TrackHeight = AllottedGeometry.GetLocalSize().Y - TrackOffsetY;

	TRange<float> WorkingRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;
	TRange<float> PlaybackRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();

	const float TimePerPixel = WorkingRange.Size<float>() / AllottedGeometry.GetLocalSize().X;

	FColor DarkGray(40, 40, 40);
	FColor MidGray(80, 80, 80);
	FColor LightGray(200, 200, 200);

	// Paint the left padding before the playback start
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D(0.f, TrackOffsetY),  FVector2D(AllottedGeometry.GetLocalSize().X, TrackHeight)),
		FEditorStyle::GetBrush("WhiteBrush"),
		DrawEffects,
		FLinearColor(DarkGray)
	);
	
	const float FullRange = WorkingRange.Size<float>();

	const float PlaybackStartLerp	= (PlaybackRange.GetLowerBoundValue() - WorkingRange.GetLowerBoundValue()) / FullRange;
	const float PlaybackEndLerp		= (PlaybackRange.GetUpperBoundValue() - WorkingRange.GetLowerBoundValue()) / FullRange;

	// Draw the playback range
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D(AllottedGeometry.GetLocalSize().X*PlaybackStartLerp, TrackOffsetY),  FVector2D(AllottedGeometry.GetLocalSize().X*(PlaybackEndLerp - PlaybackStartLerp), TrackHeight)),
		FEditorStyle::GetBrush("WhiteBrush"),
		DrawEffects,
		FLinearColor(MidGray)
	);

	const float CurrentTime = Sequencer->GetLocalTime();
	const float ProgressLerp = (CurrentTime - WorkingRange.GetLowerBoundValue()) / FullRange;

	// Draw the playback progress
	if (ProgressLerp > PlaybackStartLerp)
	{
		const float ClampedProgressLerp = FMath::Clamp(ProgressLerp, PlaybackStartLerp, PlaybackEndLerp);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry( FVector2D(AllottedGeometry.GetLocalSize().X*PlaybackStartLerp, TrackOffsetY), FVector2D(AllottedGeometry.GetLocalSize().X * (ClampedProgressLerp - PlaybackStartLerp), TrackHeight) ),
			FEditorStyle::GetBrush("WhiteBrush"),
			DrawEffects,
			FLinearColor(LightGray)
		);
	}

	bool bPlayMarkerOnKey = false;

	const FLinearColor KeyFrameColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

	// Draw the current key collection tick marks
	{
		static const float BrushWidth = 7.f;
		static const float BrushHeight = 7.f;

		if (ActiveKeyCollection.IsValid())
		{
			const float BrushOffsetY = TrackOffsetY + TrackHeight * .5f - BrushHeight * .5f;
			const FSlateBrush* KeyBrush = FLevelSequenceEditorStyle::Get()->GetBrush("LevelSequenceEditor.CinematicViewportTransportRangeKey");

			ActiveKeyCollection->IterateKeys([&](float Time){
				if (Time < WorkingRange.GetLowerBoundValue() || Time > WorkingRange.GetUpperBoundValue())
				{
					// continue iteration
					return true;
				}

				if (FMath::IsNearlyEqual(CurrentTime, Time, TimePerPixel*.5f))
				{
					bPlayMarkerOnKey = true;
				}

				float Lerp = (Time - WorkingRange.GetLowerBoundValue()) / FullRange;

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId+2,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(BrushWidth, BrushHeight),
						FSlateLayoutTransform(FVector2D(AllottedGeometry.GetLocalSize().X*Lerp - BrushWidth*.5f, BrushOffsetY))
					),
					KeyBrush,
					DrawEffects,
					KeyFrameColor
				);

				// continue iteration
				return true;
			});
		}
	}

	// Draw the play marker
	{
		static const float BrushWidth = 11.f, BrushHeight = 6.f;
		const float PositionX = AllottedGeometry.GetLocalSize().X * ProgressLerp;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(PositionX - FMath::CeilToFloat(BrushWidth/2), 0.f), FVector2D(BrushWidth, BrushHeight)),
			FLevelSequenceEditorStyle::Get()->GetBrush("LevelSequenceEditor.CinematicViewportPlayMarker"),
			DrawEffects,
			bPlayMarkerOnKey ? KeyFrameColor : LightGray
		);

		if (!bPlayMarkerOnKey)
		{
			TArray<FVector2D> LinePoints;
			LinePoints.Add(FVector2D(PositionX, BrushHeight));
			LinePoints.Add(FVector2D(PositionX, AllottedGeometry.GetLocalSize().Y));

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				DrawEffects,
				LightGray,
				false
			);
		}
	}

	// Draw the play bounds
	{
		static const float BrushWidth = 4.f;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize().X*PlaybackStartLerp, TrackOffsetY), FVector2D(BrushWidth, TrackHeight)),
			FLevelSequenceEditorStyle::Get()->GetBrush("LevelSequenceEditor.CinematicViewportRangeStart"),
			DrawEffects,
			FColor(32, 128, 32)	// 120, 75, 50 (HSV)
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize().X*PlaybackEndLerp - BrushWidth, TrackOffsetY), FVector2D(BrushWidth, TrackHeight)),
			FLevelSequenceEditorStyle::Get()->GetBrush("LevelSequenceEditor.CinematicViewportRangeEnd"),
			DrawEffects,
			FColor(128, 32, 32)	// 0, 75, 50 (HSV)
		);
	}

	return LayerId;
}

#undef LOCTEXT_NAMESPACE
