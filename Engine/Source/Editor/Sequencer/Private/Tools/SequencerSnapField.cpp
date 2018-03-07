// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tools/SequencerSnapField.h"
#include "MovieScene.h"
#include "SSequencer.h"
#include "SSequencerTreeView.h"
#include "MovieSceneSequence.h"

struct FSnapGridVisitor : ISequencerEntityVisitor
{
	FSnapGridVisitor(ISequencerSnapCandidate& InCandidate, uint32 EntityMask)
		: ISequencerEntityVisitor(EntityMask)
		, Candidate(InCandidate)
	{}

	virtual void VisitKey(FKeyHandle KeyHandle, float KeyTime, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode>) const
	{
		if (Candidate.IsKeyApplicable(KeyHandle, KeyArea, Section))
		{
			Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::Key, KeyTime });
		}
	}
	virtual void VisitSection(UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode> Node) const
	{
		if (Candidate.AreSectionBoundsApplicable(Section))
		{
			Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::SectionBounds, Section->GetStartTime() });
			Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::SectionBounds, Section->GetEndTime() });
		}

		if (Candidate.AreSectionCustomSnapsApplicable(Section))
		{
			TArray<float> CustomSnaps;
			Section->GetSnapTimes(CustomSnaps, false);
			for (float Time : CustomSnaps)
			{
				Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::CustomSection, Time });
			}
		}
	}

	ISequencerSnapCandidate& Candidate;
	mutable TArray<FSequencerSnapPoint> Snaps;
};

FSequencerSnapField::FSequencerSnapField(const ISequencer& InSequencer, ISequencerSnapCandidate& Candidate, uint32 EntityMask)
{
	TSharedPtr<SSequencerTreeView> TreeView = StaticCastSharedRef<SSequencer>(InSequencer.GetSequencerWidget())->GetTreeView();

	TArray<TSharedRef<FSequencerDisplayNode>> VisibleNodes;
	for (const SSequencerTreeView::FCachedGeometry& Geometry : TreeView->GetAllVisibleNodes())
	{
		VisibleNodes.Add(Geometry.Node);
	}

	auto ViewRange = InSequencer.GetViewRange();
	FSequencerEntityWalker Walker(ViewRange);

	// Traverse the visible space, collecting snapping times as we go
	FSnapGridVisitor Visitor(Candidate, EntityMask);
	Walker.Traverse(Visitor, VisibleNodes);

	// Add the playback range start/end bounds as potential snap candidates
	TRange<float> PlaybackRange = InSequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::PlaybackRange, PlaybackRange.GetLowerBoundValue() });
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::PlaybackRange, PlaybackRange.GetUpperBoundValue() });

	// Add the current time as a potential snap candidate
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::CurrentTime, InSequencer.GetLocalTime() });

	// Add the selection range bounds as a potential snap candidate
	TRange<float> SelectionRange = InSequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetSelectionRange();
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::InOutRange, SelectionRange.GetLowerBoundValue() });
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::InOutRange, SelectionRange.GetUpperBoundValue() });

	// Sort
	Visitor.Snaps.Sort([](const FSequencerSnapPoint& A, const FSequencerSnapPoint& B){
		return A.Time < B.Time;
	});

	// Remove duplicates
	for (int32 Index = 0; Index < Visitor.Snaps.Num(); ++Index)
	{
		const float CurrentTime = Visitor.Snaps[Index].Time;

		int32 NumToMerge = 0;
		for (int32 DuplIndex = Index + 1; DuplIndex < Visitor.Snaps.Num(); ++DuplIndex)
		{
			if (!FMath::IsNearlyEqual(CurrentTime, Visitor.Snaps[DuplIndex].Time))
			{
				break;
			}
			++NumToMerge;
		}

		if (NumToMerge)
		{
			Visitor.Snaps.RemoveAt(Index + 1, NumToMerge, false);
		}
	}

	SortedSnaps = MoveTemp(Visitor.Snaps);
}

TOptional<float> FSequencerSnapField::Snap(float InTime, float Threshold) const
{
	int32 Min = 0;
	int32 Max = SortedSnaps.Num();

	// Binary search, then linearly search a range
	for ( ; Min != Max ; )
	{
		int32 SearchIndex = Min + (Max - Min) / 2;

		float ProspectiveSnapPos = SortedSnaps[SearchIndex].Time;
		if (ProspectiveSnapPos > InTime + Threshold)
		{
			Max = SearchIndex;
		}
		else if (ProspectiveSnapPos < InTime - Threshold)
		{
			Min = SearchIndex + 1;
		}
		else
		{
			// Linearly search forwards and backwards to find the closest snap

			float SnapDelta = ProspectiveSnapPos - InTime;

			// Search forwards while we're in the threshold
			for (int32 FwdIndex = SearchIndex+1; FwdIndex < Max-1 && SortedSnaps[FwdIndex].Time < InTime + Threshold; ++FwdIndex)
			{
				float ThisSnapDelta = InTime - SortedSnaps[FwdIndex].Time;
				if (FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
				{
					SnapDelta = ThisSnapDelta;
					ProspectiveSnapPos = SortedSnaps[FwdIndex].Time;
				}
			}

			// Search backwards while we're in the threshold
			for (int32 BckIndex = SearchIndex-1; BckIndex >= Min && SortedSnaps[BckIndex].Time > InTime + Threshold; --BckIndex)
			{
				float ThisSnapDelta = InTime - SortedSnaps[BckIndex].Time;
				if (FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
				{
					SnapDelta = ThisSnapDelta;
					ProspectiveSnapPos = SortedSnaps[BckIndex].Time;
				}
			}

			return ProspectiveSnapPos;
		}
	}

	return TOptional<float>();
}

TOptional<FSequencerSnapField::FSnapResult> FSequencerSnapField::Snap(const TArray<float>& InTimes, float Threshold) const
{
	TOptional<FSnapResult> ProspectiveSnap;
	float SnapDelta = 0.f;

	for (float Time : InTimes)
	{
		TOptional<float> ThisSnap = Snap(Time, Threshold);
		if (!ThisSnap.IsSet())
		{
			continue;
		}

		float ThisSnapDelta = ThisSnap.GetValue() - Time;
		if (!ProspectiveSnap.IsSet() || FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
		{
			ProspectiveSnap = FSnapResult{ Time, ThisSnap.GetValue() };
			SnapDelta = ThisSnapDelta;
		}
	}

	return ProspectiveSnap;
}
