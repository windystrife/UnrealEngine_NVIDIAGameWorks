// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "MovieSceneSequence.h"

FMovieSceneSequenceCachedSignature::FMovieSceneSequenceCachedSignature(UMovieSceneSequence& InSequence)
	: Sequence(&InSequence)
	, CachedSignature(InSequence.GetSignature())
{
}

#if WITH_EDITORONLY_DATA

void FCachedMovieSceneEvaluationTemplate::Initialize(UMovieSceneSequence& InSequence, FMovieSceneSequenceTemplateStore* InOrigin)
{
	SourceSequence = &InSequence;
	Origin = InOrigin;
}

void FCachedMovieSceneEvaluationTemplate::Regenerate()
{
	Regenerate(CachedCompilationParams);
}

void FCachedMovieSceneEvaluationTemplate::Regenerate(const FMovieSceneTrackCompilationParams& Params)
{
	if (IsOutOfDate(Params))
	{
		RegenerateImpl(Params);
	}
}

void FCachedMovieSceneEvaluationTemplate::ForceRegenerate(const FMovieSceneTrackCompilationParams& Params)
{
	ResetGeneratedData();
	RegenerateImpl(Params);
}

void FCachedMovieSceneEvaluationTemplate::RegenerateImpl(const FMovieSceneTrackCompilationParams& Params)
{
	if (Params.bDuringBlueprintCompile != CachedCompilationParams.bDuringBlueprintCompile)
	{
		ResetGeneratedData();
	}

	CachedSignatures.Reset();
	CachedCompilationParams = Params;

	if (UMovieSceneSequence* Sequence = SourceSequence.Get())
	{
		FMovieSceneSequenceTemplateStore DefaultStore;
		Sequence->GenerateEvaluationTemplate(*this, Params, Origin ? *Origin : DefaultStore);

		CachedSignatures.Add(FMovieSceneSequenceCachedSignature(*Sequence));
		for (auto& Pair : Hierarchy.AllSubSequenceData())
		{
			if (UMovieSceneSequence* SubSequence = Pair.Value.Sequence)
			{
				CachedSignatures.Add(FMovieSceneSequenceCachedSignature(*SubSequence));
			}
		}
	}
}

bool FCachedMovieSceneEvaluationTemplate::IsOutOfDate(const FMovieSceneTrackCompilationParams& Params) const
{
	if (Params != CachedCompilationParams || CachedSignatures.Num() == 0)
	{
		return true;
	}

	// out of date if the cached signautres contains any out of date sigs
	return CachedSignatures.ContainsByPredicate(
		[](const FMovieSceneSequenceCachedSignature& Sig){
			UMovieSceneSequence* Sequence = Sig.Sequence.Get();
			return !Sequence || Sequence->GetSignature() != Sig.CachedSignature;
		}
	);
}

#endif // WITH_EDITORONLY_DATA

TArrayView<const FMovieSceneTrackIdentifier> FMovieSceneTemplateGenerationLedger::FindTracks(const FGuid& InSignature) const
{
	if (auto* Tracks = TrackSignatureToTrackIdentifier.Find(InSignature))
	{
		return Tracks->Data;
	}
	return TArrayView<FMovieSceneTrackIdentifier>();
}

void FMovieSceneTemplateGenerationLedger::AddTrack(const FGuid& InSignature, FMovieSceneTrackIdentifier Identifier)
{
	TrackSignatureToTrackIdentifier.FindOrAdd(InSignature).Data.Add(Identifier);
	++TrackReferenceCounts.FindOrAdd(Identifier);
}

void FMovieSceneEvaluationTemplate::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		for (auto& Pair : Tracks)
		{
			if (TemplateLedger.LastTrackIdentifier == FMovieSceneTrackIdentifier::Invalid() || TemplateLedger.LastTrackIdentifier.Value < Pair.Key)
			{
				// Reset previously serialized, invalid data
				ResetGeneratedData();
				break;
			}
		}
	}
}

void FMovieSceneEvaluationTemplate::ResetGeneratedData()
{
	TemplateLedger.TrackSignatureToTrackIdentifier.Reset();
	TemplateLedger.TrackReferenceCounts.Reset();

	Tracks.Reset();
	StaleTracks.Reset();
	EvaluationField = FMovieSceneEvaluationField();
	Hierarchy = FMovieSceneSequenceHierarchy();
	bHasLegacyTrackInstances = false;
}

FMovieSceneTrackIdentifier FMovieSceneEvaluationTemplate::AddTrack(const FGuid& InSignature, FMovieSceneEvaluationTrack&& InTrack)
{
	FMovieSceneTrackIdentifier NewIdentifier = ++TemplateLedger.LastTrackIdentifier;
	
	InTrack.SetupOverrides();
	Tracks.Add(NewIdentifier.Value, MoveTemp(InTrack));
	TemplateLedger.AddTrack(InSignature, NewIdentifier);

	return NewIdentifier;
}

void FMovieSceneEvaluationTemplate::RemoveTrack(const FGuid& InSignature)
{
	for (FMovieSceneTrackIdentifier TrackIdentifier : TemplateLedger.FindTracks(InSignature))
	{
		int32* RefCount = TemplateLedger.TrackReferenceCounts.Find(TrackIdentifier);
		if (ensure(RefCount) && --(*RefCount) == 0)
		{
			if (bKeepStaleTracks)
			{
				if (FMovieSceneEvaluationTrack* Track = Tracks.Find(TrackIdentifier.Value))
				{
					StaleTracks.Add(TrackIdentifier.Value, MoveTemp(*Track));
				}
			}
			
			Tracks.Remove(TrackIdentifier.Value);
			TemplateLedger.TrackReferenceCounts.Remove(TrackIdentifier);
		}
	}
	TemplateLedger.TrackSignatureToTrackIdentifier.Remove(InSignature);
}

const TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& FMovieSceneEvaluationTemplate::GetTracks() const
{
	// Reinterpret the uint32 as FMovieSceneTrackIdentifier
	static_assert(sizeof(FMovieSceneTrackIdentifier) == sizeof(uint32), "FMovieSceneTrackIdentifier is not convertible directly to/from uint32.");
	return *reinterpret_cast<const TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>*>(&Tracks);
}

TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& FMovieSceneEvaluationTemplate::GetTracks()
{
	// Reinterpret the uint32 as FMovieSceneTrackIdentifier
	static_assert(sizeof(FMovieSceneTrackIdentifier) == sizeof(uint32), "FMovieSceneTrackIdentifier is not convertible directly to/from uint32.");
	return *reinterpret_cast<TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>*>(&Tracks);
}

TArrayView<const FMovieSceneTrackIdentifier> FMovieSceneEvaluationTemplate::FindTracks(const FGuid& InSignature) const
{
	return TemplateLedger.FindTracks(InSignature);
}
