// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrackEditor.h"

#include "MovieSceneNiagaraEmitterTrack.h"


/**
*	Track editor for Niagara emitter tracks
*/
class FNiagaraEmitterTrackEditor
	: public FMovieSceneTrackEditor
{
public:
	FNiagaraEmitterTrackEditor(TSharedPtr<ISequencer> Sequencer);

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

	//~ FMovieSceneTrackEditor interface.
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
};
