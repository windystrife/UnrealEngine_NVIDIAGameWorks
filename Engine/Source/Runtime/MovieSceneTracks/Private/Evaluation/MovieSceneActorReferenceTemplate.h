// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Curves/IntegralCurve.h"
#include "MovieSceneActorReferenceTemplate.generated.h"

class UMovieSceneActorReferenceSection;
class UMovieScenePropertyTrack;

USTRUCT()
struct FMovieSceneActorReferenceSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneActorReferenceSectionTemplate(){}
	FMovieSceneActorReferenceSectionTemplate(const UMovieSceneActorReferenceSection& Section, const UMovieScenePropertyTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}
	virtual void SetupOverrides() override
	{
		EnableOverrides(RequiresSetupFlag);
	}
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override
	{
		PropertyData.SetupTrack(PersistentData);
	}

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieScenePropertySectionData PropertyData;

	UPROPERTY()
	FIntegralCurve ActorGuidIndexCurve;

	UPROPERTY()
	TArray<FGuid> ActorGuids;
};
