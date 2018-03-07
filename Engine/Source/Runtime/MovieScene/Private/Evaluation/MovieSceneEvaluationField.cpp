// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationField.h"
#include "Algo/Sort.h"

int32 FMovieSceneEvaluationField::GetSegmentFromTime(float Time) const
{
	// Linear search
	// @todo: accelerated search based on the last evaluated index?
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Contains(Time))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

TRange<int32> FMovieSceneEvaluationField::OverlapRange(TRange<float> Range) const
{
	int32 StartIndex = 0, Num = 0;
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Overlaps(Range))
		{
			if (Num == 0)
			{
				StartIndex = Index;
			}
			++Num;
		}
		else if (Num != 0)
		{
			break;
		}
	}

	return Num != 0 ? TRange<int32>(StartIndex, StartIndex + Num) : TRange<int32>::Empty();
}

void FMovieSceneEvaluationMetaData::RemapSequenceIDsForRoot(FMovieSceneSequenceID OverrideRootID)
{
	if (OverrideRootID == MovieSceneSequenceID::Root)
	{
		return;
	}

	for (FMovieSceneSequenceID& SequenceID : ActiveSequences)
	{
		SequenceID = SequenceID.AccumulateParentID(OverrideRootID);
	}
	for (FMovieSceneOrderedEvaluationKey& OrderedKey : ActiveEntities)
	{
		OrderedKey.Key.SequenceID = OrderedKey.Key.SequenceID.AccumulateParentID(OverrideRootID);
	}
}

void FMovieSceneEvaluationMetaData::DiffSequences(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneSequenceID>* NewSequences, TArray<FMovieSceneSequenceID>* ExpiredSequences) const
{
	// This algorithm works on the premise that each array is sorted, and each ID can only appear once
	auto ThisFrameIDs = ActiveSequences.CreateConstIterator();
	auto LastFrameIDs = LastFrame.ActiveSequences.CreateConstIterator();

	// Iterate both arrays together
	while (ThisFrameIDs && LastFrameIDs)
	{
		FMovieSceneSequenceID ThisID = *ThisFrameIDs;
		FMovieSceneSequenceID LastID = *LastFrameIDs;

		// If they're the same, skip
		if (ThisID == LastID)
		{
			++ThisFrameIDs;
			++LastFrameIDs;
			continue;
		}

		if (LastID < ThisID)
		{
			// Last frame iterator is less than this frame's, which means the last ID is no longer evaluated
			if (ExpiredSequences)
			{
				ExpiredSequences->Add(LastID);
			}
			++LastFrameIDs;
		}
		else
		{
			// Last frame iterator is greater than this frame's, which means this ID is new
			if (NewSequences)
			{
				NewSequences->Add(ThisID);
			}

			++ThisFrameIDs;
		}
	}

	// Add any remaining expired sequences
	if (ExpiredSequences)
	{
		while (LastFrameIDs)
		{
			ExpiredSequences->Add(*LastFrameIDs);
			++LastFrameIDs;
		}
	}

	// Add any remaining new sequences
	if (NewSequences)
	{
		while (ThisFrameIDs)
		{
			NewSequences->Add(*ThisFrameIDs);
			++ThisFrameIDs;
		}
	}
}

void FMovieSceneEvaluationMetaData::DiffEntities(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneOrderedEvaluationKey>* NewKeys, TArray<FMovieSceneOrderedEvaluationKey>* ExpiredKeys) const
{
	// This algorithm works on the premise that each array is sorted, and each ID can only appear once
	auto ThisFrameKeys = ActiveEntities.CreateConstIterator();
	auto LastFrameKeys = LastFrame.ActiveEntities.CreateConstIterator();

	// Iterate both arrays together
	while (ThisFrameKeys && LastFrameKeys)
	{
		FMovieSceneOrderedEvaluationKey ThisKey = *ThisFrameKeys;
		FMovieSceneOrderedEvaluationKey LastKey = *LastFrameKeys;

		// If they're the same, skip
		if (ThisKey.Key == LastKey.Key)
		{
			++ThisFrameKeys;
			++LastFrameKeys;
			continue;
		}

		if (LastKey.Key < ThisKey.Key)
		{
			// Last frame iterator is less than this frame's, which means the last entity is no longer evaluated
			if (ExpiredKeys)
			{
				ExpiredKeys->Add(LastKey);
			}
			++LastFrameKeys;
		}
		else
		{
			// Last frame iterator is greater than this frame's, which means this entity is new
			if (NewKeys)
			{
				NewKeys->Add(ThisKey);
			}

			++ThisFrameKeys;
		}
	}

	// Add any remaining expired entities
	if (ExpiredKeys)
	{
		while (LastFrameKeys)
		{
			ExpiredKeys->Add(*LastFrameKeys);
			++LastFrameKeys;
		}

		// Expired keys are torn down in reverse
		Algo::SortBy(*ExpiredKeys, [](const FMovieSceneOrderedEvaluationKey& In){ return uint32(-1) - In.EvaluationIndex; });
	}

	// Add any remaining new entities
	if (NewKeys)
	{
		while (ThisFrameKeys)
		{
			NewKeys->Add(*ThisFrameKeys);
			++ThisFrameKeys;
		}

		Algo::SortBy(*NewKeys, &FMovieSceneOrderedEvaluationKey::EvaluationIndex);
	}
}
