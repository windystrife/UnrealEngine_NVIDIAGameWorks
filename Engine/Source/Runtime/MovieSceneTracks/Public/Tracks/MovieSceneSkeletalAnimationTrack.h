// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneSkeletalAnimationTrack.generated.h"

/**
 * Handles animation of skeletal mesh actors
 */
UCLASS(MinimalAPI)
class UMovieSceneSkeletalAnimationTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new animation to this track */
	virtual void AddNewAnimation(float KeyTime, class UAnimSequenceBase* AnimSequence);

	/** Gets the animation sections at a certain time */
	TArray<UMovieSceneSection*> GetAnimSectionsAtTime(float Time);

public:

	// UMovieSceneTrack interface

	virtual void PostLoad() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetRowCompilerRules() const override;
	virtual bool SupportsMultipleRows() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:

	/** List of all animation sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AnimationSections;

	UPROPERTY()
	bool bUseLegacySectionIndexBlend;
};
