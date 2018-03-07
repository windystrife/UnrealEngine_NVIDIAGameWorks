// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "MovieSceneSequenceID.h"
#include "Containers/ArrayView.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Compilation/MovieSceneSegmentCompiler.h"

class IMovieSceneModule;
class UMovieSceneSequence;

/**
 * Class responsible for generating up-to-date evaluation template data
 */
class FMovieSceneEvaluationTemplateGenerator final : IMovieSceneTemplateGenerator
{
public:

	/**
	 * Construction from a sequence and a destination template
	 */
	FMovieSceneEvaluationTemplateGenerator(UMovieSceneSequence& InSequence, FMovieSceneEvaluationTemplate& OutTemplate, FMovieSceneSequenceTemplateStore& InStore);

	/**
	 * Generate the template with the specified parameters
	 *
	 * @param InParams 			Compilation parameters
	 */
	void Generate(FMovieSceneTrackCompilationParams InParams);

private:

	/**
	 * Process a track, potentially causing it to be compiled if it's out of date
	 *
	 * @param InTrack 			The track to process
	 * @param ObjectId 			Object binding ID of the track
	 */
	void ProcessTrack(const UMovieSceneTrack& InTrack, const FGuid& ObjectId = FGuid());


	/**
	 * Remove any references to old track templates that are no longer required to exist
	 */
	void RemoveOldTrackReferences();

	/**
	 * Update the evaluation field with the specified segments and segment ptrs
	 *
	 * @param Segments 			The segments to populate the evaluation field with
	 * @param Impls 			Array of implementation pointers that each segment refers to
	 */
	void UpdateEvaluationField(const TArray<FMovieSceneSegment>& Segments, const TArray<FMovieSceneEvaluationFieldSegmentPtr>& Impls, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates);

	/**
	 * Initialize segment meta-data for the specified group
	 *
	 * @param MetaData			Meta-data container for the group
	 * @param Group				The evaluation group to determine meta data for
	 * @param Templates			Template store for locating tracks
	 */
	void InitializeMetaData(FMovieSceneEvaluationMetaData& MetaData, FMovieSceneEvaluationGroup& Group, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates);

private:

	/** IMovieSceneTemplateGenerator overrides */
	virtual void AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) override;
	virtual void AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) override;
	virtual void AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack) override;
	virtual void AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs, ESectionEvaluationFlags Flags) override;
	virtual FMovieSceneSequenceTransform GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const override;
	virtual void AddSubSequence(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID, FMovieSceneSequenceID SequenceID) override;

private:

	/**
	 * Sort predicate used to sort evaluation tracks in a given group
	 */
	bool SortPredicate(const FMovieSceneEvaluationFieldTrackPtr& InPtrA, const FMovieSceneEvaluationFieldTrackPtr& InPtrB, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates, IMovieSceneModule& MovieSceneModule);

	/**
	 * Attempt to find a track from its evaluation field pointer
	 *
	 * @param InPtr 			Ptr to the track to lookup
	 * @return The track, or nullptr
	 */
	const FMovieSceneEvaluationTrack* LookupTrack(const FMovieSceneEvaluationFieldTrackPtr& InPtr, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates);

private:

	/** The source sequence we are generating a template for */
	UMovieSceneSequence& SourceSequence;
	/** The destination template we're generating into */
	FMovieSceneEvaluationTemplate& Template;

	/** Transient compiler args that are passed to each track that is compiled */
	FMovieSceneTrackCompilerArgs TransientArgs;

	/** Temporary set of signatures we've come across in the sequence we're compiling */
	TSet<FGuid> CompiledSignatures;

	/** Segment data used for evaluation field generation */
	TArray<FMovieSceneSectionData> SegmentData;
	/** Look up table whose indices match implementation indices in the above segments */
	TArray<FMovieSceneEvaluationFieldSegmentPtr> TrackLUT;

	/** Map of external segments added from sub sequences */
	struct FExternalSegment
	{
		int32 Index;
		ESectionEvaluationFlags Flags;
	};
	TMultiMap<FMovieSceneEvaluationFieldSegmentPtr, FExternalSegment> ExternalSegmentLookup;

	/** Set of shared track keys that have been added (used to ensure we only add a shared track ID once) */
	TSet<FSharedPersistentDataKey> AddedSharedTracks;
};
