// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Compilation/MovieSceneSegmentCompiler.h"

namespace MovieSceneSegmentCompiler
{
	static TOptional<FMovieSceneSegment> EvaluateNearestSegment(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment)
	{
		if (PreviousSegment)
		{
			// There is a preceeding segment
			float PreviousSegmentRangeBound = PreviousSegment->Range.GetUpperBoundValue();

			FMovieSceneSegment EmptySpace(Range);
			for (FSectionEvaluationData Data : PreviousSegment->Impls)
			{
				EmptySpace.Impls.Add(FSectionEvaluationData(Data.ImplIndex, PreviousSegmentRangeBound));
			}
			return EmptySpace;
		}
		else if (NextSegment)
		{
			// Before any sections
			float NextSegmentRangeBound = NextSegment->Range.GetLowerBoundValue();

			FMovieSceneSegment EmptySpace(Range);
			for (FSectionEvaluationData Data : NextSegment->Impls)
			{
				EmptySpace.Impls.Add(FSectionEvaluationData(Data.ImplIndex, NextSegmentRangeBound));
			}
			return EmptySpace;
		}

		return TOptional<FMovieSceneSegment>();
	}

	static void BlendSegmentHighPass(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData)
	{
		if (!Segment.Impls.Num())
		{
			return;
		}

		int32 HighestPriority = TNumericLimits<int32>::Lowest();
		for (const FSectionEvaluationData& Impl : Segment.Impls)
		{
			const FMovieSceneSectionData& SectionData = SourceData[Impl.ImplIndex];
			if (!SectionData.BlendType.IsValid() &&
				!SectionData.EvalData.IsPreRoll() &&
				!SectionData.EvalData.IsPostRoll())
			{
				HighestPriority = FMath::Max(HighestPriority, SectionData.Priority);
			}
		}

		// Remove anything that's not the highest priority, (excluding pre/postroll sections)
		for (int32 RemoveAtIndex = Segment.Impls.Num() - 1; RemoveAtIndex >= 0; --RemoveAtIndex)
		{
			const FMovieSceneSectionData& SectionData = SourceData[Segment.Impls[RemoveAtIndex].ImplIndex];
			if (SectionData.Priority != HighestPriority &&
				!SectionData.BlendType.IsValid() &&
				!SectionData.EvalData.IsPreRoll() && 
				!SectionData.EvalData.IsPostRoll())
			{
				Segment.Impls.RemoveAt(RemoveAtIndex, 1, false);
			}
		}
	}

	// Reduces the evaluated sections to only the section that resides last in the source data. Legacy behaviour from various track instances.
	static void BlendSegmentLegacySectionOrder(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData)
	{
		if (Segment.Impls.Num() <= 1)
		{
			return;
		}

		Segment.Impls.Sort(
			[&](const FSectionEvaluationData& A, const FSectionEvaluationData& B)
			{
				return A.ImplIndex > B.ImplIndex;
			}
		);
		Segment.Impls.SetNum(1, false);
	}
}

class FMovieSceneAdditiveCameraRules : public FMovieSceneSegmentCompilerRules
{
public:
	FMovieSceneAdditiveCameraRules(const UMovieSceneTrack* InTrack)
		: Sections(InTrack->GetAllSections())
	{
	}

	virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
	{
		// sort by start time to match application order of player camera
		Segment.Impls.Sort(
			[&](FSectionEvaluationData A, FSectionEvaluationData B){
				UMovieSceneSection* SectionA = Sections[SourceData[A.ImplIndex].EvalData.ImplIndex];
				UMovieSceneSection* SectionB = Sections[SourceData[B.ImplIndex].EvalData.ImplIndex];

				if (SectionA->IsInfinite())
				{
					return true;
				}
				else if (SectionB->IsInfinite())
				{
					return false;
				}
				else
				{
					return SectionA->GetStartTime() < SectionB->GetStartTime();
				}
			}
		);
	}

	const TArray<UMovieSceneSection*>& Sections;
};
