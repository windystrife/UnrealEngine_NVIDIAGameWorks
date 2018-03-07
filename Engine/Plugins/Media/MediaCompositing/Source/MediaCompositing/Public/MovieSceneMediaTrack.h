// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "MovieScenePropertyTrack.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneMediaTrack.generated.h"


/**
 * Implements a movie scene track for media playback.
 */
UCLASS(MinimalAPI)
class UMovieSceneMediaTrack
	: public UMovieScenePropertyTrack
{
public:

	GENERATED_BODY()

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer The object initializer.
	 */
	UMovieSceneMediaTrack(const FObjectInitializer& ObjectInitializer);

public:

	//~ UMovieScenePropertyTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool SupportsMultipleRows() const override { return true; }

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
	virtual FName GetTrackName() const override;
#endif
};
