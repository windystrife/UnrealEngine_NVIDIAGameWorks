// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneBoolSection.h"
#include "MovieSceneSpawnSection.generated.h"

/**
 * A spawn section.
 */
UCLASS(MinimalAPI)
class UMovieSceneSpawnSection 
	: public UMovieSceneBoolSection
{
	GENERATED_BODY()

	UMovieSceneSpawnSection(const FObjectInitializer& Init);
};
