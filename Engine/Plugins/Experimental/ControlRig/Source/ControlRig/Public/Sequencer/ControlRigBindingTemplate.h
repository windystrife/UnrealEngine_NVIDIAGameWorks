// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Evaluation/MovieSceneSpawnTemplate.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "ControlRigBindingTemplate.generated.h"

class UMovieSceneSpawnSection;

/** Binding track eval template based off spawn template */
USTRUCT()
struct CONTROLRIG_API FControlRigBindingTemplate : public FMovieSceneSpawnSectionTemplate
{
	GENERATED_BODY()
	
	FControlRigBindingTemplate()
		: ObjectBindingSequenceID(MovieSceneSequenceID::Root)
		, bApplyBoneFilter(false)
		, bAdditive(false)
	{
		WeightCurve.SetDefaultValue(1.0f);
	}

	FControlRigBindingTemplate(const UMovieSceneSpawnSection& SpawnSection);

	static FMovieSceneAnimTypeID GetAnimTypeID();

	/** Set the object & sequence we are bound to */
	void SetObjectBindingId(FGuid InObjectBindingId, FMovieSceneSequenceIDRef InObjectBindingSequenceID);

	/** Set the weight curve for this template */
	void SetWeightCurve(const FRichCurve& InWeightCurve, float Offset, float Scale);

	/** Set whether we are additive */
	void SetAdditive(bool bInAdditive) { bAdditive = bInAdditive; }

	/** Set whether we only apply modified bones */
	void SetPerBoneBlendFilter(bool bInApplyBoneFilter, const FInputBlendPose& InBoneFilter);

	// Bind to a runtime (non-sequencer-controlled) object
#if WITH_EDITORONLY_DATA
	static void SetObjectBinding(TWeakObjectPtr<> InObjectBinding);
	static UObject* GetObjectBinding();
	static void ClearObjectBinding();
#endif

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

private:
#if WITH_EDITORONLY_DATA
	/** The current external object binding we are using */
	static TWeakObjectPtr<UObject> ObjectBinding;
#endif

	/** The current internal object binding we are using */
	UPROPERTY()
	FGuid ObjectBindingId;

	/** The current sequence of the internal object binding we are using */
	UPROPERTY()
	FMovieSceneSequenceID ObjectBindingSequenceID;

	/** Weight curve to evaluate this rig with */
	UPROPERTY()
	FRichCurve WeightCurve;

	/** Per-bone filter to apply to our animation */
	UPROPERTY()
	FInputBlendPose BoneFilter;

	/** Only apply bones that are in the filter */
	UPROPERTY()
	bool bApplyBoneFilter;

	/** Whether we are additive */
	UPROPERTY()
	bool bAdditive;
};
