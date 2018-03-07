// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class UMovieSceneSequence;

/**
 * Interface for sequencer object bindings
 */
class ISequencerEditorObjectBinding
{
public:

	/**
	 * Builds up the sequencer's "Add" menu.
	 *
	 * @param MenuBuilder The menu builder to change.
	 */
	virtual void BuildSequencerAddMenu(FMenuBuilder& MenuBuilder) = 0;

	/**
	 * Returns whether a sequence is supported by this tool.
	 *
	 * @param InSequence The sequence that could be supported.
	 * @return true if the type is supported.
	 */
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerEditorObjectBinding() { }
};
