// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SScrollBarTrack.h"
#include "Layout/ArrangedChildren.h"

static const int32 NUM_SCROLLBAR_SLOTS = 3;

void SScrollBarTrack::Construct(const FArguments& InArgs)
{
	OffsetFraction = 0;
	ThumbSizeFraction = 0;
	MinThumbSize = 35;
	Orientation = InArgs._Orientation;

	// This panel only supports 3 children!
	for (int32 CurChild = 0; CurChild < NUM_SCROLLBAR_SLOTS; ++CurChild)
	{
		Children.Add(new FSlot());
	}

	Children[TOP_SLOT_INDEX]
		[
			InArgs._TopSlot.Widget
		];

	Children[BOTTOM_SLOT_INDEX]
		[
			InArgs._BottomSlot.Widget
		];

	Children[THUMB_SLOT_INDEX]
		[
			InArgs._ThumbSlot.Widget
		];
}

SScrollBarTrack::FTrackSizeInfo SScrollBarTrack::GetTrackSizeInfo(const FGeometry& InTrackGeometry) const
{
	return FTrackSizeInfo(InTrackGeometry, Orientation, MinThumbSize, this->ThumbSizeFraction, this->OffsetFraction);
}

void SScrollBarTrack::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const float Width = AllottedGeometry.Size.X;
	const float Height = AllottedGeometry.Size.Y;

	// We only need to show all three children when the thumb is visible, otherwise we only need to show the track
	if (IsNeeded())
	{
		FTrackSizeInfo TrackSizeInfo = this->GetTrackSizeInfo(AllottedGeometry);

		// Arrange top half of the track
		FVector2D ChildSize = (Orientation == Orient_Horizontal)
			? FVector2D(TrackSizeInfo.ThumbStart, Height)
			: FVector2D(Width, TrackSizeInfo.ThumbStart);

		FVector2D ChildPos(0, 0);
		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(Children[TOP_SLOT_INDEX].GetWidget(), ChildPos, ChildSize)
			);

		// Arrange bottom half of the track
		ChildPos = (Orientation == Orient_Horizontal)
			? FVector2D(TrackSizeInfo.GetThumbEnd(), 0)
			: FVector2D(0, TrackSizeInfo.GetThumbEnd());

		ChildSize = (Orientation == Orient_Horizontal)
			? FVector2D(AllottedGeometry.GetLocalSize().X - TrackSizeInfo.GetThumbEnd(), Height)
			: FVector2D(Width, AllottedGeometry.GetLocalSize().Y - TrackSizeInfo.GetThumbEnd());

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(Children[BOTTOM_SLOT_INDEX].GetWidget(), ChildPos, ChildSize)
			);

		// Arrange the thumb
		ChildPos = (Orientation == Orient_Horizontal)
			? FVector2D(TrackSizeInfo.ThumbStart, 0)
			: FVector2D(0, TrackSizeInfo.ThumbStart);

		ChildSize = (Orientation == Orient_Horizontal)
			? FVector2D(TrackSizeInfo.ThumbSize, Height)
			: FVector2D(Width, TrackSizeInfo.ThumbSize);

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(Children[THUMB_SLOT_INDEX].GetWidget(), ChildPos, ChildSize)
			);
	}
	else
	{
		// No thumb is visible, so just show the top half of the track at the current width/height
		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(Children[TOP_SLOT_INDEX].GetWidget(), FVector2D(0, 0), FVector2D(Width, Height))
			);
	}
}

FVector2D SScrollBarTrack::ComputeDesiredSize(float) const
{
	if (Orientation == Orient_Horizontal)
	{
		const float DesiredHeight = FMath::Max3(Children[0].GetWidget()->GetDesiredSize().Y, Children[1].GetWidget()->GetDesiredSize().Y, Children[2].GetWidget()->GetDesiredSize().Y);
		return FVector2D(MinThumbSize, DesiredHeight);
	}
	else
	{
		const float DesiredWidth = FMath::Max3(Children[0].GetWidget()->GetDesiredSize().X, Children[1].GetWidget()->GetDesiredSize().X, Children[2].GetWidget()->GetDesiredSize().X);
		return FVector2D(DesiredWidth, MinThumbSize);
	}
}

FChildren* SScrollBarTrack::GetChildren()
{
	return &Children;
}

void SScrollBarTrack::SetSizes(float InThumbOffsetFraction, float InThumbSizeFraction)
{
	OffsetFraction = InThumbOffsetFraction;
	ThumbSizeFraction = InThumbSizeFraction;
}

bool SScrollBarTrack::IsNeeded() const
{
	// We use a small epsilon here to avoid the scroll bar showing up when all of the content is already in view, due to
	// floating point precision when the scroll bar state is set
	return ThumbSizeFraction < (1.0f - KINDA_SMALL_NUMBER);
}

float SScrollBarTrack::DistanceFromTop() const
{
	return OffsetFraction;
}

float SScrollBarTrack::DistanceFromBottom() const
{
	return 1.0f - (OffsetFraction + ThumbSizeFraction);
}

float SScrollBarTrack::GetMinThumbSize() const
{
	return MinThumbSize;
}

float SScrollBarTrack::GetThumbSizeFraction() const
{
	return ThumbSizeFraction;
}
