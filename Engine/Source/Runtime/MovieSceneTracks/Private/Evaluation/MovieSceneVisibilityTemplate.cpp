// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneVisibilityTemplate.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "MovieSceneEvaluation.h"
#include "Components/SceneComponent.h"


DECLARE_CYCLE_STAT(TEXT("Visibility Track Evaluate"), MovieSceneEval_VisibilityTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Visibility Track Token Execute"), MovieSceneEval_VisibilityTrack_TokenExecute, STATGROUP_MovieSceneEval);


/** A movie scene pre-animated token that stores a pre-animated actor's temporarily hidden in game */
struct FTemporarilyHiddenInGamePreAnimatedToken : IMovieScenePreAnimatedToken
{
	FTemporarilyHiddenInGamePreAnimatedToken(bool bInHidden, bool bInTemporarilyHiddenInGame)
		: bHidden(bInHidden)
		, bTemporarilyHiddenInGame(bInTemporarilyHiddenInGame)
	{}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		if (InObject.IsA(AActor::StaticClass()))
		{
			AActor* Actor = CastChecked<AActor>(&InObject);
			
			Actor->SetActorHiddenInGame(bHidden);

#if WITH_EDITOR
			Actor->SetIsTemporarilyHiddenInEditor(bTemporarilyHiddenInGame);
#endif // WITH_EDITOR
		}
		else if (InObject.IsA(USceneComponent::StaticClass()))
		{
			USceneComponent* SceneComponent = CastChecked<USceneComponent>(&InObject);
			
			SceneComponent->SetHiddenInGame(bHidden);
		}
	}

	bool bHidden;
	bool bTemporarilyHiddenInGame;
};

struct FTemporarilyHiddenInGameTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	/** Cache the existing state of an object before moving it */
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& InObject) const override
	{
		if (InObject.IsA(AActor::StaticClass()))
		{
			AActor* Actor = CastChecked<AActor>(&InObject);
	
			bool bInTemporarilyHiddenInGame = 
#if WITH_EDITOR
				Actor->IsTemporarilyHiddenInEditor();
#else
				false;
#endif
			return FTemporarilyHiddenInGamePreAnimatedToken(Actor->bHidden, bInTemporarilyHiddenInGame);
		}
		else if (InObject.IsA(USceneComponent::StaticClass()))
		{
			USceneComponent* SceneComponent = CastChecked<USceneComponent>(&InObject);

			const bool bUnused = false;
			return FTemporarilyHiddenInGamePreAnimatedToken(SceneComponent->bHiddenInGame, bUnused);
		}

		return FTemporarilyHiddenInGamePreAnimatedToken(false, false);
	}
};

/** A movie scene execution token that stores temporarily hidden in game */
struct FTemporarilyHiddenInGameExecutionToken
	: IMovieSceneExecutionToken
{
	bool bIsHidden;
	FTemporarilyHiddenInGameExecutionToken(bool bInIsHidden) : bIsHidden(bInIsHidden) {}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FTemporarilyHiddenInGameExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_VisibilityTrack_TokenExecute);

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			if (UObject* ObjectPtr = WeakObject.Get())
			{
				if (ObjectPtr->IsA(AActor::StaticClass()))
				{
					AActor* Actor = Cast<AActor>(ObjectPtr);

					Player.SavePreAnimatedState(*Actor, GetAnimTypeID(), FTemporarilyHiddenInGameTokenProducer());

					Actor->SetActorHiddenInGame(bIsHidden);

#if WITH_EDITOR
					if (GIsEditor && Actor->GetWorld() != nullptr && !Actor->GetWorld()->IsPlayInEditor())
					{
						Actor->SetIsTemporarilyHiddenInEditor(bIsHidden);
					}
#endif // WITH_EDITOR
				}
				else if (ObjectPtr->IsA(USceneComponent::StaticClass()))
				{
					USceneComponent* SceneComponent = Cast<USceneComponent>(ObjectPtr);
					
					Player.SavePreAnimatedState(*SceneComponent, GetAnimTypeID(), FTemporarilyHiddenInGameTokenProducer());
					SceneComponent->SetHiddenInGame(bIsHidden);
				}
			}
		}
	}
};

FMovieSceneVisibilitySectionTemplate::FMovieSceneVisibilitySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieSceneBoolPropertySectionTemplate(Section, Track)
{}

void FMovieSceneVisibilitySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_VisibilityTrack_Evaluate);

	if (BoolCurve.HasAnyData())
	{
		// Invert this evaluation since the property is "bHiddenInGame" and we want the visualization to be the inverse of that. Green means visible.
		bool bIsHidden = !BoolCurve.Evaluate(Context.GetTime());
		ExecutionTokens.Add(FTemporarilyHiddenInGameExecutionToken(bIsHidden));
	}
}

