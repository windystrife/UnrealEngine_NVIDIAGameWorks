// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"

#include "MovieSceneMediaSection.generated.h"

class UMediaSource;


/**
 * Implements a movie scene section for media playback.
 */
UCLASS(MinimalAPI)
class UMovieSceneMediaSection
	: public UMovieSceneSection
{
public:

	GENERATED_BODY()

	/** The media source proxy to use. */
	UPROPERTY(EditAnywhere, Category="Media")
	FString Proxy;

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer The object initializer.
	 */
	UMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer);

public:

	/**
	 * Get this section's video source.
	 *
	 * @return The media source object.
	 * @see SetMediaSource
	 */
	UMediaSource* GetMediaSource() const
	{
		return MediaSource;
	}

	/**
	 * Set this section's video source.
	 *
	 * @param InMediaSource The media source object to set.
	 * @see GetMediaSource
	 */
	void SetMediaSource(UMediaSource* InMediaSource)
	{
		MediaSource = InMediaSource;
	}

#if WITH_EDITORONLY_DATA

	/** @return The thumbnail reference frame offset from the start of this section */
	float GetThumbnailReferenceOffset() const
	{
		return ThumbnailReferenceOffset;
	}

	/** Set the thumbnail reference offset */
	void SetThumbnailReferenceOffset(float InNewOffset)
	{
		Modify();
		ThumbnailReferenceOffset = InNewOffset;
	}

#endif //WITH_EDITORONLY_DATA

private:

	/** The source to play with this video track */
	UPROPERTY(EditAnywhere, Category="Media")
	UMediaSource* MediaSource;

#if WITH_EDITORONLY_DATA

	/** The reference frame offset for single thumbnail rendering */
	UPROPERTY()
	float ThumbnailReferenceOffset;

#endif //WITH_EDITORONLY_DATA
};
