// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "MovieSceneSectionParameters.generated.h"

USTRUCT()
struct FMovieSceneSectionParameters
{
	GENERATED_BODY()

	/** Default constructor */
	FMovieSceneSectionParameters()
		: StartOffset(0.0f)
		, TimeScale(1.0f)
		, HierarchicalBias(100)
		, PrerollTime_DEPRECATED(0.0f)
		, PostrollTime_DEPRECATED(0.0f)
	{}

	/** Number of seconds to skip at the beginning of the sub-sequence. */
	UPROPERTY(EditAnywhere, Category="Clipping")
	float StartOffset;

	/** Playback time scaling factor. */
	UPROPERTY(EditAnywhere, Category="Timing")
	float TimeScale;

	/** Hierachical bias. Higher bias will take precedence. */
	UPROPERTY(EditAnywhere, Category="Sequence")
	int32 HierarchicalBias;

	UPROPERTY()
	float PrerollTime_DEPRECATED;
	UPROPERTY()
	float PostrollTime_DEPRECATED;
};
