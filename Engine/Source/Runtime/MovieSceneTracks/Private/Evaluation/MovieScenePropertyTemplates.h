// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Curves/IntegralCurve.h"
#include "Curves/StringCurve.h"
#include "Templates/UnrealTypeTraits.h"
#include "MovieScene3DTransformTemplate.h"
#include "MovieSceneBlendType.h"
#include "MovieScenePropertyTemplates.generated.h"

class UMovieSceneBoolSection;
class UMovieSceneByteSection;
class UMovieSceneFloatSection;
class UMovieSceneIntegerSection;
class UMovieScenePropertyTrack;
class UMovieSceneStringSection;
class UMovieSceneVectorSection;
class UMovieSceneEnumSection;
class UMovieScene3DTransformSection;


USTRUCT()
struct FMovieSceneBoolPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneBoolPropertySectionTemplate() {}
	FMovieSceneBoolPropertySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override { EnableOverrides(RequiresSetupFlag); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FIntegralCurve BoolCurve;
};

USTRUCT()
struct FMovieSceneFloatPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneFloatPropertySectionTemplate(){}
	FMovieSceneFloatPropertySectionTemplate(const UMovieSceneFloatSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FRichCurve FloatCurve;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

USTRUCT()
struct FMovieSceneBytePropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneBytePropertySectionTemplate(){}
	FMovieSceneBytePropertySectionTemplate(const UMovieSceneByteSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override { EnableOverrides(RequiresSetupFlag); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FIntegralCurve ByteCurve;
};

USTRUCT()
struct FMovieSceneEnumPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneEnumPropertySectionTemplate(){}
	FMovieSceneEnumPropertySectionTemplate(const UMovieSceneEnumSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override { EnableOverrides(RequiresSetupFlag); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FIntegralCurve EnumCurve;
};

USTRUCT()
struct FMovieSceneIntegerPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneIntegerPropertySectionTemplate(){}
	FMovieSceneIntegerPropertySectionTemplate(const UMovieSceneIntegerSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FIntegralCurve IntegerCurve;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

USTRUCT()
struct FMovieSceneStringPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneStringPropertySectionTemplate(){}
	FMovieSceneStringPropertySectionTemplate(const UMovieSceneStringSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override { EnableOverrides(RequiresSetupFlag); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FStringCurve StringCurve;
};

Expose_TNameOf(FVector2D);
Expose_TNameOf(FVector);
Expose_TNameOf(FVector4);

USTRUCT()
struct FMovieSceneVectorPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneVectorPropertySectionTemplate(){}
	FMovieSceneVectorPropertySectionTemplate(const UMovieSceneVectorSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FRichCurve ComponentCurves[4];

	UPROPERTY()
	int32 NumChannelsUsed;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

Expose_TNameOf(FTransform);

USTRUCT()
struct FMovieSceneTransformPropertySectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()

	FMovieSceneTransformPropertySectionTemplate(){}
	FMovieSceneTransformPropertySectionTemplate(const UMovieScene3DTransformSection& Section, const UMovieScenePropertyTrack& Track);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieScene3DTransformTemplateData TemplateData;
};
