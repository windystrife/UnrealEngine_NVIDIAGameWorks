// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "MovieScenePropertyTrack.generated.h"

/**
 * Base class for tracks that animate an object property
 */
UCLASS(abstract)
class MOVIESCENETRACKS_API UMovieScenePropertyTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
	virtual bool CanRename() const override { return false; }
	virtual FName GetTrackName() const override;
#endif

	virtual void PostLoad() override;

public:

	/**
	 * Sets the property name for this animatable property
	 *
	 * @param InPropertyName The property being animated
	 */
	virtual void SetPropertyNameAndPath(FName InPropertyName, const FString& InPropertyPath);

	/** @return the name of the property being animated by this track */
	FName GetPropertyName() const { return PropertyName; }
	
	/** @return The property path for this track */
	const FString& GetPropertyPath() const { return PropertyPath; }

	/**
	 * Finds a section at the current time.
	 *
	 * @param Time	The time relative to the owning movie scene where the section should be
	 * @param bSectionAdded Whether a section was added or not
	 * @return The found section, or the new section.
	 */
	class UMovieSceneSection* FindOrAddSection(float Time, bool& bSectionAdded);

#if WITH_EDITORONLY_DATA
	/** Unique name for this track to afford multiple tracks on a given object (i.e. for array properties) */
	UPROPERTY()
	FName UniqueTrackName;
#endif

protected:

	/** Name of the property being changed */
	UPROPERTY()
	FName PropertyName;

	/** Path to the property from the source object being changed */
	UPROPERTY()
	FString PropertyPath;

	/** All the sections in this list */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
