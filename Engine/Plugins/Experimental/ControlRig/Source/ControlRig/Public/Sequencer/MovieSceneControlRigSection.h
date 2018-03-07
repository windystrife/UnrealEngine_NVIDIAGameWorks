// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Sections/MovieSceneSubSection.h"
#include "ControlRig.h"
#include "MovieSceneSequencePlayer.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "MovieSceneControlRigSection.generated.h"

class UControlRigSequence;

/**
 * Movie scene section that controls animation controller animation
 */
UCLASS()
class CONTROLRIG_API UMovieSceneControlRigSection : public UMovieSceneSubSection
{
	GENERATED_BODY()

public:
	/** Blend this track in additively (using the reference pose as a base) */
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bAdditive;

	/** Only apply bones that are in the filter */
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bApplyBoneFilter;

	/** Per-bone filter to apply to our animation */
	UPROPERTY(EditAnywhere, Category = "Animation", meta=(EditCondition=bApplyBoneFilter))
	FInputBlendPose BoneFilter;

	/** The weight curve for this animation controller section */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FRichCurve Weight;

public:

	UMovieSceneControlRigSection();

	//~ MovieSceneSection interface

	virtual void MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;

	//~ UMovieSceneSubSection interface

	virtual FMovieSceneEvaluationTemplate& GenerateTemplateForSubSequence(const FMovieSceneTrackCompilerArgs& InArgs) const override;
	virtual FMovieSceneSubSequenceData GenerateSubSequenceData() const override;
};
