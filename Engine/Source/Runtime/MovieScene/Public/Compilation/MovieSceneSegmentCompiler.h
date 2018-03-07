// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneSegment.h"
#include "MovieSceneSection.h"
#include "MovieSceneBlendType.h"

/** Data structure supplied to the segment compiler that represents a section range, evaluation data (including the section's index), blend type and a priority */
struct FMovieSceneSectionData
{
	MOVIESCENE_API FMovieSceneSectionData(const TRange<float>& InBounds, FSectionEvaluationData InEvalData, FOptionalMovieSceneBlendType InBlendType = FOptionalMovieSceneBlendType(), int32 InPriority = 0);

	/** The time range in which this section is considered active */
	TRange<float> Bounds;
	/** Evaluation data with which the section is to be evaluated */
	FSectionEvaluationData EvalData;
	/** Optional blend type for the section */
	FOptionalMovieSceneBlendType BlendType;
	/** Priority for the data (ie OverlapPriority where this represents sections in a row, or row index in the case of rows in a track) */
	int32 Priority;
};

/** Structure that defines how the segment compiler should combine overlapping sections, insert empty space, sort evaluation order, and other arbitrary processing. */
struct FMovieSceneSegmentCompilerRules
{
	/** Default constructor */
	FMovieSceneSegmentCompilerRules()
	{
		bAllowEmptySegments = false;
	}

	virtual ~FMovieSceneSegmentCompilerRules() {}

	/**
	 * Public entry point to process an array of compiled segments using the specified SourceData
	 *
	 * @param Segments				(In/Out) Mutable array of compiled segments to be processed
	 * @param SourceData			The source section data that is being compiled.
	 */
	MOVIESCENE_API void ProcessSegments(TArray<FMovieSceneSegment>& Segments, const TArrayView<const FMovieSceneSectionData>& SourceData) const;

	/**
	 * Check whether the resulting segments be empty
	 */
	bool AllowEmptySegments() const
	{
		return bAllowEmptySegments;
	}

	/**
	 * Implementation function to insert empty space between two other segments or at the start/end
	 *
	 * @param Range					The time range for the potential new segment
	 * @param PreviousSegment		A pointer to the previous segment if there is one. Null if the empty space is before the first segment.
	 * @param Next					A pointer to the subsequent segment if there is one. Null if the empty space is after the last segment.
	 *
	 * @return An optional new segment to define
	 */
	virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const
	{
		return TOptional<FMovieSceneSegment>();
	}

	/**
	 * Blend the specified segment by performing some specific processing such as sorting or filtering 
	 *
	 * @param Segment				The segment to process. Segment.Impls[].ImplIndex relates to indices with SourceData
	 * @param SourceData			The source section data that is being compiled.
	 */
	virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
	{
	}

	/**
	 * Called after all segments have been calculated and blended for any additional processing
	 *
	 * @param Segments				The segments to process. Segments[].Impls[].ImplIndex relates to indices with SourceData
	 * @param SourceData			The source section data that is being compiled.
	 */
	virtual void PostProcessSegments(TArray<FMovieSceneSegment>& Segments, const TArrayView<const FMovieSceneSectionData>& SourceData) const
	{
	}

protected:

	/** Private implementation function for inserting a segment at the specified index */
	bool InsertSegment(TArray<FMovieSceneSegment>& Segments, int32 Index, const TArrayView<const FMovieSceneSectionData>& SourceData) const;

	/** Whether we allow empty segments or not */
	bool bAllowEmptySegments;
};

/** Enumeration specifying how impl indices should be specified in resulting FMovieSceneSegments */
enum class EMovieSceneSegmentIndexSpace : uint8
{
	/** FSectionEvaluationData::ImplIndex should point to indices within the original source data */
	SourceDataIndex,
	/** FSectionEvaluationData::ImplIndex should match those specified within the original source data */
	ActualImplIndex,
};

