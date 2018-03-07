// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneEvalTemplate.h"
#include "MovieScenePropertyTemplate.h"

#include "MovieSceneMediaTemplate.generated.h"

class UMovieSceneMediaSection;
class UMovieSceneMediaTrack;
class UMediaSource;


USTRUCT()
struct FMovieSceneMediaSectionParams
{
	GENERATED_BODY()

	UPROPERTY()
	float SectionStartTime;

	UPROPERTY()
	UMediaSource* Source;

	UPROPERTY()
	FString Proxy;
};


USTRUCT()
struct FMovieSceneMediaSectionTemplate
	: public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	/** Default constructor. */
	FMovieSceneMediaSectionTemplate() {}

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InSection
	 * @param InTrack
	 */
	FMovieSceneMediaSectionTemplate(const UMovieSceneMediaSection& InSection, const UMovieSceneMediaTrack& InTrack);

public:

	//~ FMovieSceneEvalTemplate interface

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual UScriptStruct& GetScriptStructImpl() const override;
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void SetupOverrides() override;

private:

	UPROPERTY()
	FMovieScenePropertySectionData PropertyData;

	UPROPERTY()
	FMovieSceneMediaSectionParams Params;
};
