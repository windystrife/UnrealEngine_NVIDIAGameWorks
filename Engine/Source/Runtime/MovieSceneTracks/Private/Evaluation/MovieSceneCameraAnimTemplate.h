// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneCameraAnimSection.h"
#include "Sections/MovieSceneCameraShakeSection.h"

#include "MovieSceneCameraAnimTemplate.generated.h"

class ACameraActor;

/** Generic section template for any additive camera animation effects */
USTRUCT()
struct FMovieSceneAdditiveCameraAnimationTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	virtual bool EnsureSetup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const { return false; }
	virtual bool UpdateCamera(ACameraActor& TempCameraActor, FMinimalViewInfo& POV, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData) const { return false; }

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { check(false); return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};

/** Section template for a camera anim */
USTRUCT()
struct FMovieSceneCameraAnimSectionTemplate : public FMovieSceneAdditiveCameraAnimationTemplate
{
	GENERATED_BODY()
	
	FMovieSceneCameraAnimSectionTemplate() {}
	FMovieSceneCameraAnimSectionTemplate(const UMovieSceneCameraAnimSection& Section);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

	virtual bool EnsureSetup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual bool UpdateCamera(ACameraActor& TempCameraActor, FMinimalViewInfo& POV, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData) const override;

	/** Source data taken from the section */
	UPROPERTY()
	FMovieSceneCameraAnimSectionData SourceData;

	/** Cached section start time */
	UPROPERTY()
	float SectionStartTime;
};

/** Section template for a camera anim */
USTRUCT()
struct FMovieSceneCameraShakeSectionTemplate : public FMovieSceneAdditiveCameraAnimationTemplate
{
	GENERATED_BODY()
	
	FMovieSceneCameraShakeSectionTemplate() {}
	FMovieSceneCameraShakeSectionTemplate(const UMovieSceneCameraShakeSection& Section);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

	virtual bool EnsureSetup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual bool UpdateCamera(ACameraActor& TempCameraActor, FMinimalViewInfo& POV, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData) const override;


	/** Source data taken from the section */
	UPROPERTY()
	FMovieSceneCameraShakeSectionData SourceData;

	/** Cached section start time */
	UPROPERTY()
	float SectionStartTime;
};


USTRUCT()
struct FMovieSceneAdditiveCameraAnimationTrackTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneAdditiveCameraAnimationTrackTemplate();

	static const FMovieSceneSharedDataId SharedDataId;

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override
	{
		EnableOverrides(RequiresInitializeFlag);
	}
	
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
