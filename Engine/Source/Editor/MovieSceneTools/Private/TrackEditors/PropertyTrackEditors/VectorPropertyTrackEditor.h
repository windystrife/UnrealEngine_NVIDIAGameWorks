// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Sections/MovieSceneVectorSection.h"

/**
 * A property track editor for vectors.
 */
class FVectorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FVectorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<3>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<3>>({
			FAnimatedPropertyKey::FromStructType(NAME_Vector),
			FAnimatedPropertyKey::FromStructType(NAME_Vector4),
			FAnimatedPropertyKey::FromStructType(NAME_Vector2D)
		});
	}

	/**
	 * Creates an instance of this class (called by a sequence).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	//~ ISequencerTrackEditor interface

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

protected:

	//~ FMovieSceneTrackEditor interface

	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FVectorKey>& NewGeneratedKeys, TArray<FVectorKey>& DefaultGeneratedKeys) override;
	virtual void InitializeNewTrack(UMovieSceneVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams) override;

private:

	static FName XName;
	static FName YName;
	static FName ZName;
	static FName WName;
};
