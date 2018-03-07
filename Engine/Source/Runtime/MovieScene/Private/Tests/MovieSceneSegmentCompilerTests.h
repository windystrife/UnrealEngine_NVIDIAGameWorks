// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneSegmentCompilerTests.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneSegmentCompilerTestTrack : public UMovieSceneTrack
{
public:

	GENERATED_BODY()

	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override { return SectionArray; }
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetTrackCompilerRules() const override;

	UPROPERTY()
	bool bHighPassFilter;
	
	UPROPERTY()
	TArray<UMovieSceneSection*> SectionArray;
};

UCLASS(MinimalAPI)
class UMovieSceneSegmentCompilerTestSection : public UMovieSceneSection
{
public:
	GENERATED_BODY()
};

