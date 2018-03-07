// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Sections/MovieSceneColorSection.h"

/**
* A property track editor for colors.
*/
class FColorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FColorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<3>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<3>>({
			FAnimatedPropertyKey::FromStructType(NAME_Color),
			FAnimatedPropertyKey::FromStructType(NAME_LinearColor),
			FAnimatedPropertyKey::FromStructType("SlateColor")
		});
	}

	/**
	 * Creates an instance of this class (called by a sequencer).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	//~ ISequencerTrackEditor interface

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

protected:

	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FColorKey>& NewGeneratedKeys, TArray<FColorKey>& DefaultGeneratedKeys) override;

protected:

	//~ ISequencerTrackEditor interface

	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

private:

	static FName RedName;
	static FName GreenName;
	static FName BlueName;
	static FName AlphaName;
	static FName SpecifiedColorName;
};
