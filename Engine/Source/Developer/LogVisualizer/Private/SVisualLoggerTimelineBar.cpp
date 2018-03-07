// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SVisualLoggerTimelineBar.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "VisualLoggerDatabase.h"
#include "LogVisualizerStyle.h"
#include "LogVisualizerPrivate.h"
#include "VisualLoggerTimeSliderController.h"

FReply SVisualLoggerTimelineBar::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TimelineOwner.Pin()->OnMouseButtonDown(MyGeometry, MouseEvent);
	FReply Replay = TimeSliderController->OnMouseButtonDown(*this, MyGeometry, MouseEvent);
	if (Replay.IsEventHandled())
	{
		FName RowName = TimelineOwner.Pin()->GetName();
		FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
		const float ScrubPosition = TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get();
		const int32 ClosestItem = DBRow.GetClosestItem(ScrubPosition);
		const auto& Items = DBRow.GetItems();
		if (Items.IsValidIndex(ClosestItem))
		{
			TimeSliderController->CommitScrubPosition(Items[ClosestItem].Entry.TimeStamp, false);
		}
	}
	return Replay;
}

FReply SVisualLoggerTimelineBar::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TimelineOwner.Pin()->OnMouseButtonUp(MyGeometry, MouseEvent);

	FReply  Replay = TimeSliderController->OnMouseButtonUp(*this, MyGeometry, MouseEvent);
	if (Replay.IsEventHandled())
	{
		FName RowName = TimelineOwner.Pin()->GetName();
		FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
		const float ScrubPosition = TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get();
		const int32 ClosestItem = DBRow.GetClosestItem(ScrubPosition);
		const auto& Items = DBRow.GetItems();
		if (Items.IsValidIndex(ClosestItem))
		{
			TimeSliderController->CommitScrubPosition(Items[ClosestItem].Entry.TimeStamp, false);
		}
	}
	return Replay;
}

FReply SVisualLoggerTimelineBar::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TimeSliderController->OnMouseMove(*this, MyGeometry, MouseEvent);
}

