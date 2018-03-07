// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneAudioTrack.generated.h"

class USoundBase;

namespace AudioTrackConstants
{
	const float ScrubDuration = 0.050f;
}


/**
 * Handles manipulation of audio.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneAudioTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new sound cue to the audio */
	virtual void AddNewSound(USoundBase* Sound, float Time);

	/** @return The audio sections on this track */
	const TArray<UMovieSceneSection*>& GetAudioSections() const
	{
		return AudioSections;
	}

	/** @return true if this is a master audio track */
	bool IsAMasterTrack() const;

	float GetSoundDuration(USoundBase* Sound);

public:

	// UMovieSceneTrack interface

	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool SupportsMultipleRows() const override;
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetRowCompilerRules() const override;

private:

	/** List of all master audio sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AudioSections;

#if WITH_EDITORONLY_DATA

public:

	/**
	 * Get the height of this track's rows
	 */
	int32 GetRowHeight() const
	{
		return RowHeight;
	}

	/**
	 * Set the height of this track's rows
	 */
	void SetRowHeight(int32 NewRowHeight)
	{
		RowHeight = FMath::Max(16, NewRowHeight);
	}

private:

	/** The height for each row of this track */
	UPROPERTY()
	int32 RowHeight;

#endif
};
