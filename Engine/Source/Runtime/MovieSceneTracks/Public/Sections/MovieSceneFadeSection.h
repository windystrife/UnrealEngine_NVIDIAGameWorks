// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneFloatSection.h"
#include "MovieSceneFadeSection.generated.h"


/**
 * A single floating point section.
 */
UCLASS(MinimalAPI)
class UMovieSceneFadeSection
	: public UMovieSceneFloatSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneFadeSection();

public:

	/** Fade color. */
	UPROPERTY(EditAnywhere, Category="Fade", meta=(InlineColorPicker))
	FLinearColor FadeColor;

	/** Fade audio. */
	UPROPERTY(EditAnywhere, Category="Fade")
	uint32 bFadeAudio:1;
};
