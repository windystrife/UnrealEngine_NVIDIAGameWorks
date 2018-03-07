// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneEvaluationKey.h"
#include "Evaluation/MovieSceneTrackIdentifier.h"
#include "MovieSceneEvaluationField.generated.h"

/** A pointer to a track held within an evaluation template */
USTRUCT()
struct FMovieSceneEvaluationFieldTrackPtr
{
	GENERATED_BODY()

	/**
	 * Default constructor
	 */
	FMovieSceneEvaluationFieldTrackPtr(){}

	/**
	 * Construction from a sequence ID, and the index of the track within that sequence's track list
	 */
	FMovieSceneEvaluationFieldTrackPtr(FMovieSceneSequenceIDRef InSequenceID, FMovieSceneTrackIdentifier InTrackIdentifier)
		: SequenceID(InSequenceID)
		, TrackIdentifier(InTrackIdentifier)
	{}

	/**
	 * Check for equality
	 */
	friend bool operator==(FMovieSceneEvaluationFieldTrackPtr A, FMovieSceneEvaluationFieldTrackPtr B)
	{
		return A.TrackIdentifier == B.TrackIdentifier && A.SequenceID == B.SequenceID;
	}

	/**
	 * Get a hashed representation of this type
	 */
	friend uint32 GetTypeHash(FMovieSceneEvaluationFieldTrackPtr LHS)
	{
		return HashCombine(GetTypeHash(LHS.TrackIdentifier), GetTypeHash(LHS.SequenceID));
	}

	/** The sequence ID that identifies to which sequence the track belongs */
	UPROPERTY()
	FMovieSceneSequenceID SequenceID;

	/** The Identifier of the track inside the track map (see FMovieSceneEvaluationTemplate::Tracks) */
	UPROPERTY()
	FMovieSceneTrackIdentifier TrackIdentifier;
};

/** A pointer to a particular segment of a track held within an evaluation template */
USTRUCT()
struct FMovieSceneEvaluationFieldSegmentPtr : public FMovieSceneEvaluationFieldTrackPtr
{
	GENERATED_BODY()

	/**
	 * Default constructor
	 */
	FMovieSceneEvaluationFieldSegmentPtr(){}

	/**
	 * Construction from a sequence ID, and the index of the track within that sequence's track list
	 */
	FMovieSceneEvaluationFieldSegmentPtr(FMovieSceneSequenceIDRef InSequenceID, FMovieSceneTrackIdentifier InTrackIdentifier, int32 InSegmentIndex)
		: FMovieSceneEvaluationFieldTrackPtr(InSequenceID, InTrackIdentifier)
		, SegmentIndex(InSegmentIndex)
	{}

	/**
	 * Check for equality
	 */
	friend bool operator==(FMovieSceneEvaluationFieldSegmentPtr A, FMovieSceneEvaluationFieldSegmentPtr B)
	{
		return A.SegmentIndex == B.SegmentIndex && A.TrackIdentifier == B.TrackIdentifier && A.SequenceID == B.SequenceID;
	}

	/**
	 * Get a hashed representation of this type
	 */
	friend uint32 GetTypeHash(FMovieSceneEvaluationFieldSegmentPtr LHS)
	{
		return HashCombine(LHS.SegmentIndex, GetTypeHash(static_cast<FMovieSceneEvaluationFieldTrackPtr&>(LHS)));
	}

	/** The index of the segment within the track (see FMovieSceneEvaluationTrack::Segments) */
	UPROPERTY()
	int32 SegmentIndex;
};

/** Lookup table index for a group of evaluation templates */
USTRUCT()
struct FMovieSceneEvaluationGroupLUTIndex
{
	GENERATED_BODY()

	FMovieSceneEvaluationGroupLUTIndex()
		: LUTOffset(0)
		, NumInitPtrs(0)
		, NumEvalPtrs(0)
	{}

	/** The offset within FMovieSceneEvaluationGroup::SegmentPtrLUT that this index starts */
	UPROPERTY()
	int32 LUTOffset;

	/** The number of initialization pointers are stored after &FMovieSceneEvaluationGroup::SegmentPtrLUT[0] + LUTOffset. */
	UPROPERTY()
	int32 NumInitPtrs;

	/** The number of evaluation pointers are stored after &FMovieSceneEvaluationGroup::SegmentPtrLUT[0] + LUTOffset + NumInitPtrs. */
	UPROPERTY()
	int32 NumEvalPtrs;
};

/** Holds segment pointers for all segments that are active for a given range of the sequence */
USTRUCT()
struct FMovieSceneEvaluationGroup
{
	GENERATED_BODY()

