// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneParameterTemplate.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Components/PrimitiveComponent.h"
#include "MovieSceneEvaluation.h"


FMovieSceneParameterSectionTemplate::FMovieSceneParameterSectionTemplate(const UMovieSceneParameterSection& Section)
	: Scalars(Section.GetScalarParameterNamesAndCurves())
	, Vectors(Section.GetVectorParameterNamesAndCurves())
	, Colors(Section.GetColorParameterNamesAndCurves())
{
}

void FMovieSceneParameterSectionTemplate::EvaluateCurves(const FMovieSceneContext& Context, FEvaluatedParameterSectionValues& Values) const
{
	float Time = Context.GetTime();

	for ( const FScalarParameterNameAndCurve& Scalar : Scalars )
	{
		Values.ScalarValues.Add(
			FScalarParameterNameAndValue(Scalar.ParameterName, Scalar.ParameterCurve.Eval(Time))
			);
	}

	for ( const FVectorParameterNameAndCurves& Vector : Vectors )
	{
		Values.VectorValues.Add(
			FVectorParameterNameAndValue(
				Vector.ParameterName,
				FVector(
					Vector.XCurve.Eval(Time),
					Vector.YCurve.Eval(Time),
					Vector.ZCurve.Eval(Time)
				)
			)
		);
	}

	for ( const FColorParameterNameAndCurves& Color : Colors )
	{
		Values.ColorValues.Add(
			FColorParameterNameAndValue(
				Color.ParameterName,
				FLinearColor(
					Color.RedCurve.Eval(Time ),
					Color.GreenCurve.Eval(Time),
					Color.BlueCurve.Eval(Time),
					Color.AlphaCurve.Eval(Time)
				)
			)
		);
	}
}

void FDefaultMaterialAccessor::Apply(UMaterialInstanceDynamic& Material, const FEvaluatedParameterSectionValues& Values)
{
	for (const FScalarParameterNameAndValue& ScalarValue : Values.ScalarValues)
	{
		Material.SetScalarParameterValue(ScalarValue.ParameterName, ScalarValue.Value);
	}
	for (const FVectorParameterNameAndValue& VectorValue : Values.VectorValues)
	{
		Material.SetVectorParameterValue(VectorValue.ParameterName, VectorValue.Value);
	}
	for (const FColorParameterNameAndValue& ColorValue : Values.ColorValues)
	{
		Material.SetVectorParameterValue(ColorValue.ParameterName, ColorValue.Value);
	}
}


TMovieSceneAnimTypeIDContainer<int32> MaterialIndexAnimTypeIDs;

struct FComponentMaterialAccessor : FDefaultMaterialAccessor
{
	FComponentMaterialAccessor(int32 InMaterialIndex)
		: MaterialIndex(InMaterialIndex)
	{}

	FComponentMaterialAccessor(const FComponentMaterialAccessor&) = default;
	FComponentMaterialAccessor& operator=(const FComponentMaterialAccessor&) = default;
	
	FMovieSceneAnimTypeID GetAnimTypeID() const
	{
		return MaterialIndexAnimTypeIDs.GetAnimTypeID(MaterialIndex);
	}

	UMaterialInterface* GetMaterialForObject(UObject& Object) const
	{
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(&Object);
		return Component ? Component->GetMaterial(MaterialIndex) : nullptr;
	}

	void SetMaterialForObject(UObject& Object, UMaterialInterface& Material) const
	{
		UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(&Object);
		Component->SetMaterial(MaterialIndex, &Material);
	}

	int32 MaterialIndex;
};

FMovieSceneComponentMaterialSectionTemplate::FMovieSceneComponentMaterialSectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneComponentMaterialTrack& Track)
	: FMovieSceneParameterSectionTemplate(Section)
	, MaterialIndex(Track.GetMaterialIndex())
{
}

void FMovieSceneComponentMaterialSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	TMaterialTrackExecutionToken<FComponentMaterialAccessor> ExecutionToken(MaterialIndex);

	EvaluateCurves(Context, ExecutionToken.Values);

	ExecutionTokens.Add(MoveTemp(ExecutionToken));
}
