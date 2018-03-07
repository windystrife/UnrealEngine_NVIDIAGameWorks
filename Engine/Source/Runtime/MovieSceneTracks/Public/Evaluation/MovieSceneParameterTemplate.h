// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"
#include "MovieSceneExecutionToken.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneParameterSection.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IMovieScenePlayer.h"

#include "MovieSceneParameterTemplate.generated.h"

class UMovieSceneComponentMaterialTrack;

DECLARE_CYCLE_STAT(TEXT("Parameter Track Token Execute"), MovieSceneEval_ParameterTrack_TokenExecute, STATGROUP_MovieSceneEval);

/** Evaluation structure that holds evaluated values */
struct FEvaluatedParameterSectionValues
{
	FEvaluatedParameterSectionValues() = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FEvaluatedParameterSectionValues(FEvaluatedParameterSectionValues&&) = default;
	FEvaluatedParameterSectionValues& operator=(FEvaluatedParameterSectionValues&&) = default;
#else
	FEvaluatedParameterSectionValues(FEvaluatedParameterSectionValues&& RHS)
	{
		*this = MoveTemp(RHS);
	}
	FEvaluatedParameterSectionValues& operator=(FEvaluatedParameterSectionValues&& RHS)
	{
		ScalarValues = MoveTemp(RHS.ScalarValues);
		VectorValues = MoveTemp(RHS.VectorValues);
		ColorValues = MoveTemp(RHS.ColorValues);
		return *this;
	}
#endif

	/** Array of evaluated scalar values */
	TArray<FScalarParameterNameAndValue, TInlineAllocator<2>> ScalarValues;
	/** Array of evaluated vector values */
	TArray<FVectorParameterNameAndValue, TInlineAllocator<2>> VectorValues;
	/** Array of evaluated color values */
	TArray<FColorParameterNameAndValue, TInlineAllocator<2>> ColorValues;
};

/** Template that performs evaluation of parameter sections */
USTRUCT()
struct FMovieSceneParameterSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneParameterSectionTemplate() {}

protected:

	/** Protected constructor to initialize from a parameter section */
	MOVIESCENETRACKS_API FMovieSceneParameterSectionTemplate(const UMovieSceneParameterSection& Section);

	/** Evaluate our curves, outputting evaluated values into the specified container */
	MOVIESCENETRACKS_API void EvaluateCurves(const FMovieSceneContext& Context, FEvaluatedParameterSectionValues& OutValues) const;

private:

	/** The scalar parameter names and their associated curves. */
	UPROPERTY()
	TArray<FScalarParameterNameAndCurve> Scalars;

	/** The vector parameter names and their associated curves. */
	UPROPERTY()
	TArray<FVectorParameterNameAndCurves> Vectors;

	/** The color parameter names and their associated curves. */
	UPROPERTY()
	TArray<FColorParameterNameAndCurves> Colors;
};

/** Default accessor type for use with TMaterialTrackExecutionToken */
struct FDefaultMaterialAccessor
{
	// Implement in derived classes:

	/** Get the anim type ID for the evaluation token */
	// FMovieSceneAnimTypeID 	GetAnimTypeID();

	/** Get the material from the specified object */
	// UMaterialInterface* 		GetMaterialForObject(UObject& Object);

	/** Set the material for the specified object */
	// void 					SetMaterialForObject(UObject& Object, UMaterialInterface& Material);

	/** Apply the specified values onto the specified material */
	MOVIESCENETRACKS_API void Apply(UMaterialInstanceDynamic& Material, const FEvaluatedParameterSectionValues& Values);
};

/**
 * Material track execution token
 * Templated on accessor type to allow for copyable accessors into pre animated state
 */
template<typename AccessorType>
struct TMaterialTrackExecutionToken : IMovieSceneExecutionToken
{
	TMaterialTrackExecutionToken(AccessorType InAccessor)
		: Accessor(MoveTemp(InAccessor))
	{
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	TMaterialTrackExecutionToken(TMaterialTrackExecutionToken&&) = default;
	TMaterialTrackExecutionToken& operator=(TMaterialTrackExecutionToken&&) = default;
#else
	TMaterialTrackExecutionToken(TMaterialTrackExecutionToken&& RHS)
		: Accessor(MoveTemp(RHS.Accessor))
		, Values(MoveTemp(RHS.Values))
	{
	}
	TMaterialTrackExecutionToken& operator=(TMaterialTrackExecutionToken&& RHS)
	{
		Accessor = MoveTemp(RHS.Accessor);
		Values = MoveTemp(RHS.Values);
		return *this;
	}
#endif

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ParameterTrack_TokenExecute)
		
		for (TWeakObjectPtr<>& WeakObject : Player.FindBoundObjects(Operand))
		{
			UObject* Object = WeakObject.Get();
			UMaterialInterface* Material = Object ? Accessor.GetMaterialForObject(*Object) : nullptr;

			if (!Material)
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterialInstance = Cast<UMaterialInstanceDynamic>(Material);
			if (!DynamicMaterialInstance)
			{
				// Save the old instance
				Player.SavePreAnimatedState(*Object, Accessor.GetAnimTypeID(), FPreAnimatedTokenProducer(Accessor));

				FString DynamicName = Material->GetName() + "_Animated";
				FName UniqueDynamicName = MakeUniqueObjectName( Object, UMaterialInstanceDynamic::StaticClass() , *DynamicName );
				DynamicMaterialInstance = UMaterialInstanceDynamic::Create( Material, Object, UniqueDynamicName );

				Accessor.SetMaterialForObject(*Object, *DynamicMaterialInstance);
			}

			Accessor.Apply(*DynamicMaterialInstance, Values);
		}
	}

	AccessorType Accessor;
	FEvaluatedParameterSectionValues Values;

private:

	struct FPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
	{
		struct FPreAnimatedToken : IMovieScenePreAnimatedToken
		{
			FPreAnimatedToken(UObject& Object, const AccessorType& InAccessor)
				: Accessor(InAccessor)
				, Material(Accessor.GetMaterialForObject(Object))
			{}

			virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
			{
				if (UMaterialInterface* PinnedMaterial = Material.Get())
				{
					Accessor.SetMaterialForObject(Object, *PinnedMaterial);
				}
			}

			AccessorType Accessor;
			TWeakObjectPtr<UMaterialInterface> Material;
		};

		FPreAnimatedTokenProducer(const AccessorType& InAccessor) : Accessor(InAccessor) {}

		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FPreAnimatedToken(Object, Accessor);
		}

		const AccessorType& Accessor;
	};
};

/** Evaluation template for primitive component materials */
USTRUCT()
struct FMovieSceneComponentMaterialSectionTemplate : public FMovieSceneParameterSectionTemplate
{
	GENERATED_BODY()

	FMovieSceneComponentMaterialSectionTemplate() {}
	FMovieSceneComponentMaterialSectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneComponentMaterialTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	int32 MaterialIndex;
};
