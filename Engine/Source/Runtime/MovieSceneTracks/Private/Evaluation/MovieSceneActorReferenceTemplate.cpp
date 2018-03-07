// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneActorReferenceTemplate.h"

#include "Sections/MovieSceneActorReferenceSection.h"
#include "Tracks/MovieSceneActorReferenceTrack.h"
#include "GameFramework/Actor.h"
#include "MovieSceneEvaluation.h"


namespace PropertyTemplate
{
	template<>
	AActor* ConvertFromIntermediateType<AActor*, FGuid>(const FGuid& InObjectGuid, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		FMovieSceneEvaluationOperand NewOperand = Operand;
		NewOperand.ObjectBindingID = InObjectGuid;

		for (TWeakObjectPtr<>& WeakObject : Player.FindBoundObjects(NewOperand))
		{
			if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	template<>
	AActor* ConvertFromIntermediateType<AActor*, TWeakObjectPtr<AActor>>(const TWeakObjectPtr<AActor>& InWeakPtr, IMovieScenePlayer& Player)
	{
		return InWeakPtr.Get();
	}

	template<>
	AActor* ConvertFromIntermediateType<AActor*, TWeakObjectPtr<AActor>>(const TWeakObjectPtr<AActor>& InWeakPtr, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		return InWeakPtr.Get();
	}

	static bool IsValueValid(AActor* InValue)
	{
		return InValue != nullptr;
	}

	template<> IMovieScenePreAnimatedTokenPtr CacheExistingState<AActor*, FGuid>(UObject& Object, FTrackInstancePropertyBindings& PropertyBindings)
	{
		return TCachedState<AActor*, TWeakObjectPtr<AActor>>(PropertyBindings.GetCurrentValue<AActor*>(Object), PropertyBindings);
	}
}

FMovieSceneActorReferenceSectionTemplate::FMovieSceneActorReferenceSectionTemplate(const UMovieSceneActorReferenceSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, ActorGuidIndexCurve(Section.GetActorReferenceCurve())
	, ActorGuids(Section.GetActorGuids())
{
}

void FMovieSceneActorReferenceSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;
	FSectionData& TrackData = PersistentData.GetSectionData<FSectionData>();

	const int32 ActorGuidIndex = ActorGuidIndexCurve.Evaluate(Context.GetTime());
	FGuid ObjectBindingID = ActorGuids.IsValidIndex(ActorGuidIndex) ? ActorGuids[ActorGuidIndex] : FGuid();

	ExecutionTokens.Add(TPropertyTrackExecutionToken<AActor*, FGuid>(ObjectBindingID));
}
