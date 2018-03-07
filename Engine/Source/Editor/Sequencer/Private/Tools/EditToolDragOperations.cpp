// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tools/EditToolDragOperations.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "Sequencer.h"
#include "SequencerSettings.h"
#include "SequencerCommonHelpers.h"
#include "VirtualTrackArea.h"
#include "SequencerTrackNode.h"

struct FDefaultKeySnappingCandidates : ISequencerSnapCandidate
{
	FDefaultKeySnappingCandidates(const TSet<FSequencerSelectedKey>& InKeysToExclude)
		: KeysToExclude(InKeysToExclude)
	{}

	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) override
	{
		return !KeysToExclude.Contains(FSequencerSelectedKey(*Section, KeyArea, KeyHandle));
	}

	const TSet<FSequencerSelectedKey>& KeysToExclude;
};

struct FDefaultSectionSnappingCandidates : ISequencerSnapCandidate
{
	FDefaultSectionSnappingCandidates(FSectionHandle InSectionToIgnore)
	{
		SectionsToIgnore.Add(InSectionToIgnore.GetSectionObject());
	}

	FDefaultSectionSnappingCandidates(const TArray<FSectionHandle>& InSectionsToIgnore)
	{
		for (auto& SectionHandle : InSectionsToIgnore)
		{
			SectionsToIgnore.Add(SectionHandle.GetSectionObject());
		}
	}

	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) override
	{
		return !SectionsToIgnore.Contains(Section);
	}

	TSet<UMovieSceneSection*> SectionsToIgnore;
};

TOptional<FSequencerSnapField::FSnapResult> SnapToInterval(const TArray<float>& InTimes, float Threshold, FSequencer* Sequencer)
{
	TOptional<FSequencerSnapField::FSnapResult> Result;

	float SnapAmount = 0.f;
	for (float Time : InTimes)
	{
		float IntervalSnap = SequencerHelpers::SnapTimeToInterval(Time, Sequencer->GetFixedFrameInterval());
		float ThisSnapAmount = IntervalSnap - Time;
		if (FMath::Abs(ThisSnapAmount) <= Threshold)
		{
			if (!Result.IsSet() || FMath::Abs(ThisSnapAmount) < SnapAmount)
			{
				Result = FSequencerSnapField::FSnapResult{Time, IntervalSnap};
				SnapAmount = ThisSnapAmount;
			}
		}
	}

	return Result;
}

/** How many pixels near the mouse has to be before snapping occurs */
const float PixelSnapWidth = 10.f;

TRange<float> GetSectionBoundaries(const UMovieSceneSection* Section, TArray<FSectionHandle>& SectionHandles, TSharedPtr<FSequencerTrackNode> SequencerNode)
{
	// Only get boundaries for the sections that aren't being moved
	TArray<const UMovieSceneSection*> SectionsBeingMoved;
	for (auto SectionHandle : SectionHandles)
	{
		SectionsBeingMoved.Add(SectionHandle.GetSectionObject());
	}

	// Find the borders of where you can drag to
	float LowerBound = -FLT_MAX, UpperBound = FLT_MAX;

	// Also get the closest borders on either side
	const TArray< TSharedRef<ISequencerSection> >& AllSections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < AllSections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* TestSection = AllSections[SectionIndex]->GetSectionObject();

		if (!SectionsBeingMoved.Contains(TestSection) && Section->GetRowIndex() == TestSection->GetRowIndex())
		{
			if (TestSection->GetEndTime() <= Section->GetStartTime() && TestSection->GetEndTime() > LowerBound)
			{
				LowerBound = TestSection->GetEndTime();
			}
			if (TestSection->GetStartTime() >= Section->GetEndTime() && TestSection->GetStartTime() < UpperBound)
			{
				UpperBound = TestSection->GetStartTime();
			}
		}
	}

	return TRange<float>(LowerBound, UpperBound);
}

FEditToolDragOperation::FEditToolDragOperation( FSequencer& InSequencer )
	: Sequencer(InSequencer)
{
	Settings = Sequencer.GetSettings();
}

