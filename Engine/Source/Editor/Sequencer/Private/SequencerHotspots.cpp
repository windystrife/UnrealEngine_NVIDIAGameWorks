// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerHotspots.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "Tools/EditToolDragOperations.h"
#include "SequencerContextMenus.h"
#include "SSequencerTrackArea.h"
#include "Tools/SequencerEditTool_Movement.h"
#include "Tools/SequencerEditTool_Selection.h"
#include "SequencerTrackNode.h"
#include "SBox.h"

#define LOCTEXT_NAMESPACE "SequencerHotspots"

TSharedRef<ISequencerSection> FSectionHandle::GetSectionInterface() const
{
	return TrackNode->GetSections()[SectionIndex];
}

UMovieSceneSection* FSectionHandle::GetSectionObject() const
{
	return GetSectionInterface()->GetSectionObject();
}

void FKeyHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

TOptional<float> FKeyHotspot::GetTime() const
{
	return Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
}

bool FKeyHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, float MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);
	FKeyContextMenu::BuildMenu(MenuBuilder, Sequencer);
	return true;
}

TOptional<float> FSectionHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	return ThisSection ? ThisSection->GetStartTime() : TOptional<float>();
}

TOptional<float> FSectionHotspot::GetOffsetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	return ThisSection ? ThisSection->GetOffsetTime() : TOptional<float>();
}

void FSectionHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();

	// Move sections if they are selected
	if (InSequencer.GetSelection().IsSelected(ThisSection))
	{
		InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
	}
	else
	{
		// Activate selection mode if the section has keys or is infinite, otherwise just move it
		TSet<FKeyHandle> KeyHandles;
		ThisSection->GetKeyHandles(KeyHandles, ThisSection->GetRange());

		if (KeyHandles.Num() || ThisSection->IsInfinite())
		{
			InTrackArea.AttemptToActivateTool(FSequencerEditTool_Selection::Identifier);
		}
		else
		{
			InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
		}
	}
}

bool FSectionHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, float MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);

	TSharedPtr<ISequencerSection> SectionInterface = Section.TrackNode->GetSections()[Section.SectionIndex];

	FGuid ObjectBinding;
	if (Section.TrackNode.IsValid())
	{
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = Section.TrackNode->FindParentObjectBindingNode();
		if (ObjectBindingNode.IsValid())
		{
			ObjectBinding = ObjectBindingNode->GetObjectBinding();
		}
	}

	FSectionContextMenu::BuildMenu(MenuBuilder, Sequencer, MouseDownTime);

	SectionInterface->BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	return true;
}

TOptional<float> FSectionResizeHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	if (!ThisSection)
	{
		return TOptional<float>();
	}
	return HandleType == Left ? ThisSection->GetStartTime() : ThisSection->GetEndTime();
}

void FSectionResizeHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

TSharedPtr<ISequencerEditToolDragOperation> FSectionResizeHotspot::InitiateDrag(ISequencer& Sequencer)
{
	const auto& SelectedSections = Sequencer.GetSelection().GetSelectedSections();
	auto SectionHandles = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget())->GetSectionHandles(SelectedSections);
	
	if (!SelectedSections.Contains(Section.GetSectionObject()))
	{
		Sequencer.GetSelection().Empty();
		Sequencer.GetSelection().AddToSelection(Section.GetSectionObject());
		SequencerHelpers::UpdateHoveredNodeFromSelectedSections(static_cast<FSequencer&>(Sequencer));

		SectionHandles.Empty();
		SectionHandles.Add(Section);
	}
	const bool bIsSlipping = false;
	return MakeShareable( new FResizeSection(static_cast<FSequencer&>(Sequencer), SectionHandles, HandleType == Right, bIsSlipping) );
}

TOptional<float> FSectionEasingHandleHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	if (ThisSection)
	{
		if (HandleType == ESequencerEasingType::In && !ThisSection->GetEaseInRange().IsEmpty())
		{
			return ThisSection->GetEaseInRange().GetUpperBoundValue();
		}
		else if (HandleType == ESequencerEasingType::Out && !ThisSection->GetEaseOutRange().IsEmpty())
		{
			return ThisSection->GetEaseOutRange().GetLowerBoundValue();
		}
	}
	return TOptional<float>();
}

void FSectionEasingHandleHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

bool FSectionEasingHandleHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime)
{
	FEasingContextMenu::BuildMenu(MenuBuilder, { FEasingAreaHandle{Section, HandleType} }, static_cast<FSequencer&>(Sequencer), MouseDownTime);
	return true;
}

TSharedPtr<ISequencerEditToolDragOperation> FSectionEasingHandleHotspot::InitiateDrag(ISequencer& Sequencer)
{
	return MakeShareable( new FManipulateSectionEasing(static_cast<FSequencer&>(Sequencer), Section, HandleType == ESequencerEasingType::In) );
}

bool FSectionEasingAreaHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime)
{
	FEasingContextMenu::BuildMenu(MenuBuilder, Easings, static_cast<FSequencer&>(Sequencer), MouseDownTime);

	TSharedPtr<ISequencerSection> SectionInterface = Section.TrackNode->GetSections()[Section.SectionIndex];

	FGuid ObjectBinding;
	if (Section.TrackNode.IsValid())
	{
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = Section.TrackNode->FindParentObjectBindingNode();
		if (ObjectBindingNode.IsValid())
		{
			ObjectBinding = ObjectBindingNode->GetObjectBinding();
		}
	}

	SectionInterface->BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	return true;
}

#undef LOCTEXT_NAMESPACE