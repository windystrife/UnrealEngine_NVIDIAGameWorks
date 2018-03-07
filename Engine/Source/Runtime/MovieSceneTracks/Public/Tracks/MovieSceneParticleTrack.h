// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneParticleTrack.generated.h"

/**
 * Handles triggering of particle emitters
 */
UCLASS(MinimalAPI)
class UMovieSceneParticleTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Get the track's particle sections.
	 *
	 * @return Particle sections collection.
	 */
	virtual TArray<UMovieSceneSection*> GetAllParticleSections() const
	{
		return ParticleSections;
	}

public:

	// UMovieSceneTrack interface

	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void AddNewSection(float SectionTime);
	virtual UMovieSceneSection* CreateNewSection() override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:

	/** List of all particle sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> ParticleSections;
};
