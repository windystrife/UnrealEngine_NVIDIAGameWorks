// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "ControlRigBindingTrack.generated.h"



/**
 * Handles when a controller should be bound and unbound
 */
UCLASS(MinimalAPI)
class UControlRigBindingTrack : public UMovieSceneSpawnTrack
{
public:

	GENERATED_BODY()
	
public:

	// UMovieSceneTrack interface
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif
};
