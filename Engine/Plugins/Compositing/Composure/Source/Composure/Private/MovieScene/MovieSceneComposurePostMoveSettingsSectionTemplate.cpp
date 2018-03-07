// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneComposurePostMoveSettingsSectionTemplate.h"
#include "MovieSceneComposurePostMoveSettingsTrack.h"
#include "MovieSceneComposurePostMoveSettingsSection.h"
#include "ComposurePostMoves.h"

template<>
FMovieSceneAnimTypeID GetBlendingDataType<FComposurePostMoveSettings>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

FMovieSceneComposurePostMoveSettingsSectionTemplate::FMovieSceneComposurePostMoveSettingsSectionTemplate(const UMovieSceneComposurePostMoveSettingsSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, BlendType(Section.GetBlendType().Get())
{
	Pivot[0] = Section.GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::X);
	Pivot[1] = Section.GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::Y);

	Translation[0] = Section.GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::X);
	Translation[1] = Section.GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::Y);

	RotationAngle = Section.GetCurve(EComposurePostMoveSettingsChannel::RotationAngle, EComposurePostMoveSettingsAxis::None);

	Scale = Section.GetCurve(EComposurePostMoveSettingsChannel::Scale, EComposurePostMoveSettingsAxis::None);
}

void FMovieSceneComposurePostMoveSettingsSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const float Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 6> AnimatedData;

	// Only activate channels if the curve has data associated with it
	auto EvalChannel = [&AnimatedData, Time](uint8 ChanneIndex, const FRichCurve& Curve)
	{
		if (Curve.HasAnyData())
		{
			AnimatedData.Set(ChanneIndex, Curve.Eval(Time));
		}
	};

	EvalChannel(0, Pivot[0]);
	EvalChannel(1, Pivot[1]);

	EvalChannel(2, Translation[0]);
	EvalChannel(3, Translation[1]);

	EvalChannel(4, RotationAngle);

	EvalChannel(5, Scale);

	if (!AnimatedData.IsEmpty())
	{
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FComposurePostMoveSettings>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FComposurePostMoveSettings>(AnimatedData, BlendType, Weight));
	}
}