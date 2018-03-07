// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScene3DPathTemplate.h"
#include "Evaluation/MovieSceneTemplateCommon.h"
#include "MovieSceneCommonHelpers.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"


DECLARE_CYCLE_STAT(TEXT("Path Track Evaluate"), MovieSceneEval_PathTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Path Track Token Execute"), MovieSceneEval_PathTrack_TokenExecute, STATGROUP_MovieSceneEval);


/** A movie scene execution token that stores a specific spline component, and an operand */
struct F3DPathExecutionToken
	: IMovieSceneExecutionToken
{
	F3DPathExecutionToken(FGuid InPathGuid, float InTiming, MovieScene3DPathSection_Axis InFrontAxisEnum, MovieScene3DPathSection_Axis InUpAxisEnum, bool bInFollow, bool bInReverse, bool bInForceUpright) 
		: PathGuid(InPathGuid)
		, Timing(InTiming)
		, FrontAxisEnum(InFrontAxisEnum)
		, UpAxisEnum(InUpAxisEnum)
		, bFollow(bInFollow)
		, bReverse(bInReverse)
		, bForceUpright(bInForceUpright)
	{}

	void Eval(USceneComponent* SceneComponent, USplineComponent* SplineComponent, FVector& OutTranslation, FRotator& OutRotation)
	{
		if (Timing < 0.f)
		{
			Timing = 0.f;
		}
		else if (Timing > 1.f)
		{
			Timing = 1.f;
		}

		if (bReverse)
		{
			Timing = 1.f - Timing;
		}

		const bool UseConstantVelocity = true;
		OutTranslation = SplineComponent->GetWorldLocationAtTime(Timing, UseConstantVelocity);
		OutRotation = SplineComponent->GetWorldRotationAtTime(Timing, UseConstantVelocity);
	
		FMatrix NewRotationMatrix = FRotationMatrix(OutRotation);

		FVector UpAxis(0, 0, 0);
		if (UpAxisEnum == MovieScene3DPathSection_Axis::X)
		{
			UpAxis = FVector(1, 0, 0);
		}
		else if (UpAxisEnum == MovieScene3DPathSection_Axis::NEG_X)
		{
			UpAxis = FVector(-1, 0, 0);
		}
		else if (UpAxisEnum == MovieScene3DPathSection_Axis::Y)
		{
			UpAxis = FVector(0, 1, 0);
		}
		else if (UpAxisEnum == MovieScene3DPathSection_Axis::NEG_Y)
		{
			UpAxis = FVector(0, -1, 0);
		}
		else if (UpAxisEnum == MovieScene3DPathSection_Axis::Z)
		{
			UpAxis = FVector(0, 0, 1);
		}
		else if (UpAxisEnum == MovieScene3DPathSection_Axis::NEG_Z)
		{
			UpAxis = FVector(0, 0, -1);
		}

		FVector FrontAxis(0, 0, 0);
		if (FrontAxisEnum == MovieScene3DPathSection_Axis::X)
		{
			FrontAxis = FVector(1, 0, 0);
		}
		else if (FrontAxisEnum == MovieScene3DPathSection_Axis::NEG_X)
		{
			FrontAxis = FVector(-1, 0, 0);
		}
		else if (FrontAxisEnum == MovieScene3DPathSection_Axis::Y)
		{
			FrontAxis = FVector(0, 1, 0);
		}
		else if (FrontAxisEnum == MovieScene3DPathSection_Axis::NEG_Y)
		{
			FrontAxis = FVector(0, -1, 0);
		}
		else if (FrontAxisEnum == MovieScene3DPathSection_Axis::Z)
		{
			FrontAxis = FVector(0, 0, 1);
		}
		else if (FrontAxisEnum == MovieScene3DPathSection_Axis::NEG_Z)
		{
			FrontAxis = FVector(0, 0, -1);
		}

		// Negate the front axis because the spline rotation comes in reversed
		FrontAxis *= FVector(-1, -1, -1);

		FMatrix AxisRotator = FRotationMatrix::MakeFromXZ(FrontAxis, UpAxis);
		NewRotationMatrix = AxisRotator * NewRotationMatrix;
		OutRotation = NewRotationMatrix.Rotator();

		if (bForceUpright)
		{
			OutRotation.Pitch = 0.f;
			OutRotation.Roll = 0.f;
		}

		if (!bFollow)
		{
			OutRotation = SceneComponent->GetRelativeTransform().GetRotation().Rotator();
		}
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PathTrack_TokenExecute)

		FMovieSceneEvaluationOperand PathOperand(Operand.SequenceID, PathGuid);
		
		TArrayView<TWeakObjectPtr<>> Objects = Player.FindBoundObjects(PathOperand);
		if (!Objects.Num())
		{
			return;
		}

		// Only ever deal with one parent
		UObject* ParentObject = Objects[0].Get();
		AActor* Actor = Cast<AActor>(ParentObject);

		TArray<USplineComponent*> SplineComponents;
		Actor->GetComponents(SplineComponents);

		if (!SplineComponents.Num())
		{
			return;
		}

		USplineComponent* SplineComponent = SplineComponents[0];

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			USceneComponent* SceneComponent = ObjectPtr ? MovieSceneHelpers::SceneComponentFromRuntimeObject(ObjectPtr) : nullptr;

			if (SceneComponent)
			{
				Player.SavePreAnimatedState(*SceneComponent, FMobilityTokenProducer::GetAnimTypeID(), FMobilityTokenProducer());
				Player.SavePreAnimatedState(*SceneComponent, F3DTransformTokenProducer::GetAnimTypeID(), F3DTransformTokenProducer());

				FVector Location;
				FRotator Rotation;
				Eval(SceneComponent, SplineComponent, Location, Rotation);
				
				SceneComponent->SetMobility(EComponentMobility::Movable);
				SceneComponent->SetRelativeLocationAndRotation(Location, Rotation);
			}
		}
	}
	
	FGuid PathGuid;
	float Timing;
	MovieScene3DPathSection_Axis FrontAxisEnum;
	MovieScene3DPathSection_Axis UpAxisEnum;
	bool bFollow;
	bool bReverse;
	bool bForceUpright;
};

FMovieScene3DPathSectionTemplate::FMovieScene3DPathSectionTemplate(const UMovieScene3DPathSection& Section)
	: PathGuid(Section.GetConstraintId())
	, TimingCurve(Section.GetTimingCurve())
	, FrontAxisEnum(Section.GetFrontAxisEnum())
	, UpAxisEnum(Section.GetUpAxisEnum())
	, bFollow(Section.GetFollow())
	, bReverse(Section.GetReverse())
	, bForceUpright(Section.GetForceUpright())
{
}

void FMovieScene3DPathSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PathTrack_Evaluate)
		
	float Timing = TimingCurve.Eval(Context.GetTime());

	ExecutionTokens.Add(F3DPathExecutionToken(PathGuid, Timing, FrontAxisEnum, UpAxisEnum, bFollow, bReverse, bForceUpright));
}
