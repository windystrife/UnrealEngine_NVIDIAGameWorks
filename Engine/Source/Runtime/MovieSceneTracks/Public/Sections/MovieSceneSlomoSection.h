// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneFloatSection.h"
#include "MovieSceneSlomoSection.generated.h"


/**
 * A single floating point section.
 */
UCLASS(MinimalAPI)
class UMovieSceneSlomoSection
	: public UMovieSceneFloatSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneSlomoSection();
};
