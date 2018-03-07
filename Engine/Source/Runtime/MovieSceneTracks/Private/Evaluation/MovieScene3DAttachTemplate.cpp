// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScene3DAttachTemplate.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Evaluation/MovieSceneTemplateCommon.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"


DECLARE_CYCLE_STAT(TEXT("Attach Track Evaluate"), MovieSceneEval_AttachTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Attach Track Token Execute"), MovieSceneEval_AttachTrack_TokenExecute, STATGROUP_MovieSceneEval);


struct F3DAttachRuleState
{
	F3DAttachRuleState(EAttachmentRule InAttachmentLocationRule, EAttachmentRule InAttachmentRotationRule, EAttachmentRule InAttachmentScaleRule, EDetachmentRule InDetachmentLocationRule, EDetachmentRule InDetachmentRotationRule, EDetachmentRule InDetachmentScaleRule)
		: AttachmentLocationRule(InAttachmentLocationRule)
		, AttachmentRotationRule(InAttachmentRotationRule)
		, AttachmentScaleRule(InAttachmentScaleRule)
		, DetachmentLocationRule(InDetachmentLocationRule)
		, DetachmentRotationRule(InDetachmentRotationRule)
		, DetachmentScaleRule(InDetachmentScaleRule)
	{}

	EAttachmentRule AttachmentLocationRule;
	EAttachmentRule AttachmentRotationRule;
	EAttachmentRule AttachmentScaleRule;
	EDetachmentRule DetachmentLocationRule;
	EDetachmentRule DetachmentRotationRule;
	EDetachmentRule DetachmentScaleRule;
};


/** A token that sets a component's attach */
struct F3DAttachTrackToken : public F3DAttachRuleState
{
	F3DAttachTrackToken(USceneComponent* InAttachParent, FName InAttachSocketName, bool bInShouldbeAttached, EAttachmentRule InAttachmentLocationRule, EAttachmentRule InAttachmentRotationRule, EAttachmentRule InAttachmentScaleRule, EDetachmentRule InDetachmentLocationRule, EDetachmentRule InDetachmentRotationRule, EDetachmentRule InDetachmentScaleRule)
		: F3DAttachRuleState(InAttachmentLocationRule, InAttachmentRotationRule, InAttachmentScaleRule, InDetachmentLocationRule, InDetachmentRotationRule, InDetachmentScaleRule)
		, AttachSocketName(InAttachSocketName)
		, bShouldBeAttached(bInShouldbeAttached)
	{}

	void Apply(USceneComponent& SceneComponent) const
	{
		if (bShouldBeAttached)
		{
			if (SceneComponent.GetAttachParent() != AttachParent.Get() || SceneComponent.GetAttachSocketName() != AttachSocketName)
			{
				FAttachmentTransformRules AttachmentRules(AttachmentLocationRule, AttachmentRotationRule, AttachmentScaleRule, false);

				SceneComponent.AttachToComponent(AttachParent.Get(), AttachmentRules, AttachSocketName);	
			}

			// Match the component velocity of the parent. If the attached child has any transformation, the velocity will be 
			// computed by the 3d transform template.
			if (SceneComponent.GetAttachParent())
			{
				SceneComponent.ComponentVelocity = SceneComponent.GetAttachParent()->GetComponentVelocity();
			}
		}
		else
		{
			FDetachmentTransformRules DetachmentRules(DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule, false);

			SceneComponent.DetachFromComponent(DetachmentRules);
		}
	}

	TWeakObjectPtr<USceneComponent> AttachParent;
	FName AttachSocketName;
	bool bShouldBeAttached;
};

/** A movie scene pre-animated token that stores a pre-animated component attachment */
struct F3DAttachTrackPreAnimatedToken : F3DAttachTrackToken, IMovieScenePreAnimatedToken
{
	F3DAttachTrackPreAnimatedToken(USceneComponent& SceneComponent, EAttachmentRule InAttachmentLocationRule, EAttachmentRule InAttachmentRotationRule, EAttachmentRule InAttachmentScaleRule, EDetachmentRule InDetachmentLocationRule, EDetachmentRule InDetachmentRotationRule, EDetachmentRule InDetachmentScaleRule)
		: F3DAttachTrackToken(SceneComponent.GetAttachParent(), SceneComponent.GetAttachSocketName(), true, InAttachmentLocationRule, InAttachmentRotationRule, InAttachmentScaleRule, InDetachmentLocationRule, InDetachmentRotationRule, InDetachmentScaleRule)
	{}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(&InObject);

		Apply(*SceneComponent);

		// Detach if there was no pre-existing parent
		if (!AttachParent.IsValid())
		{
			FDetachmentTransformRules DetachmentRules(DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule, false);

			SceneComponent->DetachFromComponent(DetachmentRules);
		}
	}
};

struct F3DAttachTokenProducer : F3DAttachRuleState, IMovieScenePreAnimatedTokenProducer
{
	F3DAttachTokenProducer(EAttachmentRule InAttachmentLocationRule, EAttachmentRule InAttachmentRotationRule, EAttachmentRule InAttachmentScaleRule, EDetachmentRule InDetachmentLocationRule, EDetachmentRule InDetachmentRotationRule, EDetachmentRule InDetachmentScaleRule) 
		: F3DAttachRuleState(InAttachmentLocationRule, InAttachmentRotationRule, InAttachmentScaleRule, InDetachmentLocationRule, InDetachmentRotationRule, InDetachmentScaleRule)
	{}

