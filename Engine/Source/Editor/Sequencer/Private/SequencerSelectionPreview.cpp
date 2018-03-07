// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerSelectionPreview.h"
#include "MovieSceneSection.h"

void FSequencerSelectionPreview::SetSelectionState(FSequencerSelectedKey Key, ESelectionPreviewState InState)
{
	if (InState == ESelectionPreviewState::Undefined)
	{
		DefinedKeyStates.Remove(Key);
	}
	else
	{
		DefinedKeyStates.Add(Key, InState);
	}
}

void FSequencerSelectionPreview::SetSelectionState(UMovieSceneSection* Section, ESelectionPreviewState InState)
{
	if (InState == ESelectionPreviewState::Undefined)
	{
		DefinedSectionStates.Remove(Section);
	}
	else
	{
		DefinedSectionStates.Add(Section, InState);
	}
}

void FSequencerSelectionPreview::SetSelectionState(TSharedRef<FSequencerDisplayNode> OutlinerNode, ESelectionPreviewState InState)
{
	if (InState == ESelectionPreviewState::Undefined)
	{
		DefinedOutlinerNodeStates.Remove(OutlinerNode);
	}
	else
	{
		DefinedOutlinerNodeStates.Add(OutlinerNode, InState);
	}
}

ESelectionPreviewState FSequencerSelectionPreview::GetSelectionState(FSequencerSelectedKey Key) const
{
	if (auto* State = DefinedKeyStates.Find(Key))
	{
		return *State;
	}
	return ESelectionPreviewState::Undefined;
}

ESelectionPreviewState FSequencerSelectionPreview::GetSelectionState(UMovieSceneSection* Section) const
{
	if (auto* State = DefinedSectionStates.Find(Section))
	{
		return *State;
	}
	return ESelectionPreviewState::Undefined;
}

ESelectionPreviewState FSequencerSelectionPreview::GetSelectionState(TSharedRef<FSequencerDisplayNode> OutlinerNode) const
{
	if (auto* State = DefinedOutlinerNodeStates.Find(OutlinerNode))
	{
		return *State;
	}
	return ESelectionPreviewState::Undefined;
}

void FSequencerSelectionPreview::Empty()
{
	EmptyDefinedKeyStates();
	EmptyDefinedSectionStates();
	EmptyDefinedOutlinerNodeStates();
}

void FSequencerSelectionPreview::EmptyDefinedKeyStates()
{
	DefinedKeyStates.Reset();
}

void FSequencerSelectionPreview::EmptyDefinedSectionStates()
{
	DefinedSectionStates.Reset();
}

void FSequencerSelectionPreview::EmptyDefinedOutlinerNodeStates()
{
	DefinedOutlinerNodeStates.Reset();
}
