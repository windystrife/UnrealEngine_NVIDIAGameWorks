// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScene3DConstraintTrack.h"
#include "MovieScene3DAttachTrack.generated.h"

/**
 * Handles manipulation of path tracks in a movie scene.
 */
UCLASS( MinimalAPI )
class UMovieScene3DAttachTrack
	: public UMovieScene3DConstraintTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieScene3DConstraintTrack interface

	virtual void AddConstraint( float Time, float ConstraintEndTime, const FName SocketName, const FName ComponentName, const FGuid& ConstraintId ) override;
	virtual class UMovieSceneSection* CreateNewSection() override;

public:

	// UMovieSceneTrack interface

	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif
};
