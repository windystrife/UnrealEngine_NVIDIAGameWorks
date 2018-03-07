// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneParticleTemplate.h"
#include "Sections/MovieSceneParticleSection.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystemComponent.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"


DECLARE_CYCLE_STAT(TEXT("Particle Track Token Execute"), MovieSceneEval_ParticleTrack_TokenExecute, STATGROUP_MovieSceneEval);

static UParticleSystemComponent* GetParticleSystemComponentFromObject(UObject* Object)
{
	if (AEmitter* Emitter = Cast<AEmitter>(Object))
	{
		return Emitter->GetParticleSystemComponent();
	}
	else
	{
		return Cast<UParticleSystemComponent>(Object);
	}
}

struct FParticleKeyState : IPersistentEvaluationData
{
	FKeyHandle LastKeyHandle;
	FKeyHandle InvalidKeyHandle;
};

/** A movie scene pre-animated token that stores a pre-animated active state */
struct FActivePreAnimatedToken : IMovieScenePreAnimatedToken
{
	FActivePreAnimatedToken(UObject& InObject)
	{
		bCurrentlyActive = false;

		if (AEmitter* Emitter = Cast<AEmitter>(&InObject))
		{
			bCurrentlyActive = Emitter->bCurrentlyActive;
		}
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		UParticleSystemComponent* ParticleSystemComponent = GetParticleSystemComponentFromObject(&InObject);
		if (ParticleSystemComponent)
		{
			ParticleSystemComponent->SetActive(bCurrentlyActive, true);
		}
	}

private:
	bool bCurrentlyActive;
};

struct FActiveTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	static FMovieSceneAnimTypeID GetAnimTypeID() 
	{
		return TMovieSceneAnimTypeID<FActiveTokenProducer>();
	}

private:
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		return FActivePreAnimatedToken(Object);
	}
};


/** A movie scene execution token that stores a specific transform, and an operand */
struct FParticleTrackExecutionToken
	: IMovieSceneExecutionToken
{
	FParticleTrackExecutionToken(EParticleKey::Type InParticleKey, TOptional<FKeyHandle> InKeyHandle = TOptional<FKeyHandle>())
		: ParticleKey(InParticleKey), KeyHandle(InKeyHandle)
	{
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ParticleTrack_TokenExecute)
		
		if (KeyHandle.IsSet())
		{
			PersistentData.GetOrAddSectionData<FParticleKeyState>().LastKeyHandle = KeyHandle.GetValue();
		}

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			UParticleSystemComponent* ParticleSystemComponent = GetParticleSystemComponentFromObject(ObjectPtr);

			if (ParticleSystemComponent)
			{
				Player.SavePreAnimatedState(*ObjectPtr, FActiveTokenProducer::GetAnimTypeID(), FActiveTokenProducer());

				if ( ParticleKey == EParticleKey::Activate)
				{
					if ( ParticleSystemComponent->IsActive() )
					{
						ParticleSystemComponent->SetActive(false, true);
					}
					ParticleSystemComponent->SetActive(true, true);
				}
				else if( ParticleKey == EParticleKey::Deactivate )
				{
					ParticleSystemComponent->SetActive(false, true);
				}
				else if ( ParticleKey == EParticleKey::Trigger )
				{
					ParticleSystemComponent->ActivateSystem(true);
				}
			}
		}
	}

	EParticleKey::Type ParticleKey;
	TOptional<FKeyHandle> KeyHandle;
};

FMovieSceneParticleSectionTemplate::FMovieSceneParticleSectionTemplate(const UMovieSceneParticleSection& Section)
	: ParticleKeys(Section.GetParticleCurve())
{
}

void FMovieSceneParticleSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const bool bPlaying = Context.GetDirection() == EPlayDirection::Forwards && Context.GetRange().Size<float>() >= 0.f && Context.GetStatus() == EMovieScenePlayerStatus::Playing;

	const FParticleKeyState* SectionData = PersistentData.FindSectionData<FParticleKeyState>();

	if (!bPlaying)
	{
		ExecutionTokens.Add(FParticleTrackExecutionToken(EParticleKey::Deactivate, SectionData ? SectionData->InvalidKeyHandle : TOptional<FKeyHandle>()));
	}
	else
	{
		FKeyHandle PreviousHandle = ParticleKeys.FindKeyBeforeOrAt(Context.GetTime());
		if (ParticleKeys.IsKeyHandleValid(PreviousHandle) && (!SectionData || SectionData->LastKeyHandle != PreviousHandle))
		{
			FParticleTrackExecutionToken NewToken(
				(EParticleKey::Type)ParticleKeys.GetKey(PreviousHandle).Value,
				PreviousHandle);

			ExecutionTokens.Add(MoveTemp(NewToken));
		}
	}
}