FReply SVisualLoggerTimelineBar::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FName RowName = TimelineOwner.Pin()->GetName();
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (World && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FLogVisualizer::Get().UpdateCameraPosition(RowName, DBRow.GetCurrentItemIndex());
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

SVisualLoggerTimelineBar::~SVisualLoggerTimelineBar()
{
}

void SVisualLoggerTimelineBar::Construct(const FArguments& InArgs, TSharedPtr<FVisualLoggerTimeSliderController> InTimeSliderController, TSharedPtr<SLogVisualizerTimeline> InTimelineOwner)
{
	TimeSliderController = InTimeSliderController;
	TimelineOwner = InTimelineOwner;

	TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
}

FVector2D SVisualLoggerTimelineBar::ComputeDesiredSize(float) const
{
	return FVector2D(5000.0f, 20.0f);
}

int32 SVisualLoggerTimelineBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	//@TODO: Optimize it like it was with old LogVisualizer, to draw everything much faster (SebaK)
	int32 RetLayerId = LayerId;

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
	float LocalScrubPosition = TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get();

	float ViewRange = LocalViewRange.Size<float>();
	float PixelsPerInput = ViewRange > 0 ? AllottedGeometry.GetLocalSize().X / ViewRange : 0;
	float CurrentScrubLinePos = (LocalScrubPosition - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;
	float BoxWidth = FMath::Max(0.08f * PixelsPerInput, 4.0f);

	// Draw a region around the entire section area
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		FLogVisualizerStyle::Get().GetBrush("Sequencer.SectionArea.Background"),
		ESlateDrawEffect::None,
		TimelineOwner.Pin()->IsSelected() ? FLinearColor(.2f, .2f, .2f, 0.5f) : FLinearColor(.1f, .1f, .1f, 0.5f)
		);

	const FSlateBrush* FillImage = FLogVisualizerStyle::Get().GetBrush("LogVisualizer.LogBar.EntryDefault");
	static const FColor CurrentTimeColor(140, 255, 255, 255);
	static const FColor ErrorTimeColor(255, 0, 0, 255);
	static const FColor WarningTimeColor(255, 255, 0, 255);
	static const FColor SelectedBarColor(255, 255, 255, 255);
	const FSlateBrush* SelectedFillImage = FLogVisualizerStyle::Get().GetBrush("LogVisualizer.LogBar.Selected");

	const ESlateDrawEffect DrawEffects = ESlateDrawEffect::None;// bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TArray<float> ErrorTimes;
	TArray<float> WarningTimes;
	int32 EntryIndex = 0;

	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(TimelineOwner.Pin()->GetName());
	auto &Entries = DBRow.GetItems();
	while (EntryIndex < Entries.Num())
	{
		const FVisualLogEntry& Entry = Entries[EntryIndex].Entry;
		if (Entry.TimeStamp < LocalViewRange.GetLowerBoundValue() || Entry.TimeStamp > LocalViewRange.GetUpperBoundValue())
		{
			EntryIndex++;
			continue;
		}

		if (DBRow.IsItemVisible(EntryIndex)==false)
		{
			EntryIndex++;
			continue;
		}

		// find bar width, connect all contiguous bars to draw them as one geometry (rendering optimization)
		const float StartPos = (Entry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput - 2;
		float EndPos = (Entry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput + 2;
		int32 StartIndex = EntryIndex;
		float LastEndX = MAX_FLT;
		for (; StartIndex < Entries.Num(); ++StartIndex)
		{
			const FVisualLogEntry& CurrentEntry = Entries[StartIndex].Entry;
			if (CurrentEntry.TimeStamp < LocalViewRange.GetLowerBoundValue() || CurrentEntry.TimeStamp > LocalViewRange.GetUpperBoundValue())
			{
				break;
			}

			if (DBRow.IsItemVisible(StartIndex) == false)
			{
				continue;
			}

			const TArray<FVisualLogLine>& LogLines = CurrentEntry.LogLines;
			bool bAddedWarning = false;
			bool bAddedError = false;
			for (const FVisualLogLine& CurrentLine : LogLines)
			{
				if (CurrentLine.Verbosity <= ELogVerbosity::Error && !bAddedError)
				{
					ErrorTimes.AddUnique(CurrentEntry.TimeStamp);
					bAddedError = true;
				}
				else if (CurrentLine.Verbosity == ELogVerbosity::Warning && !bAddedWarning)
				{
					WarningTimes.AddUnique(CurrentEntry.TimeStamp);
					bAddedWarning = true;
				}
				if (bAddedError && bAddedWarning)
				{
					break;
				}
			}

			const float CurrentStartPos = (CurrentEntry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput - 2;
			if (CurrentStartPos > EndPos)
			{
				break;
			}
			EndPos = (CurrentEntry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput + 2;
		}

		if (EndPos - StartPos > 0)
		{
			const float BarWidth = (EndPos - StartPos);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				RetLayerId,
				AllottedGeometry.ToPaintGeometry(
				FVector2D(StartPos, 0.0f),
				FVector2D(BarWidth, AllottedGeometry.Size.Y)),
				FillImage,
				DrawEffects,
				CurrentTimeColor
				);
		}
		EntryIndex = StartIndex;
	}

	if (WarningTimes.Num()) RetLayerId++;
	for (auto CurrentTime : WarningTimes)
	{
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 3, 0.0f),
			FVector2D(6, AllottedGeometry.GetLocalSize().Y)),
			FillImage,
			DrawEffects,
			WarningTimeColor
			);
	}

	if (ErrorTimes.Num()) RetLayerId++;
	for (auto CurrentTime : ErrorTimes)
	{
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 3, 0.0f),
			FVector2D(6, AllottedGeometry.GetLocalSize().Y)),
			FillImage,
			DrawEffects,
			ErrorTimeColor
			);
	}

	int32 BestItemIndex = DBRow.GetClosestItem(LocalScrubPosition);

	if (TimelineOwner.Pin()->IsSelected() && DBRow.GetCurrentItemIndex() != INDEX_NONE)
	{
		const auto &HighlightedItemEntry = DBRow.GetCurrentItem();
		float CurrentTime = HighlightedItemEntry.Entry.TimeStamp;
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 2, 0.0f),
			FVector2D(4, AllottedGeometry.GetLocalSize().Y)),
			SelectedFillImage,
			ESlateDrawEffect::None,
			SelectedBarColor
			);
	}

	return RetLayerId;
}
