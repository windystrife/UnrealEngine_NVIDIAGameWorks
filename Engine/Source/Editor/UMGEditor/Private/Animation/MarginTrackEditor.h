// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Animation/MovieSceneMarginTrack.h"
#include "Animation/MovieSceneMarginSection.h"

class FMarginTrackEditor
	: public FPropertyTrackEditor<UMovieSceneMarginTrack, UMovieSceneMarginSection, FMarginKey>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FMarginTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FPropertyTrackEditor<UMovieSceneMarginTrack, UMovieSceneMarginSection, FMarginKey>( InSequencer, GetAnimatedPropertyTypes() )
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromStructType("Margin") });
	}

	/** Virtual destructor. */

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

	//~ ISequencerTrackEditor Interface

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

protected:

	//~ FPropertyTrackEditor Interface

	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FMarginKey>& NewGeneratedKeys, TArray<FMarginKey>& DefaultGeneratedKeys) override;

private:

	static FName LeftName;
	static FName TopName;
	static FName RightName;
	static FName BottomName;
};