FCursorReply FEditToolDragOperation::GetCursor() const
{
	return FCursorReply::Cursor( EMouseCursor::Default );
}

int32 FEditToolDragOperation::OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	return LayerId;
}

void FEditToolDragOperation::BeginTransaction( TArray< FSectionHandle >& Sections, const FText& TransactionDesc )
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	Transaction.Reset( new FScopedTransaction(TransactionDesc) );

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); )
	{
		UMovieSceneSection* SectionObj = Sections[SectionIndex].GetSectionObject();

		SectionObj->SetFlags( RF_Transactional );
		// Save the current state of the section
		if (SectionObj->TryModify())
		{
			++SectionIndex;
		}
		else
		{
			Sections.RemoveAt(SectionIndex);
		}
	}
}

void FEditToolDragOperation::EndTransaction()
{
	Transaction.Reset();
	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FResizeSection::FResizeSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections, bool bInDraggingByEnd, bool bInIsSlipping )
	: FEditToolDragOperation( InSequencer )
	, Sections( MoveTemp(InSections) )
	, bDraggingByEnd(bInDraggingByEnd)
	, bIsSlipping(bInIsSlipping)
	, MouseDownTime(0.f)
{
}

void FResizeSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "DragSectionEdgeTransaction", "Resize section") );

	MouseDownTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	DraggedKeyHandles.Empty();
	SectionInitTimes.Empty();

	bool bIsDilating = MouseEvent.IsControlDown();

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		for (auto SequencerSection : Handle.TrackNode->GetSections())
		{
			if (SequencerSection->GetSectionObject() == Section)
			{
				if (bIsDilating)
				{
					SequencerSection->BeginDilateSection();
				}
				else
				{
					SequencerSection->BeginResizeSection();
				}
				break;
			}
		}

		Section->GetKeyHandles(DraggedKeyHandles, Section->GetRange());
		SectionInitTimes.Add(Section, bDraggingByEnd ? Section->GetEndTime() : Section->GetStartTime());
	}
}

void FResizeSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	EndTransaction();

	DraggedKeyHandles.Empty();
}

void FResizeSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	bool bIsDilating = MouseEvent.IsControlDown();

	// Convert the current mouse position to a time
	float DeltaTime = VirtualTrackArea.PixelToTime(LocalMousePos.X) - MouseDownTime;

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> SectionTimes;
		for (auto& Handle : Sections)
		{
			UMovieSceneSection* Section = Handle.GetSectionObject();
			SectionTimes.Add(SectionInitTimes[Section] + DeltaTime);
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			SnappedTime = SnapToInterval(SectionTimes, Sequencer.GetFixedFrameInterval()/2.f, &Sequencer);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			DeltaTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		// Find the corresponding sequencer section to this movie scene section
		for (auto SequencerSection : Handle.TrackNode->GetSections())
		{
			if (SequencerSection->GetSectionObject() == Section)
			{
				float NewTime = SectionInitTimes[Section] + DeltaTime;

				if( bDraggingByEnd )
				{
					// Dragging the end of a section
					// Ensure we aren't shrinking past the start time
					NewTime = FMath::Max( NewTime, Section->GetStartTime() );

					if (bIsDilating)
					{
						float NewSize = NewTime - Section->GetStartTime();
						float DilationFactor = NewSize / Section->GetTimeSize();
						SequencerSection->DilateSection(DilationFactor, Section->GetStartTime(), DraggedKeyHandles);
					}
					else if (bIsSlipping)
					{
						SequencerSection->SlipSection( NewTime );
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_TrailingEdge, NewTime );
					}
				}
				else
				{
					// Dragging the start of a section
					// Ensure we arent expanding past the end time
					NewTime = FMath::Min( NewTime, Section->GetEndTime() );

					if (bIsDilating)
					{
						float NewSize = Section->GetEndTime() - NewTime;
						float DilationFactor = NewSize / Section->GetTimeSize();
						SequencerSection->DilateSection(DilationFactor, Section->GetEndTime(), DraggedKeyHandles);
					}
					else if (bIsSlipping)
					{
						SequencerSection->SlipSection( NewTime );
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_LeadingEdge, NewTime );
					}
				}

				UMovieSceneTrack* OuterTrack = Section->GetTypedOuter<UMovieSceneTrack>();
				if (OuterTrack)
				{
					OuterTrack->Modify();
					OuterTrack->OnSectionMoved(*Section);
				}

				break;
			}
		}
	}

	{
		TSet<UMovieSceneTrack*> Tracks;
		for (auto SectionHandle : Sections)
		{
			if (UMovieSceneTrack* Track = SectionHandle.GetSectionObject()->GetTypedOuter<UMovieSceneTrack>())
			{
				Tracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : Tracks)
		{
			Track->UpdateEasing();
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FMoveSection::FMoveSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections )
	: FEditToolDragOperation( InSequencer )
{
	// Only allow sections that are not infinite to be movable.
	for (auto InSection : InSections)
	{
		if (!InSection.GetSectionObject()->IsInfinite())
		{
			Sections.Add(InSection);
		}
	}

	SequencerNodeTreeUpdatedHandle = InSequencer.GetNodeTree()->OnUpdated().AddRaw(this, &FMoveSection::OnSequencerNodeTreeUpdated);
}

FMoveSection::~FMoveSection()
{
	Sequencer.GetNodeTree()->OnUpdated().Remove(SequencerNodeTreeUpdatedHandle);
}

void FMoveSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "MoveSectionTransaction", "Move Section") );

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	DraggedKeyHandles.Empty();

	const FVector2D InitialPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

	RelativeOffsets.Reserve(Sections.Num());
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto* Section = Sections[Index].GetSectionObject();

		Section->GetKeyHandles(DraggedKeyHandles, Section->GetRange());
		RelativeOffsets.Add(FRelativeOffset{ Section->GetStartTime() - InitialPosition.X, Section->GetEndTime() - InitialPosition.X });
	}
}

void FMoveSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	DraggedKeyHandles.Empty();

	TSet<UMovieSceneTrack*> Tracks;
	bool bRowIndicesFixed = false;
	for (auto& Handle : Sections)
	{
		Tracks.Add(Handle.TrackNode->GetTrack());
	}
	for (UMovieSceneTrack* Track : Tracks)
	{
		bRowIndicesFixed |= Track->FixRowIndices();
	}
	if (bRowIndicesFixed)
	{
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		UMovieSceneTrack* OuterTrack = Cast<UMovieSceneTrack>(Section->GetOuter());

		if (OuterTrack)
		{
			OuterTrack->Modify();
			OuterTrack->OnSectionMoved(*Section);
		}
	}

	EndTransaction();
}

void FMoveSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	LocalMousePos.Y = FMath::Clamp(LocalMousePos.Y, 0.f, VirtualTrackArea.GetPhysicalSize().Y);

	// Convert the current mouse position to a time
	FVector2D VirtualMousePos = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> SectionTimes;
		SectionTimes.Reserve(RelativeOffsets.Num());
		for (const auto& Offset : RelativeOffsets)
		{
			SectionTimes.Add(VirtualMousePos.X + Offset.StartTime);
			SectionTimes.Add(VirtualMousePos.X + Offset.EndTime);
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			SnappedTime = SnapToInterval(SectionTimes, Sequencer.GetFixedFrameInterval()/2.f, &Sequencer);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			VirtualMousePos.X += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	// If sections are all on different rows, don't set row indices for anything because it leads to odd behavior.
	bool bSectionsAreOnDifferentRows = false;
	int32 FirstRowIndex = Sections[0].GetSectionObject()->GetRowIndex();
	TArray<const UMovieSceneSection*> SectionsBeingMoved;
	for (auto SectionHandle : Sections)
	{
		if (FirstRowIndex != SectionHandle.GetSectionObject()->GetRowIndex())
		{
			bSectionsAreOnDifferentRows = true;
		}
		SectionsBeingMoved.Add(SectionHandle.GetSectionObject());
	}

	TOptional<float> MinDeltaXTime;

	// Disallow movement if any of the sections can't move
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (Section->GetBlendType().IsValid())
		{
			continue;
		}

		// *2 because we store start/end times
		const float DeltaTime = VirtualMousePos.X + RelativeOffsets[Index].StartTime - Section->GetStartTime();

		// Find the borders of where you can move to
		TRange<float> SectionBoundaries = GetSectionBoundaries(Section, Sections, Handle.TrackNode);

		float LeftMovementMaximum = SectionBoundaries.GetLowerBoundValue();
		float RightMovementMaximum = SectionBoundaries.GetUpperBoundValue();
		float NewStartTime = Section->GetStartTime() + DeltaTime;
		float NewEndTime = Section->GetEndTime() + DeltaTime;

		if (NewStartTime < LeftMovementMaximum || NewEndTime > RightMovementMaximum)
		{
			float ClampedDeltaTime = NewStartTime < LeftMovementMaximum ? LeftMovementMaximum - Section->GetStartTime() : RightMovementMaximum - Section->GetEndTime();

			if (!MinDeltaXTime.IsSet())
			{
				MinDeltaXTime = ClampedDeltaTime;
			}
			else if (MinDeltaXTime.GetValue() > ClampedDeltaTime)
			{
				MinDeltaXTime = ClampedDeltaTime;
			}
		}
	}

	bool bRowIndexChanged = false;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();

		// *2 because we store start/end times
		const float DeltaTime = VirtualMousePos.X + RelativeOffsets[Index].StartTime - Section->GetStartTime();

		auto AllSections = Handle.TrackNode->GetTrack()->GetAllSections();

		TArray<UMovieSceneSection*> MovieSceneSections;
		for (int32 i = 0; i < AllSections.Num(); ++i)
		{
			if (!SectionsBeingMoved.Contains(AllSections[i]))
			{
				MovieSceneSections.Add(AllSections[i]);
			}
		}

		int32 TargetRowIndex = Section->GetRowIndex();

		// vertical dragging
		if (Handle.TrackNode->GetTrack()->SupportsMultipleRows() && AllSections.Num() > 1)
		{
			// Compute the max row index whilst disregarding the one we're dragging
			int32 MaxRowIndex = 0;
			for (auto* Sec : MovieSceneSections)
			{
				if (Sec != Section)
				{
					MaxRowIndex = FMath::Max(Sec->GetRowIndex() + 1, MaxRowIndex);
				}
			}

			// Handle sub-track and non-sub-track dragging
			if (Handle.TrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::None)
			{
				const int32 NumRows = FMath::Max(Section->GetRowIndex() + 1, MaxRowIndex);

				// Find the total height of the track - this is necessary because tracks may contain key areas, but they will not use sub tracks unless there is more than one row
				float VirtualSectionBottom = 0.f;
				Handle.TrackNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){ VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

				// Assume same height rows
				const float VirtualSectionTop = Handle.TrackNode->GetVirtualTop();
				const float VirtualSectionHeight = VirtualSectionBottom - Handle.TrackNode->GetVirtualTop();

				const float VirtualRowHeight = VirtualSectionHeight / NumRows;
				const float MouseOffsetWithinRow = VirtualMousePos.Y - (VirtualSectionTop + (VirtualRowHeight * TargetRowIndex));

				if (MouseOffsetWithinRow < VirtualRowHeight || MouseOffsetWithinRow > VirtualRowHeight)
				{
					const int32 NewIndex = FMath::FloorToInt((VirtualMousePos.Y - VirtualSectionTop) / VirtualRowHeight);
					TargetRowIndex = FMath::Clamp(NewIndex, 0, MaxRowIndex);
				}
			}
			else if(Handle.TrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::SubTrack)
			{
				TSharedPtr<FSequencerTrackNode> ParentTrack = StaticCastSharedPtr<FSequencerTrackNode>(Handle.TrackNode->GetParent());
				if (ensure(ParentTrack.IsValid()))
				{
					for (int32 ChildIndex = 0; ChildIndex < ParentTrack->GetChildNodes().Num(); ++ChildIndex)
					{
						TSharedRef<FSequencerDisplayNode> ChildNode = ParentTrack->GetChildNodes()[ChildIndex];
						float VirtualSectionTop = ChildNode->GetVirtualTop();
						float VirtualSectionBottom = 0.f;
						ChildNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){ VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

						if (VirtualMousePos.Y < VirtualSectionBottom)
						{
							TargetRowIndex = ChildIndex;
							break;
						}
						else
						{
							TargetRowIndex = ChildIndex + 1;
						}
					}
				}
			}
		}

		bool bDeltaX = !FMath::IsNearlyZero(DeltaTime);
		bool bDeltaY = TargetRowIndex != Section->GetRowIndex();

		// Horizontal movement
		if (bDeltaX)
		{
			Section->MoveSection(MinDeltaXTime.Get(DeltaTime), DraggedKeyHandles);
		}

		// Vertical movement
		if (bDeltaY && !bSectionsAreOnDifferentRows &&
			(
				Section->GetBlendType().IsValid() ||
				!Section->OverlapsWithSections(MovieSceneSections, TargetRowIndex - Section->GetRowIndex(), DeltaTime)
			)
		)
		{
			Section->Modify();
			Section->SetRowIndex(TargetRowIndex);
			bRowIndexChanged = true;
		}
	}

	{
		TSet<UMovieSceneTrack*> Tracks;
		for (auto SectionHandle : Sections)
		{
			if (UMovieSceneTrack* Track = SectionHandle.GetSectionObject()->GetTypedOuter<UMovieSceneTrack>())
			{
				Tracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : Tracks)
		{
			Track->UpdateEasing();
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( bRowIndexChanged 
		? EMovieSceneDataChangeType::MovieSceneStructureItemsChanged 
		: EMovieSceneDataChangeType::TrackValueChanged );
}

void CollateTrackNodesByTrack(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TMap<UMovieSceneTrack*, TArray<TSharedRef<FSequencerTrackNode>>>& TrackToTrackNodesMap)
{
	for (TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes)
	{
		if (DisplayNode->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(DisplayNode);
			TArray<TSharedRef<FSequencerTrackNode>>* TrackNodes = TrackToTrackNodesMap.Find(TrackNode->GetTrack());
			if (TrackNodes == nullptr)
			{
				TrackNodes = &TrackToTrackNodesMap.Add(TrackNode->GetTrack());
			}
			TrackNodes->Add(TrackNode);
		}

		CollateTrackNodesByTrack(DisplayNode->GetChildNodes(), TrackToTrackNodesMap);
	}
}

bool TryUpdateHandleFromNewTrackNodes(const TArray<TSharedRef<FSequencerTrackNode>>& NewTrackNodes, FSectionHandle& SectionHandle)
{
	UMovieSceneSection* MovieSceneSection = SectionHandle.GetSectionObject();
	for (TSharedRef<FSequencerTrackNode> NewTrackNode : NewTrackNodes)
	{
		const TArray<TSharedRef<ISequencerSection>> SequencerSections = NewTrackNode->GetSections();
		for (int32 i = 0; i < SequencerSections.Num(); i++)
		{
			if (SequencerSections[i]->GetSectionObject() == MovieSceneSection)
			{
				SectionHandle.TrackNode = NewTrackNode;
				SectionHandle.SectionIndex = i;
				return true;
			}
		}
	}
	return false;
}

void FMoveSection::OnSequencerNodeTreeUpdated()
{
	TMap<UMovieSceneTrack*, TArray<TSharedRef<FSequencerTrackNode>>> TrackToTrackNodesMap;
	CollateTrackNodesByTrack(Sequencer.GetNodeTree()->GetRootNodes(), TrackToTrackNodesMap);

	// Update the track nodes in the handles based on the original track and section index.
	for (FSectionHandle& SectionHandle : Sections)
	{
		TArray<TSharedRef<FSequencerTrackNode>>* NewTrackNodes = TrackToTrackNodesMap.Find(SectionHandle.TrackNode->GetTrack());
		ensureMsgf(NewTrackNodes != nullptr, TEXT("Error rebuilding section handles:  Track not found after node tree update."));

		if (NewTrackNodes != nullptr)
		{
			bool bHandleUpdated = TryUpdateHandleFromNewTrackNodes(*NewTrackNodes, SectionHandle);
			ensureMsgf(bHandleUpdated, TEXT("Error rebuilding section handles: Track node with correct track and section index could not be found."));
		}
	}
}

void FMoveKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	check( SelectedKeys.Num() > 0 )

	FSequencerDisplayNode::DisableKeyGoupingRegeneration();

	FDefaultKeySnappingCandidates SnapCandidates(SelectedKeys);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates);

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "MoveKeysTransaction", "Move Keys") );

	const float MouseTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);

	for( FSequencerSelectedKey SelectedKey : SelectedKeys )
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		RelativeOffsets.Add(SelectedKey, SelectedKey.KeyArea->GetKeyTime(SelectedKey.KeyHandle.GetValue()) - MouseTime);

		// Only modify sections once
		if( !ModifiedSections.Contains( OwningSection ) )
		{
			OwningSection->SetFlags( RF_Transactional );

			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}
}

