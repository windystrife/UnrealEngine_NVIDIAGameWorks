// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneParameterSection.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneParticleParameterTrack.generated.h"

/**
 * Handles manipulation of material parameters in a movie scene.
 */
UCLASS( MinimalAPI )
class UMovieSceneParticleParameterTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif


public:

	/**
	 * Adds a scalar parameter key to the track. 
	 * @param ParameterName The name of the parameter to add a key for.
	 * @param Time The time to add the new key.
	 * @param The value for the new key.
	 */
	void MOVIESCENETRACKS_API AddScalarParameterKey( FName ParameterName, float Position, float Value );

	/**
	* Adds a Vector parameter key to the track.
	* @param ParameterName The name of the parameter to add a key for.
	* @param Time The time to add the new key.
	* @param The value for the new key.
	*/
	void MOVIESCENETRACKS_API AddVectorParameterKey( FName ParameterName, float Position, FVector Value );

	/**
	* Adds a Vector parameter key to the track.
	* @param ParameterName The name of the parameter to add a key for.
	* @param Time The time to add the new key.
	* @param The value for the new key.
	*/
	void MOVIESCENETRACKS_API AddColorParameterKey( FName ParameterName, float Position, FLinearColor Value );

private:

	/** The sections owned by this track .*/
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