	/** Array of indices that define all the flush groups in the range. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationGroupLUTIndex> LUTIndices;

	/** A grouping of evaluation pointers that occur in this range of the sequence */
	UPROPERTY()
	TArray<FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrLUT;
};

/** Struct that stores the key for an evaluated entity, and the index at which it was (or is to be) evaluated */
USTRUCT()
struct FMovieSceneOrderedEvaluationKey
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieSceneEvaluationKey Key;

	UPROPERTY()
	uint32 EvaluationIndex;
};

/** Informational meta-data that applies to a given time range */
USTRUCT()
struct FMovieSceneEvaluationMetaData
{
	GENERATED_BODY()

	FMovieSceneEvaluationMetaData() = default;

	FMovieSceneEvaluationMetaData(const FMovieSceneEvaluationMetaData&) = default;
	FMovieSceneEvaluationMetaData& operator=(const FMovieSceneEvaluationMetaData&) = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMovieSceneEvaluationMetaData(FMovieSceneEvaluationMetaData&&) = default;
	FMovieSceneEvaluationMetaData& operator=(FMovieSceneEvaluationMetaData&&) = default;
#else
	FMovieSceneEvaluationMetaData(FMovieSceneEvaluationMetaData&& RHS) : ActiveSequences(MoveTemp(RHS.ActiveSequences)), ActiveEntities(MoveTemp(RHS.ActiveEntities)) {}
	FMovieSceneEvaluationMetaData& operator=(FMovieSceneEvaluationMetaData&& RHS) { ActiveSequences = MoveTemp(RHS.ActiveSequences); ActiveEntities = MoveTemp(RHS.ActiveEntities); return *this; }
#endif

	/**
	 * Reset this meta-data
	 */
	void Reset()
	{
		ActiveSequences.Reset();
		ActiveEntities.Reset();
	}

	/**
	 * Remap this meta-data onto a different parent ID
	 *
	 * @param OverrideRootID		The parent ID to remap entities onto
	 */
	void RemapSequenceIDsForRoot(FMovieSceneSequenceID OverrideRootID);

	/**
	 * Diff the active sequences this frame, with the specified previous frame's meta-data
	 *
	 * @param LastFrame				Meta-data pertaining to the last frame
	 * @param NewSequences			(Optional) Ptr to an array that will be populated with sequences that are new this frame
	 * @param ExpiredSequences		(Optional) Ptr to an array that will be populated with sequences that are no longer being evaluated
	 */
	void DiffSequences(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneSequenceID>* NewSequences, TArray<FMovieSceneSequenceID>* ExpiredSequences) const;

	/**
	 * Diff the active entities (tracks and sections) this frame, with the specified previous frame's meta-data
	 *
	 * @param LastFrame				Meta-data pertaining to the last frame
	 * @param NewKeys				(Optional) Ptr to an array that will be populated with entities that are new this frame
	 * @param ExpiredKeys			(Optional) Ptr to an array that will be populated with entities that are no longer being evaluated
	 */
	void DiffEntities(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneOrderedEvaluationKey>* NewKeys, TArray<FMovieSceneOrderedEvaluationKey>* ExpiredKeys) const;

	/** Array of sequences that are active in this time range. */
	UPROPERTY()
	TArray<FMovieSceneSequenceID> ActiveSequences;

	/** Array of entities (tracks and/or sections) that are active in this time range. */
	UPROPERTY()
	TArray<FMovieSceneOrderedEvaluationKey> ActiveEntities;
};

/**
 * Memory layout optimized primarily for speed of searching the applicable ranges
 */
USTRUCT()
struct FMovieSceneEvaluationField
{
	GENERATED_BODY()

	/**
	 * Efficiently find the entry that exists at the specified time, if any
	 *
	 * @param Time			The time at which to seach
	 * @return The index within Ranges, Groups and MetaData that the current time resides, or INDEX_NONE if there is nothing to do at the requested time
	 */
	MOVIESCENE_API int32 GetSegmentFromTime(float Time) const;

	/**
	 * Deduce the indices into Ranges and Groups that overlap with the specified time range
	 *
	 * @param Range			The range to overlap with our field
	 * @return A range of indices into Ranges and Groups that overlap with the requested range
	 */
	MOVIESCENE_API TRange<int32> OverlapRange(TRange<float> Range) const;

	/** Ranges stored separately for fast (cache efficient) lookup. Each index has a corresponding entry in FMovieSceneEvaluationField::Groups. */
	UPROPERTY()
	TArray<FFloatRange> Ranges;

	/** Groups that store segment pointers for each of the above ranges. Each index has a corresponding entry in FMovieSceneEvaluationField::Ranges. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationGroup> Groups;

	/** Meta data that maps to entries in the 'Ranges' array. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationMetaData> MetaData;
};
