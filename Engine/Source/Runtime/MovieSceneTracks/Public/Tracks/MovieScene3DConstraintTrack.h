// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneTrack.h"
#include "MovieScene3DConstraintTrack.generated.h"

/**
 * Base class for constraint tracks (tracks that are dependent upon other objects).
 */
UCLASS( MinimalAPI )
class UMovieScene3DConstraintTrack
	: public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Adds a constraint.
	 *
	 * @param Time The time relative to the owning movie scene where the section should be.
	 * @param ConstraintEndTime Set the constraint to end at this time.
	 * @param SocketName The socket name for the constraint.
	 * @param ComponentName The name of the component the socket resides in.
	 * @param ConstraintId The id to the constraint.
	 */
	virtual void AddConstraint(float Time, float ConstraintEndTime, const FName SocketName, const FName ComponentName, const FGuid& ConstraintId) { }

public:

	// UMovieSceneTrack interface

    virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

protected:

	/** List of all constraint sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> ConstraintSections;
};
