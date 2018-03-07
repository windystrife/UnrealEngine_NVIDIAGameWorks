// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequenceInstance.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"

FMovieSceneSequenceInstance::FMovieSceneSequenceInstance(const UMovieSceneSequence& InMovieSceneSequence, FMovieSceneSequenceIDRef InSequenceID)
{
	MovieSceneSequence = &InMovieSceneSequence;
	SequenceID = InSequenceID;
}

TRange<float> FMovieSceneSequenceInstance::GetTimeRange() const
{
	return MovieSceneSequence->GetMovieScene()->GetPlaybackRange();
}
