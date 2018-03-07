// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Animation/AnimSequenceBase.h"
#include "MovieSceneSkeletalAnimationSection.generated.h"

USTRUCT()
struct FMovieSceneSkeletalAnimationParams
{
	GENERATED_BODY()

	FMovieSceneSkeletalAnimationParams();

	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || Animation == nullptr ? 0.f : Animation->SequenceLength / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return Animation != nullptr ? Animation->SequenceLength : 0.f; }

	/** The animation this section plays */
	UPROPERTY(EditAnywhere, Category="Animation", meta=(AllowedClasses = "AnimSequence, AnimComposite"))
	UAnimSequenceBase* Animation;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float StartOffset;
	
	/** The offset into the end of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float EndOffset;
	
	/** The playback rate of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	uint32 bReverse:1;

	/** The slot name to use for the animation */
	UPROPERTY( EditAnywhere, Category = "Animation" )
	FName SlotName;

	/** The weight curve for this animation section */
	UPROPERTY( EditAnywhere, Category = "Animation" )
	FRichCurve Weight;	
};

/**
 * Movie scene section that control skeletal animation
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Animation", meta=(ShowOnlyInnerProperties))
	FMovieSceneSkeletalAnimationParams Params;

public:

	//~ MovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles  ) override;
	virtual UMovieSceneSection* SplitSection(float SplitTime) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<float> GetOffsetTime() const override { return TOptional<float>(Params.StartOffset); }
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override { return TOptional<float>(); }
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override { }
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;

	/** ~UObject interface */
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	float PreviousPlayRate;

#endif

private:

	UPROPERTY()
	class UAnimSequence* AnimSequence_DEPRECATED;

	UPROPERTY()
	UAnimSequenceBase* Animation_DEPRECATED;

	UPROPERTY()
	float StartOffset_DEPRECATED;
	
	UPROPERTY()
	float EndOffset_DEPRECATED;
	
	UPROPERTY()
	float PlayRate_DEPRECATED;

	UPROPERTY()
	uint32 bReverse_DEPRECATED:1;

	UPROPERTY()
	FName SlotName_DEPRECATED;
};
