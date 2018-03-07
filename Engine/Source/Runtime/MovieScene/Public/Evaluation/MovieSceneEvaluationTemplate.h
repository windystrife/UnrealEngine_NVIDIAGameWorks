// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneTrackIdentifier.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Containers/ArrayView.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "MovieSceneEvaluationTemplate.generated.h"

class UMovieSceneSequence;

USTRUCT()
struct FMovieSceneTrackIdentifiers
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMovieSceneTrackIdentifier> Data;
};


USTRUCT()
struct FMovieSceneTemplateGenerationLedger
{
	GENERATED_BODY()

public:

	TArrayView<const FMovieSceneTrackIdentifier> FindTracks(const FGuid& InSignature) const;

	void AddTrack(const FGuid& InSignature, FMovieSceneTrackIdentifier Identifier);

public:

	UPROPERTY()
	FMovieSceneTrackIdentifier LastTrackIdentifier;

	/** Map of track identifiers to number of references within th template (generally 1, maybe >1 for shared tracks) */
	UPROPERTY()
	TMap<FMovieSceneTrackIdentifier, int32> TrackReferenceCounts;

	/** Map of track signature to array of track identifiers that it created */
	UPROPERTY()
	TMap<FGuid, FMovieSceneTrackIdentifiers> TrackSignatureToTrackIdentifier;
};
template<> struct TStructOpsTypeTraits<FMovieSceneTemplateGenerationLedger> : public TStructOpsTypeTraitsBase2<FMovieSceneTemplateGenerationLedger> { enum { WithCopy = true }; };

/**
 * Template that is used for efficient runtime evaluation of a movie scene sequence. Potentially serialized into the asset.
 */
USTRUCT()
struct FMovieSceneEvaluationTemplate
{
	GENERATED_BODY()

	/**
	 * Default construction
	 */
	FMovieSceneEvaluationTemplate()
	{
		bHasLegacyTrackInstances = 0;
		bKeepStaleTracks = 0;
	}

	/**
	 * Move construction/assignment
	 */
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMovieSceneEvaluationTemplate(FMovieSceneEvaluationTemplate&&) = default;
	FMovieSceneEvaluationTemplate& operator=(FMovieSceneEvaluationTemplate&&) = default;
#else
	FMovieSceneEvaluationTemplate(FMovieSceneEvaluationTemplate&& RHS)
	{
		*this = MoveTemp(RHS);
	}
	FMovieSceneEvaluationTemplate& operator=(FMovieSceneEvaluationTemplate&& RHS)
	{
		Tracks = MoveTemp(RHS.Tracks);
		StaleTracks = MoveTemp(RHS.StaleTracks);
		EvaluationField = MoveTemp(RHS.EvaluationField);
		Hierarchy = MoveTemp(RHS.Hierarchy);
		Ledger = MoveTemp(RHS.Ledger);
		bHasLegacyTrackInstances = RHS.bHasLegacyTrackInstances;
		bKeepStaleTracks = RHS.bKeepStaleTracks;
		return *this;
	}
#endif

	/**
	 * Non-copyable
	 */
	FMovieSceneEvaluationTemplate(const FMovieSceneEvaluationTemplate&) = default;
	FMovieSceneEvaluationTemplate& operator=(const FMovieSceneEvaluationTemplate&) = default;

public:

	/**
	 * Reset this evaluation template, whilst ensuring that track identifiers do not get reused
	 */
	MOVIESCENE_API void ResetGeneratedData();

	/**
	 * Attempt to locate a track with the specified identifier
	 */
	FMovieSceneEvaluationTrack* FindTrack(FMovieSceneTrackIdentifier Identifier)
	{
		// Fast, most common path
		if (FMovieSceneEvaluationTrack* Track = Tracks.Find(Identifier.Value))
		{
			return Track;
		}

		return bKeepStaleTracks ? StaleTracks.Find(Identifier.Value) : nullptr;
	}

	/**
	 * Attempt to locate a track with the specified identifier
	 */
	const FMovieSceneEvaluationTrack* FindTrack(FMovieSceneTrackIdentifier Identifier) const
	{
		// Fast, most common path
		if (const FMovieSceneEvaluationTrack* Track = Tracks.Find(Identifier.Value))
		{
			return Track;
		}

		return bKeepStaleTracks ? StaleTracks.Find(Identifier.Value) : nullptr;
	}

	/**
	 * Test whether the specified track identifier relates to a stale track
	 */
	bool IsTrackStale(FMovieSceneTrackIdentifier Identifier) const
	{
		return bKeepStaleTracks ? StaleTracks.Contains(Identifier.Value) : false;
	}

	/**
	 * Add a new track for the specified identifier
	 */
	MOVIESCENE_API FMovieSceneTrackIdentifier AddTrack(const FGuid& InSignature, FMovieSceneEvaluationTrack&& InTrack);

