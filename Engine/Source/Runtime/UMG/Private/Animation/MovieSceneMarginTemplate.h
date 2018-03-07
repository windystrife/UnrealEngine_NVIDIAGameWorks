// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "Layout/Margin.h"
#include "MovieSceneMarginTemplate.generated.h"

class UMovieSceneMarginSection;
class UMovieScenePropertyTrack;

Expose_TNameOf(FMargin);

USTRUCT()
struct FMovieSceneMarginSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneMarginSectionTemplate(){}
	FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FRichCurve TopCurve;

	UPROPERTY()
	FRichCurve LeftCurve;

	UPROPERTY()
	FRichCurve RightCurve;

	UPROPERTY()
	FRichCurve BottomCurve;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

/** Access the unique runtime type identifier for a margin. */
template<> UMG_API FMovieSceneAnimTypeID GetBlendingDataType<FMargin>();

/** Inform the blending accumulator to use a 4 channel float to blend margins */
template<> struct TBlendableTokenTraits<FMargin>
{
	typedef MovieScene::TMaskedBlendable<float, 4> WorkingDataType;
};

namespace MovieScene
{
	/** Convert a margin into a 4 channel blendable float */
	inline void MultiChannelFromData(FMargin In, TMultiChannelValue<float, 4>& Out)
	{
		Out = { In.Left, In.Top, In.Right, In.Bottom };
	}

	/** Convert a 4 channel blendable float into a margin */
	inline void ResolveChannelsToData(const TMultiChannelValue<float, 4>& In, FMargin& Out)
	{
		Out = FMargin(In[0], In[1], In[2], In[3]);
	}
}