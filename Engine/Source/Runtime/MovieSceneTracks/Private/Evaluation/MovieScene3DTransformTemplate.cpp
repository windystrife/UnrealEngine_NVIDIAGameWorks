// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScene3DTransformTemplate.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Evaluation/MovieSceneTemplateCommon.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/Blending/BlendableTokenStack.h"
#include "Evaluation/Blending/MovieSceneBlendingActuatorID.h"

DECLARE_CYCLE_STAT(TEXT("Transform Track Evaluate"), MovieSceneEval_TransformTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Transform Track Token Execute"), MovieSceneEval_TransformTrack_TokenExecute, STATGROUP_MovieSceneEval);

namespace MovieScene
{
	/** Convert a transform track token to a 9 channel float */
	void MultiChannelFromData(const F3DTransformTrackToken& In, TMultiChannelValue<float, 9>& Out)
	{
		FVector Rotation = In.Rotation.Euler();
		Out = { In.Translation.X, In.Translation.Y, In.Translation.Z, Rotation.X, Rotation.Y, Rotation.Z, In.Scale.X, In.Scale.Y, In.Scale.Z };
	}

	/** Convert a 9 channel float to a transform track token */
	void ResolveChannelsToData(const TMultiChannelValue<float, 9>& In, F3DTransformTrackToken& Out)
	{
		Out.Translation = FVector(In[0], In[1], In[2]);
		Out.Rotation = FRotator::MakeFromEuler(FVector(In[3], In[4], In[5]));
		Out.Scale = FVector(In[6], In[7], In[8]);
	}
}

// Specify a unique runtime type identifier for 3d transform track tokens
template<> FMovieSceneAnimTypeID GetBlendingDataType<F3DTransformTrackToken>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

/** Define working data types for blending calculations - we use a 9 channel masked blendable float */
template<> struct TBlendableTokenTraits<F3DTransformTrackToken> 
{
	typedef MovieScene::TMaskedBlendable<float, 9> WorkingDataType;
};

/** Actuator that knows how to apply transform track tokens to an object */
struct FComponentTransformActuator : TMovieSceneBlendingActuator<F3DTransformTrackToken>
{
	FComponentTransformActuator()
		: TMovieSceneBlendingActuator<F3DTransformTrackToken>(GetActuatorTypeID())
	{}

	/** Access a unique identifier for this actuator type */
	static FMovieSceneBlendingActuatorID GetActuatorTypeID()
	{
		static FMovieSceneAnimTypeID TypeID = TMovieSceneAnimTypeID<FComponentTransformActuator>();
		return FMovieSceneBlendingActuatorID(TypeID);
	}

	/** Get an object's current transform */
	virtual F3DTransformTrackToken RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		if (USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(InObject))
		{
			return F3DTransformTrackToken(SceneComponent->RelativeLocation, SceneComponent->RelativeRotation, SceneComponent->RelativeScale3D);
		}

		return F3DTransformTrackToken();
	}

	/** Set an object's transform */
	virtual void Actuate(UObject* InObject, const F3DTransformTrackToken& InFinalValue, const TBlendableTokenStack<F3DTransformTrackToken>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		ensureMsgf(InObject, TEXT("Attempting to evaluate a Transform track with a null object."));

		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(InObject);
		if (SceneComponent)
		{
			// Save preanimated state for all currently animating entities
			OriginalStack.SavePreAnimatedState(Player, *SceneComponent, FMobilityTokenProducer::GetAnimTypeID(), FMobilityTokenProducer());
			OriginalStack.SavePreAnimatedState(Player, *SceneComponent, F3DTransformTokenProducer::GetAnimTypeID(), F3DTransformTokenProducer());

			SceneComponent->SetMobility(EComponentMobility::Movable);

			InFinalValue.Apply(*SceneComponent, Context.GetDelta());
		}
	}

	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const F3DTransformTrackToken& InValue, const TBlendableTokenStack<F3DTransformTrackToken>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		InterrogationData.Add(FTransform(InValue.Rotation.Quaternion(), InValue.Translation, InValue.Scale), UMovieScene3DTransformTrack::GetInterrogationKey());
	}
};

FMovieSceneComponentTransformSectionTemplate::FMovieSceneComponentTransformSectionTemplate(const UMovieScene3DTransformSection& Section)
	: TemplateData(Section)
{
}

void FMovieSceneComponentTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace MovieScene;

	TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());
	if (TransformValue.IsEmpty())
	{
		return;
	}

	// Ensure the accumulator knows how to actually apply component transforms
	FMovieSceneBlendingActuatorID ActuatorTypeID = FComponentTransformActuator::GetActuatorTypeID();
	if (!ExecutionTokens.GetBlendingAccumulator().FindActuator<F3DTransformTrackToken>(ActuatorTypeID))
	{
		ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared<FComponentTransformActuator>());
	}

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight) && TemplateData.ManualWeight.HasAnyData())
	{
		Weight *= TemplateData.ManualWeight.Eval(Context.GetTime());
	}

	ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<F3DTransformTrackToken>(TransformValue, TemplateData.BlendType, Weight));
}

void FMovieSceneComponentTransformSectionTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	using namespace MovieScene;

	TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());
	if (TransformValue.IsEmpty())
	{
		return;
	}

	// Ensure the accumulator knows how to actually apply component transforms
	FMovieSceneBlendingActuatorID ActuatorTypeID = FComponentTransformActuator::GetActuatorTypeID();
	if (!Container.GetAccumulator().FindActuator<F3DTransformTrackToken>(ActuatorTypeID))
	{
		Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared<FComponentTransformActuator>());
	}

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight) && TemplateData.ManualWeight.HasAnyData())
	{
		Weight *= TemplateData.ManualWeight.Eval(Context.GetTime());
	}

	Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<F3DTransformTrackToken>(TransformValue, TemplateData.BlendType, Weight));
}

FMovieScene3DTransformTemplateData::FMovieScene3DTransformTemplateData(const UMovieScene3DTransformSection& Section)
	: BlendType(Section.GetBlendType().Get())
	, Mask(Section.GetMask())
{
	EMovieSceneTransformChannel MaskChannels = Mask.GetChannels();

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationX))	TranslationCurve[0]	= Section.GetTranslationCurve(EAxis::X);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationY))	TranslationCurve[1]	= Section.GetTranslationCurve(EAxis::Y);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationZ))	TranslationCurve[2]	= Section.GetTranslationCurve(EAxis::Z);

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationX))		RotationCurve[0]	= Section.GetRotationCurve(EAxis::X);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationY))		RotationCurve[1]	= Section.GetRotationCurve(EAxis::Y);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationZ))		RotationCurve[2]	= Section.GetRotationCurve(EAxis::Z);

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleX))			ScaleCurve[0]		= Section.GetScaleCurve(EAxis::X);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleY))			ScaleCurve[1]		= Section.GetScaleCurve(EAxis::Y);
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleZ))			ScaleCurve[2]		= Section.GetScaleCurve(EAxis::Z);

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::Weight))			ManualWeight		= Section.GetManualWeightCurve();
}

MovieScene::TMultiChannelValue<float, 9> FMovieScene3DTransformTemplateData::Evaluate(float Time) const
{
	MovieScene::TMultiChannelValue<float, 9> AnimatedData;

	EMovieSceneTransformChannel ChannelMask = Mask.GetChannels();

	auto EvalChannel = [&AnimatedData, Time, ChannelMask](uint8 ChanneIndex, EMovieSceneTransformChannel Channel, const FRichCurve& Curve)
	{
		if (EnumHasAllFlags(ChannelMask, Channel) && Curve.HasAnyData())
		{
			AnimatedData.Set(ChanneIndex, Curve.Eval(Time));
		}
	};

	EvalChannel(0, EMovieSceneTransformChannel::TranslationX, TranslationCurve[0]);
	EvalChannel(1, EMovieSceneTransformChannel::TranslationY, TranslationCurve[1]);
	EvalChannel(2, EMovieSceneTransformChannel::TranslationZ, TranslationCurve[2]);

	EvalChannel(3, EMovieSceneTransformChannel::RotationX, RotationCurve[0]);
	EvalChannel(4, EMovieSceneTransformChannel::RotationY, RotationCurve[1]);
	EvalChannel(5, EMovieSceneTransformChannel::RotationZ, RotationCurve[2]);

	EvalChannel(6, EMovieSceneTransformChannel::ScaleX, ScaleCurve[0]);
	EvalChannel(7, EMovieSceneTransformChannel::ScaleY, ScaleCurve[1]);
	EvalChannel(8, EMovieSceneTransformChannel::ScaleZ, ScaleCurve[2]);

	return AnimatedData;
}