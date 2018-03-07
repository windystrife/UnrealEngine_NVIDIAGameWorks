// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "SequencerSelectedKey.h"

class UMovieSceneSection;

enum class ESelectionPreviewState
{
	Undefined,
	Selected,
	NotSelected
};


/**
 * Manages the selection of keys, sections, and outliner nodes for the sequencer.
 */
class FSequencerSelectionPreview
{
public:

	/** Access the defined key states */
	const TMap<FSequencerSelectedKey, ESelectionPreviewState>& GetDefinedKeyStates() const { return DefinedKeyStates; }

	/** Access the defined section states */
	const TMap<TWeakObjectPtr<UMovieSceneSection>, ESelectionPreviewState>& GetDefinedSectionStates() const { return DefinedSectionStates; }

	/** Access the defined display node states */
	const TMap<TSharedRef<FSequencerDisplayNode>, ESelectionPreviewState>& GetDefinedOutlinerNodeStates() const { return DefinedOutlinerNodeStates; }

	/** Adds a key to the selection */
	void SetSelectionState(FSequencerSelectedKey Key, ESelectionPreviewState InState);

	/** Adds a section to the selection */
	void SetSelectionState(UMovieSceneSection* Section, ESelectionPreviewState InState);

	/** Adds an outliner node to the selection */
	void SetSelectionState(TSharedRef<FSequencerDisplayNode> OutlinerNode, ESelectionPreviewState InState);

	/** Returns the selection state for the the specified key. */
	ESelectionPreviewState GetSelectionState(FSequencerSelectedKey Key) const;

	/** Returns the selection state for the the specified section. */
	ESelectionPreviewState GetSelectionState(UMovieSceneSection* Section) const;

	/** Returns the selection state for the the specified section. */
	ESelectionPreviewState GetSelectionState(TSharedRef<FSequencerDisplayNode> OutlinerNode) const;

	/** Empties all selections. */
	void Empty();

	/** Empties the key selection. */
	void EmptyDefinedKeyStates();

	/** Empties the section selection. */
	void EmptyDefinedSectionStates();

	/** Empties the outliner node selection. */
	void EmptyDefinedOutlinerNodeStates();

private:

	TMap<FSequencerSelectedKey, ESelectionPreviewState> DefinedKeyStates;
	TMap<TWeakObjectPtr<UMovieSceneSection>, ESelectionPreviewState> DefinedSectionStates;
	TMap<TSharedRef<FSequencerDisplayNode>, ESelectionPreviewState> DefinedOutlinerNodeStates;
};