	/** Cache the existing state of an object before moving it */
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(&Object);
		return F3DAttachTrackPreAnimatedToken(*SceneComponent, AttachmentLocationRule, AttachmentRotationRule, AttachmentScaleRule, DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule);
	}
};

/** A movie scene execution token that stores a specific attach, and an operand */
struct F3DAttachExecutionToken
	: IMovieSceneExecutionToken
	, F3DAttachTrackToken
{
	F3DAttachExecutionToken(FGuid InAttachGuid, FName InAttachSocketName, FName InAttachComponentName, bool bInShouldBeAttached, EAttachmentRule InAttachmentLocationRule, EAttachmentRule InAttachmentRotationRule, EAttachmentRule InAttachmentScaleRule, EDetachmentRule InDetachmentLocationRule, EDetachmentRule InDetachmentRotationRule, EDetachmentRule InDetachmentScaleRule) 
		: F3DAttachTrackToken(nullptr, InAttachSocketName, bInShouldBeAttached, InAttachmentLocationRule, InAttachmentRotationRule, InAttachmentScaleRule, InDetachmentLocationRule, InDetachmentRotationRule, InDetachmentScaleRule)
		, AttachGuid(InAttachGuid)
		, AttachComponentName(InAttachComponentName)
	{}

	USceneComponent* GetAttachComponent(AActor* InParentActor)
	{
		if (AttachSocketName != NAME_None)
		{
			if (AttachComponentName != NAME_None )
			{
				TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
				for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
				{
					if (PotentialAttachComponent->GetFName() == AttachComponentName && PotentialAttachComponent->DoesSocketExist(AttachSocketName))
					{
						return PotentialAttachComponent;
					}
				}
			}
			else if (InParentActor->GetRootComponent()->DoesSocketExist(AttachSocketName))
			{
				return InParentActor->GetRootComponent();
			}
		}
		else if (AttachComponentName != NAME_None )
		{
			TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
			for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
			{
				if (PotentialAttachComponent->GetFName() == AttachComponentName)
				{
					return PotentialAttachComponent;
				}
			}
		}

		if (InParentActor->GetDefaultAttachComponent())
		{
			return InParentActor->GetDefaultAttachComponent();
		}
		else
		{
			return InParentActor->GetRootComponent();
		}
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_AttachTrack_TokenExecute)

		FMovieSceneEvaluationOperand AttachOperand(Operand.SequenceID, AttachGuid);
		
		TArrayView<TWeakObjectPtr<>> Objects = Player.FindBoundObjects(AttachOperand);
		if (!Objects.Num())
		{
			return;
		}

		// Only ever deal with one parent
		UObject* ParentObject = Objects[0].Get();
		AActor* ParentActor = Cast<AActor>(ParentObject);

		if (!ParentActor)
		{
			return;
		}

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			USceneComponent* SceneComponent = ObjectPtr ? MovieSceneHelpers::SceneComponentFromRuntimeObject(ObjectPtr) : nullptr;

			if (SceneComponent)
			{
				Player.SavePreAnimatedState(*SceneComponent, FMobilityTokenProducer::GetAnimTypeID(), FMobilityTokenProducer());

				// Attachment can affect transform as well, so save transform state.
				Player.SavePreAnimatedState(*SceneComponent, F3DTransformTokenProducer::GetAnimTypeID(), F3DTransformTokenProducer());

				Player.SavePreAnimatedState(*SceneComponent, TMovieSceneAnimTypeID<F3DAttachExecutionToken>(), F3DAttachTokenProducer(AttachmentLocationRule, AttachmentRotationRule, AttachmentScaleRule, DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule));

				AttachParent = GetAttachComponent(ParentActor);

				SceneComponent->SetMobility(EComponentMobility::Movable);

				Apply(*SceneComponent);
			}
		}
	}
	
	FGuid AttachGuid;
	FName AttachComponentName;
};

FMovieScene3DAttachSectionTemplate::FMovieScene3DAttachSectionTemplate(const UMovieScene3DAttachSection& Section)
	: AttachGuid(Section.GetConstraintId())
	, AttachSocketName(Section.AttachSocketName)
	, AttachComponentName(Section.AttachComponentName)
	, AttachmentLocationRule(Section.AttachmentLocationRule)
	, AttachmentRotationRule(Section.AttachmentRotationRule)
	, AttachmentScaleRule(Section.AttachmentScaleRule)
	, DetachmentLocationRule(Section.DetachmentLocationRule)
	, DetachmentRotationRule(Section.DetachmentRotationRule)
	, DetachmentScaleRule(Section.DetachmentScaleRule)
{
}

void FMovieScene3DAttachSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_AttachTrack_Evaluate)

	const bool bShouldBeAttached = true;

	ExecutionTokens.Add(F3DAttachExecutionToken(AttachGuid, AttachSocketName, AttachComponentName, bShouldBeAttached, AttachmentLocationRule, AttachmentRotationRule, AttachmentScaleRule, DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule));
}
