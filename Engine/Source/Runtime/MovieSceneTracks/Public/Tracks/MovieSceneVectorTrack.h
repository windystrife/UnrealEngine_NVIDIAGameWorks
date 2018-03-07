// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"

#include "MovieSceneVectorTrack.generated.h"

/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneVectorTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	/** @return Get the number of channels used by the vector */
	int32 GetNumChannelsUsed() const { return NumChannelsUsed; }

	/** Set the number of channels used by the vector */
	void SetNumChannelsUsed( int32 InNumChannelsUsed ) { NumChannelsUsed = InNumChannelsUsed; }

private:
	/** The number of channels used by the vector (2,3, or 4) */
	UPROPERTY()
	int32 NumChannelsUsed;
};
