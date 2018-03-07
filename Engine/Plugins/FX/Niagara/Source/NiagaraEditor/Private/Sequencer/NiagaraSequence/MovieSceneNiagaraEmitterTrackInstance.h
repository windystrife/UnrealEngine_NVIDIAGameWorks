// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class UMovieSceneNiagaraEmitterTrack;
struct FNiagaraEmitterInstance;

/**
*	One instance of a UEmitterMovieSceneTrack
*/
class FMovieSceneNiagaraEmitterTrackInstance
	: public IMovieSceneTrackInstance
{
public:
	FMovieSceneNiagaraEmitterTrackInstance(UMovieSceneNiagaraEmitterTrack* InTrack)
		: Track(InTrack)
	{
	}

	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
	{
	}

	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}

private:
	TSharedPtr<FNiagaraEmitterInstance> Emitter;
	UMovieSceneNiagaraEmitterTrack *Track;
};