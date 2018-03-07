// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"

/**
 * A property track editor for transforms.
 */
class FTransformPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneTransformTrack, UMovieScene3DTransformSection, FTransformKey>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FTransformPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneTransformTrack, UMovieScene3DTransformSection, FTransformKey>(InSequencer, GetAnimatedPropertyTypes())
	{ }

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({
			FAnimatedPropertyKey::FromStructType(NAME_Transform)
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
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
protected:

	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FTransformKey>& NewGeneratedKeys, TArray<FTransformKey>& DefaultGeneratedKeys) override;
};
