// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "ComposurePostMoves.h"
#include "MovieSceneComposurePostMoveSettingsSectionTemplate.generated.h"

class UMovieSceneComposurePostMoveSettingsSection;
class UMovieScenePropertyTrack;

/** A movie scene evaluation template for post move settings sections. */
USTRUCT()
struct FMovieSceneComposurePostMoveSettingsSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()

	FMovieSceneComposurePostMoveSettingsSectionTemplate() {}
	FMovieSceneComposurePostMoveSettingsSectionTemplate(const UMovieSceneComposurePostMoveSettingsSection& Section, const UMovieScenePropertyTrack& Track);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FRichCurve Pivot[2];

	UPROPERTY()
	FRichCurve Translation[2];

	UPROPERTY()
	FRichCurve RotationAngle;

	UPROPERTY()
	FRichCurve Scale;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

template<> COMPOSURE_API FMovieSceneAnimTypeID GetBlendingDataType<FComposurePostMoveSettings>();

template<> struct TBlendableTokenTraits<FComposurePostMoveSettings>
{
	typedef MovieScene::TMaskedBlendable<float, 6> WorkingDataType;
};

namespace MovieScene
{
	inline void MultiChannelFromData(const FComposurePostMoveSettings& In, TMultiChannelValue<float, 6>& Out)
	{
		Out = { In.Pivot.X, In.Pivot.Y, In.Translation.X, In.Translation.Y, In.RotationAngle, In.Scale };
	}

	inline void ResolveChannelsToData(const TMultiChannelValue<float, 6>& In, FComposurePostMoveSettings& Out)
	{
		Out.Pivot = FVector2D(In[0], In[1]);
		Out.Translation = FVector2D(In[2], In[3]);
		Out.RotationAngle = In[4];
		Out.Scale = In[5];
	}
}