/** Movie scene segment compiler that takes an unordered, arbitrary array of section data, and produces an ordered array of segments that define which implementations occur at which time ranges */
struct FMovieSceneSegmentCompiler
{
	/**
	 * Compiles the specified source data into an array of segments
	 *
	 * @param SourceData 			Unordered data to compile. May contain duplicates or overlapping time ranges.
	 * @param Rules 				(Optional) Compiler rulse structure to compile with
	 * @param IndexSpace 			(Optional) How the resulting segments should relate to the source data (either indices into the data, or actual indices specified from the data)
	 *
	 * @return An ordered array of segments that define unique combinations of overlapping source data. Segments are guaranteed to not overlap, may be contiguous and may contain gaps depending on compiler rules
	 */
	MOVIESCENE_API TArray<FMovieSceneSegment> Compile(TArrayView<const FMovieSceneSectionData> Data, const FMovieSceneSegmentCompilerRules* Rules = nullptr, EMovieSceneSegmentIndexSpace IndexSpace = EMovieSceneSegmentIndexSpace::ActualImplIndex);

private:

	/**
	 * Add any EvalData currently considered as overlapping to their relevant segments
	 */
	void CloseCompletedSegments();

	/**
	 * Find and index into OverlappingSections where the supplied eval data is considered to match
	 *
	 * @param InEvalData 		Evaluation data to find an index for
	 * @return A valid index into OverlappingSections, or INDEX_NONE
	 */
	int32 FindOverlappingIndex(FSectionEvaluationData InEvalData) const;

	/** Private structure that specifies where a particular evaluation starts or ends */
	struct FBound
	{
		FBound(FSectionEvaluationData InEvalData, TRangeBound<float> InBound) : EvalData(InEvalData), Bound(InBound) {}

		FSectionEvaluationData EvalData;
		TRangeBound<float> Bound;
	};

	/** Current read indices for the lower and upper bounds */
	int32 LowerReadIndex, UpperReadIndex;
	/** Arrays of lower and upper bounds sorted ascending and descending respectively. */
	TArray<FBound> LowerBounds, UpperBounds;
	/** Ordered array of compiled segments */
	TArray<FMovieSceneSegment> CompiledSegments;
	/** Section evaluation data for sections that are being evaluated at the current time of processing */
	TArray<FSectionEvaluationData, TInlineAllocator<16>> OverlappingSections;
	/** Reference counts for any of the entries in OverlappingSections that map to the same object (necessary for when the same eval data is specified in multiple source data elements) */
	TArray<uint32, TInlineAllocator<16>> OverlappingRefCounts;
	/** The index space that we are generating for */
	EMovieSceneSegmentIndexSpace IndexSpace;
	/** The source section data */
	TArrayView<const FMovieSceneSectionData> SourceData;
};

/** Resulting structure from FMovieSceneTrackCompiler::Compile */
struct FMovieSceneTrackEvaluationField
{
	/** The compiled segments, ordered by time range, with no overlaps. May contain empty space */
	TArray<FMovieSceneSegment> Segments;
};

/** Structure used for compiling a UMovieSceneTrack using specific rules for compiling its rows, and combining such rows */
struct FMovieSceneTrackCompiler
{
	/** A row in a track */
	struct FRow
	{
		/** All the sections contained in the row */
		TArray<FMovieSceneSectionData, TInlineAllocator<8>> Sections;

		/** Compiler rules used to compile this row */
		const FMovieSceneSegmentCompilerRules* CompileRules;
	};

	struct FRows
	{
		/** Construction from an unordered array of sections, and an optional compiler rules object for compiling each row */
		MOVIESCENE_API FRows(const TArray<UMovieSceneSection*>& Sections, const FMovieSceneSegmentCompilerRules* CompileRules = nullptr);
		
		/** Array of rows */
		TArray<FRow, TInlineAllocator<2>> Rows;
	};

	/**
	 * Compile the specified rows using the specified compiler rules for combining each row
	 *
	 * @param Rows 			The rows to compile
	 * @param Rules 		Compiler rules that define how the rows should be blended together
	 *
	 * @return An ordered array of segments that define unique combinations of overlapping sections. Segments are guaranteed to not overlap, may be contiguous and may contain gaps depending on compiler rules
	 */
	MOVIESCENE_API FMovieSceneTrackEvaluationField Compile(TArrayView<FRow> Rows, const FMovieSceneSegmentCompilerRules* Rules = nullptr);
};



