// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Sections/MovieSceneBoolSection.h"


/**
 * A property track for bool properties which have been set
 * to use a UMovieSceneVisibilityTrack through metadata.
 */
class FVisibilityPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneVisibilityTrack, UMovieSceneBoolSection, bool>
{
public:

	/** Constructor. */
	FVisibilityPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		// Don't supply any property type names to watch since the FBoolPropertyTrackEditor is already watching for bool property changes.
		// @todo: Is this right? comment says we should't be passing the property type name, yet we currently are.
		: FPropertyTrackEditor<UMovieSceneVisibilityTrack, UMovieSceneBoolSection, bool>(InSequencer, GetAnimatedPropertyTypes())
	{ }

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromPropertyTypeName(NAME_BoolProperty) });
	}

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	//~ ISequencerTrackEditor interface

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

protected:

	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<bool>& NewGeneratedKeys, TArray<bool>& DefaultGeneratedKeys) override;
	virtual bool ForCustomizedUseOnly() { return true; }
};
