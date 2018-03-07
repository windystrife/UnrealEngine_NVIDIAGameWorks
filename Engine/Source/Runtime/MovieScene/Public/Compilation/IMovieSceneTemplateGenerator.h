// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSegment.h"
#include "Containers/ArrayView.h"
#include "MovieSceneSequenceID.h"

class UMovieSceneTrack;
struct FMovieSceneEvaluationFieldSegmentPtr;
struct FMovieSceneEvaluationTrack;
struct FMovieSceneSequenceTransform;
struct FMovieSceneSharedDataId;
struct FMovieSceneSubSequenceData;

/** Abstract base class used to generate evaluation templates */
struct IMovieSceneTemplateGenerator
{
	/**
	 * Add a new track that is to be owned by this template
	 *
	 * @param InTrackTemplate			The track template to add
	 * @param SourceTrack				The originating track
	 */
	virtual void AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a new track that is potentially shared between multiple tracks. Only one instance of SharedId can exist.
	 *
	 * @param InTrackTemplate			The track template to add
	 * @param SharedId					The identifier of the shared track (for cross referencing between tracks)
	 * @param SourceTrack				The originating track
	 */
	virtual void AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a legacy track to the template
	 *
	 * @param InTrackTemplate			The legacy track template
	 * @param SourceTrack				The originating track
	 */
	virtual void AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a set of remapped segments from a sub sequence to this template
	 *
	 * @param RootRange					The range in which to add the specified segments (at the root level)
	 * @param SegmentPtrs				The remapped segment ptrs
	 * @param Flags						Flags for these external segments
	 */
	virtual void AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs, ESectionEvaluationFlags Flags) = 0;

	/**
	 * Get a sequence's transform from its ID
	 *
	 * @param SequenceID				The ID of the sequence to get a transform for
	 * @return The sequence's transform
	 */
	virtual FMovieSceneSequenceTransform GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const = 0;

	/**
	 * Add the specified sub sequence data to this generator
	 *
	 * @param SequenceData				Data pertaining to the sequence to add
	 * @param ParentID					ID of the parent sequence
	 * @param SequenceID				Specific ID to use to identify this sub sequence in the template. Must not already exist in the template.
	 */
	virtual void AddSubSequence(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID, FMovieSceneSequenceID SequenceID) = 0;

protected:
	virtual ~IMovieSceneTemplateGenerator() { }
};