	/**
	 * Remove any tracks that correspond to the specified signature
	 */
	MOVIESCENE_API void RemoveTrack(const FGuid& InSignature);

	/**
	 * Iterate this template's tracks.
	 */
	MOVIESCENE_API const TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& GetTracks() const;

	/**
	 * Iterate this template's tracks (non-const).
	 * NOTE that this is intended for use during the compilation phase in-editor. 
	 * Beware of using this to modify tracks afterwards as it will almost certainly break evaluation.
	 */
	MOVIESCENE_API TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& GetTracks();

	/**
	 * Find tracks within this template that relate to the specified signature
	 */
	MOVIESCENE_API TArrayView<const FMovieSceneTrackIdentifier> FindTracks(const FGuid& InSignature) const;

	/**
	 * Called after this template has been serialized in some way
	 */
	MOVIESCENE_API void PostSerialize(const FArchive& Ar);

	/**
	 * Purge any stale tracks we may have
	 */
	void PurgeStaleTracks()
	{
		StaleTracks.Reset();
	}

public:

	/**
	 * Get this template's generation ledger
	 */
	const FMovieSceneTemplateGenerationLedger& GetLedger() const
	{
		return TemplateLedger;
	}

private:

	/** Map of evaluation tracks from identifier to track */
	UPROPERTY()
	TMap<uint32, FMovieSceneEvaluationTrack> Tracks;

	/** Transient map of stale tracks. Only populated dureing regeneration where bKeepStaleTracks is true */
	TMap<uint32, FMovieSceneEvaluationTrack> StaleTracks;

public:

	/** Evaluation field for efficient runtime evaluation */
	UPROPERTY()
	FMovieSceneEvaluationField EvaluationField;

	/** Map of all sequences found in this template (recursively) */
	UPROPERTY()
	FMovieSceneSequenceHierarchy Hierarchy;

private:

	UPROPERTY()
	FMovieSceneTemplateGenerationLedger TemplateLedger;

public:

	/** When set, this template contains legacy track instances that require the initialization of a legacy sequence instance */
	UPROPERTY()
	uint32 bHasLegacyTrackInstances : 1;

	/** Primarily used in editor to keep stale tracks around during template regeneration to ensure we can call OnEndEvaluation on them. */
	UPROPERTY()
	uint32 bKeepStaleTracks : 1;
};
template<> struct TStructOpsTypeTraits<FMovieSceneEvaluationTemplate> : public TStructOpsTypeTraitsBase2<FMovieSceneEvaluationTemplate> { enum { WithPostSerialize = true }; };

USTRUCT()
struct FMovieSceneSequenceCachedSignature
{
	GENERATED_BODY()

	FMovieSceneSequenceCachedSignature() {}
	MOVIESCENE_API FMovieSceneSequenceCachedSignature(UMovieSceneSequence& InSequence);

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequence> Sequence;

	UPROPERTY()
	FGuid CachedSignature;
};

USTRUCT()
struct FCachedMovieSceneEvaluationTemplate : public FMovieSceneEvaluationTemplate
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	FCachedMovieSceneEvaluationTemplate()
		: Origin(nullptr)
	{
		bKeepStaleTracks = 1;
	}

	FCachedMovieSceneEvaluationTemplate(UMovieSceneSequence& InSequence)
		: Origin(nullptr)
	{
		bKeepStaleTracks = 1;
		Initialize(InSequence);
	}

	MOVIESCENE_API void Initialize(UMovieSceneSequence& InSequence, FMovieSceneSequenceTemplateStore* InOrigin = nullptr);

	MOVIESCENE_API void Regenerate();

	MOVIESCENE_API void Regenerate(const FMovieSceneTrackCompilationParams& NewParams);

	MOVIESCENE_API void ForceRegenerate(const FMovieSceneTrackCompilationParams& NewParams);

	MOVIESCENE_API bool IsOutOfDate(const FMovieSceneTrackCompilationParams& NewParams) const;

private:

	void RegenerateImpl(const FMovieSceneTrackCompilationParams& NewParams);

	/** Transient data */
	TWeakObjectPtr<UMovieSceneSequence> SourceSequence;
	FMovieSceneSequenceTemplateStore* Origin;

	UPROPERTY()
	FMovieSceneTrackCompilationParams CachedCompilationParams;

	UPROPERTY()
	TArray<FMovieSceneSequenceCachedSignature> CachedSignatures;
#endif
};

template<> struct TStructOpsTypeTraits<FCachedMovieSceneEvaluationTemplate> : public TStructOpsTypeTraitsBase2<FCachedMovieSceneEvaluationTemplate> { enum { WithCopy = false, WithPostSerialize = true }; };
