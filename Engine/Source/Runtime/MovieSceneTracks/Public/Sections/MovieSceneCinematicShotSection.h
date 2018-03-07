// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieSceneCinematicShotSection.generated.h"

/**
 * Implements a cinematic shot section.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneCinematicShotSection
	: public UMovieSceneSubSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneCinematicShotSection();

public:

	/** @return The shot display name */
	FText GetShotDisplayName() const
	{
		return DisplayName;
	}

	/** Set the shot display name */
	void SetShotDisplayName(const FText& InDisplayName)
	{
		if (TryModify())
		{
			DisplayName = InDisplayName;
		}
	}

private:

	/** The Shot's display name */
	UPROPERTY()
	FText DisplayName;

#if WITH_EDITORONLY_DATA
public:
	/** @return The shot thumbnail reference frame offset from the start of this section */
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

private:

	/** The shot's reference frame offset for single thumbnail rendering */
	UPROPERTY()
	float ThumbnailReferenceOffset;
#endif
};
