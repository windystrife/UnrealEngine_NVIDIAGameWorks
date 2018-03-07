// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneStringTrack.generated.h"

/**
 * Implements a movie scene track that holds a series of strings.
 */
UCLASS(MinimalAPI)
class UMovieSceneStringTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	UMovieSceneStringTrack()
	{
#if WITH_EDITORONLY_DATA
		TrackTint = FColor(128, 128, 128);
#endif
	}

public:

	/**
	 * Adds an string to the appropriate section.
	 *
	 * This method will create a new section if no appropriate one exists.
	 *
	 * @param Time The time at which the string should be triggered.
	 * @param String The string to add.
	 * @param KeyParams The keying parameters
	 */
	bool AddKeyToSection(float Time, const FString& String);

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position The position at which to evaluate this track.
	 * @param LastPositionThe last playback position.
	 * @param InOutString The string at the evaluation time.
	 * @return true if anything was evaluated. Note: if false is returned InOutString remains unchanged
	 */
	DEPRECATED(4.15, "Direct evaluation of string tracks is no longer supported. Please create an evaluation template (see FMovieSceneStringPropertySectionTemplate).")
	virtual bool Eval(float Position, float LastPostion, FString& InOutString) const;

public:

	//~ UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveAllAnimationData() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif
};
