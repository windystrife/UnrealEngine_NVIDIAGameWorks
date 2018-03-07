// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/MovieScene2DTransformSection.h"

class F2DTransformTrackEditor
	: public FPropertyTrackEditor<UMovieScene2DTransformTrack, UMovieScene2DTransformSection, F2DTransformKey>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	F2DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FPropertyTrackEditor<UMovieScene2DTransformTrack, UMovieScene2DTransformSection, F2DTransformKey>( InSequencer, GetAnimatedPropertyTypes() )
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromStructType("WidgetTransform") });
	}

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

	// FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<F2DTransformKey>& NewGeneratedKeys, TArray<F2DTransformKey>& DefaultGeneratedKeys ) override;

private:
	static FName TranslationName;
	static FName ScaleName;
	static FName ShearName;
	static FName AngleName;
};
