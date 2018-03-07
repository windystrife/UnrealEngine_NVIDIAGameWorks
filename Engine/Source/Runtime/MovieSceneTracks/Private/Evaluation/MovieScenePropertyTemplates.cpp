// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePropertyTemplates.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneByteSection.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneStringSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieScene.h"
#include "MovieSceneEvaluation.h"
#include "MovieSceneTemplateCommon.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"

namespace
{
	FName SanitizeBoolPropertyName(FName InPropertyName)
	{
		FString PropertyVarName = InPropertyName.ToString();
		PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
		return FName(*PropertyVarName);
	}
}

//	----------------------------------------------------------------------------
//	Boolean Property Template
FMovieSceneBoolPropertySectionTemplate::FMovieSceneBoolPropertySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, BoolCurve(Section.GetCurve())
{
	PropertyData.PropertyName = SanitizeBoolPropertyName(PropertyData.PropertyName);
}

void FMovieSceneBoolPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Only evaluate if the curve has any data
	if (BoolCurve.HasAnyData())
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<bool>(!!BoolCurve.Evaluate(Context.GetTime())));
	}
}


//	----------------------------------------------------------------------------
//	Float Property Template
FMovieSceneFloatPropertySectionTemplate::FMovieSceneFloatPropertySectionTemplate(const UMovieSceneFloatSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, FloatCurve(Section.GetFloatCurve())
	, BlendType(Section.GetBlendType().Get())
{}

void FMovieSceneFloatPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Only evaluate if the curve has any data
	if (FloatCurve.HasAnyData())
	{
		// Actuator type ID for this property
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<float>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		const float Value = FloatCurve.Eval(Context.GetTime());
		const float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<float>(Value, BlendType, Weight));
	}
}


//	----------------------------------------------------------------------------
//	Byte Property Template
FMovieSceneBytePropertySectionTemplate::FMovieSceneBytePropertySectionTemplate(const UMovieSceneByteSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, ByteCurve(Section.GetCurve())
{}

void FMovieSceneBytePropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Only evaluate if the curve has any data
	if (ByteCurve.HasAnyData())
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<uint8>(ByteCurve.Evaluate(Context.GetTime())));
	}
}


//	----------------------------------------------------------------------------
//	Enum Property Template
FMovieSceneEnumPropertySectionTemplate::FMovieSceneEnumPropertySectionTemplate(const UMovieSceneEnumSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, EnumCurve(Section.GetCurve())
{}

void FMovieSceneEnumPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Only evaluate if the curve has any data
	if (EnumCurve.HasAnyData())
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<int64>(EnumCurve.Evaluate(Context.GetTime())));
	}
}


//	----------------------------------------------------------------------------
//	Integer Property Template
FMovieSceneIntegerPropertySectionTemplate::FMovieSceneIntegerPropertySectionTemplate(const UMovieSceneIntegerSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, IntegerCurve(Section.GetCurve())
	, BlendType(Section.GetBlendType().Get())
{}

void FMovieSceneIntegerPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (IntegerCurve.HasAnyData())
	{
		// Actuator type ID for this property
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<int32>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		const int32 Value = IntegerCurve.Evaluate(Context.GetTime());
		const float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<int32>(Value, BlendType, Weight));
	}
}


//	----------------------------------------------------------------------------
//	String Property Template
FMovieSceneStringPropertySectionTemplate::FMovieSceneStringPropertySectionTemplate(const UMovieSceneStringSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, StringCurve(Section.GetStringCurve())
{}

void FMovieSceneStringPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (StringCurve.HasAnyData())
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<FString>(StringCurve.Eval(Context.GetTime(), FString())));
	}
}


//	----------------------------------------------------------------------------
//	Vector Property Template
FMovieSceneVectorPropertySectionTemplate::FMovieSceneVectorPropertySectionTemplate(const UMovieSceneVectorSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, NumChannelsUsed(Section.GetChannelsUsed())
	, BlendType(Section.GetBlendType().Get())
{
	for (int32 Index = 0; Index < NumChannelsUsed; ++Index)
	{
		ComponentCurves[Index] = Section.GetCurve(Index);
	}
}

/** Helper function fo evaluating a number of curves for a specific vector type */
template<typename VectorType, uint8 N>
void EvaluateVectorCurve(EMovieSceneBlendType BlendType, float Weight, float Time, const FRichCurve* Curves, FMovieSceneBlendingActuatorID ActuatorTypeID, FMovieSceneExecutionTokens& ExecutionTokens)
{
	MovieScene::TMultiChannelValue<float, N> AnimatedChannels;

	for (uint8 Index = 0; Index < N; ++Index)
	{
		const FRichCurve& Curve = Curves[Index];
		if (Curve.HasAnyData())
		{
			AnimatedChannels.Set(Index, Curve.Eval(Time));
		}
	}

	// Only blend the token if at least one of the channels was animated
	if (!AnimatedChannels.IsEmpty())
	{
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<VectorType>(AnimatedChannels, BlendType, Weight));
	}
}

void FMovieSceneVectorPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FMovieSceneBlendingAccumulator& Accumulator = ExecutionTokens.GetBlendingAccumulator();

	const float Time = Context.GetTime();
	const float Weight = EvaluateEasing(Context.GetTime());

	switch (NumChannelsUsed)
	{
	case 2:
		EvaluateVectorCurve<FVector2D, 2>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector2D>(Accumulator), ExecutionTokens);
		break;

	case 3:
		EvaluateVectorCurve<FVector, 3>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector>(Accumulator), ExecutionTokens);
		break;

	case 4:
		EvaluateVectorCurve<FVector4, 4>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector4>(Accumulator), ExecutionTokens);
		break;

	default:
		UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
		break;
	}
}


//	----------------------------------------------------------------------------
//	Transform Property Template
FMovieSceneTransformPropertySectionTemplate::FMovieSceneTransformPropertySectionTemplate(const UMovieScene3DTransformSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, TemplateData(Section)
{}

void FMovieSceneTransformPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MovieScene::TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());

	// Actuator type ID for this property
	FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FTransform>(ExecutionTokens.GetBlendingAccumulator());

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	{
		Weight *= TemplateData.ManualWeight.Eval(Context.GetTime());
	}

	// Add the blendable to the accumulator
	ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FTransform>(TransformValue, TemplateData.BlendType, Weight));
}