void FMoveKeys::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	float MouseTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);
	float DistanceMoved = MouseTime - VirtualTrackArea.PixelToTime(LocalMousePos.X - MouseEvent.GetCursorDelta().X);

	if( DistanceMoved == 0.0f )
	{
		return;
	}

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> KeyTimes;
		for( FSequencerSelectedKey SelectedKey : SelectedKeys )
		{
			KeyTimes.Add(MouseTime + RelativeOffsets.FindRef(SelectedKey));
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapKeyTimesToKeys())
		{
			SnappedTime = SnapField->Snap(KeyTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapKeyTimesToInterval())
		{
			SnappedTime = SnapToInterval(KeyTimes, Sequencer.GetFixedFrameInterval()/2.f, &Sequencer);
		}

		if (SnappedTime.IsSet())
		{
			MouseTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	float PrevNewKeyTime = FLT_MAX;

	for( FSequencerSelectedKey SelectedKey : SelectedKeys )
	{
		UMovieSceneSection* Section = SelectedKey.Section;

		if (!ModifiedSections.Contains(Section))
		{
			continue;
		}

		TSharedPtr<IKeyArea>& KeyArea = SelectedKey.KeyArea;

		float NewKeyTime = MouseTime + RelativeOffsets.FindRef(SelectedKey);
		float CurrentTime = KeyArea->GetKeyTime( SelectedKey.KeyHandle.GetValue() );

		// Tell the key area to move the key.  We reset the key index as a result of the move because moving a key can change it's internal index 
		KeyArea->MoveKey( SelectedKey.KeyHandle.GetValue(), NewKeyTime - CurrentTime );

		// If the key moves outside of the section resize the section to fit the key
		// @todo Sequencer - Doesn't account for hitting other sections 
		if( NewKeyTime > Section->GetEndTime() )
		{
			Section->SetEndTime( NewKeyTime );
		}
		else if( NewKeyTime < Section->GetStartTime() )
		{
			Section->SetStartTime( NewKeyTime );
		}

		if (PrevNewKeyTime == FLT_MAX)
		{
			PrevNewKeyTime = NewKeyTime;
		}
		else if (!FMath::IsNearlyEqual(NewKeyTime, PrevNewKeyTime))
		{
			PrevNewKeyTime = -FLT_MAX;
		}
	}

	// Snap the play time to the new dragged key time if all the keyframes were dragged to the same time
	if (Settings->GetSnapPlayTimeToDraggedKey() && PrevNewKeyTime != FLT_MAX && PrevNewKeyTime != -FLT_MAX)
	{
		Sequencer.SetLocalTime(PrevNewKeyTime);
	}

	for (UMovieSceneSection* Section : ModifiedSections)
	{
		Section->MarkAsChanged();
	}
	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FMoveKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	ModifiedSections.Empty();
	EndTransaction();
	FSequencerDisplayNode::EnableKeyGoupingRegeneration();
}

void FDuplicateKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Duplicate and select all the keys
	TSet<FSequencerSelectedKey> OldSelection = SelectedKeys;

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "DuplicateKeysTransaction", "Duplicate Keys") );

	// Modify all the sections first
	for (const FSequencerSelectedKey& SelectedKey : SelectedKeys)
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		// Only modify sections once
		if (!ModifiedSections.Contains(OwningSection ))
		{
			OwningSection->SetFlags(RF_Transactional);
			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}

	// Then duplicate the keys

	// @todo sequencer: selection in transactions
	Sequencer.GetSelection().EmptySelectedKeys();
	for (const FSequencerSelectedKey& SelectedKey : OldSelection)
	{
		FSequencerSelectedKey NewKey = SelectedKey;
		NewKey.KeyHandle = SelectedKey.KeyArea->DuplicateKey(SelectedKey.KeyHandle.GetValue());
		Sequencer.GetSelection().AddToSelection(NewKey);
	}

	// Now start the move drag
	FMoveKeys::OnBeginDrag(MouseEvent, LocalMousePos, VirtualTrackArea);
}

void FDuplicateKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	FMoveKeys::OnEndDrag(MouseEvent, LocalMousePos, VirtualTrackArea);

	EndTransaction();
}

FManipulateSectionEasing::FManipulateSectionEasing( FSequencer& InSequencer, FSectionHandle InSection, bool _bEaseIn )
	: FEditToolDragOperation(InSequencer)
	, Handle(InSection)
	, bEaseIn(_bEaseIn)
	, MouseDownTime(0.f)
{
}

void FManipulateSectionEasing::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	Transaction.Reset( new FScopedTransaction(NSLOCTEXT("Sequencer", "DragSectionEasing", "Change Section Easing")) );

	UMovieSceneSection* Section = Handle.GetSectionObject();
	Section->SetFlags( RF_Transactional );
	Section->Modify();

	MouseDownTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);

	if (Settings->GetSnapSectionTimesToSections())
	{
		// Construct a snap field of all section bounds
		ISequencerSnapCandidate SnapCandidates;
		SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);
	}

	InitValue = bEaseIn ? Section->Easing.GetEaseInTime() : Section->Easing.GetEaseOutTime();
}

void FManipulateSectionEasing::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Convert the current mouse position to a time
	float DeltaTime = VirtualTrackArea.PixelToTime(LocalMousePos.X) - MouseDownTime;

	// Snapping
	if (Settings->GetIsSnapEnabled())
	{
		TArray<float> SnapTimes;
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (bEaseIn)
		{
			SnapTimes.Add(FMath::Clamp(Section->GetStartTime() + InitValue.Get(0.f) + DeltaTime, Section->GetStartTime(), Section->GetEndTime()));
		}
		else
		{
			SnapTimes.Add(FMath::Clamp(Section->GetEndTime() - InitValue.Get(0.f) + DeltaTime, Section->GetStartTime(), Section->GetEndTime()));
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SnapTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			SnappedTime = SnapToInterval(SnapTimes, Sequencer.GetFixedFrameInterval()/2.f, &Sequencer);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			DeltaTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	UMovieSceneSection* Section = Handle.GetSectionObject();

	if (bEaseIn)
	{
		Section->Easing.bManualEaseIn = true;
		Section->Easing.ManualEaseInTime = FMath::Clamp(InitValue.Get(0.f) + DeltaTime, 0.f, Section->GetEndTime() - Section->GetStartTime());
	}
	else
	{
		Section->Easing.bManualEaseOut = true;
		Section->Easing.ManualEaseOutTime = FMath::Clamp(InitValue.Get(0.f) - DeltaTime, 0.f, Section->GetEndTime() - Section->GetStartTime());
	}

	UMovieSceneTrack* OuterTrack = Section->GetTypedOuter<UMovieSceneTrack>();
	if (OuterTrack)
	{
		OuterTrack->MarkAsChanged();
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FManipulateSectionEasing::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	EndTransaction();
}

