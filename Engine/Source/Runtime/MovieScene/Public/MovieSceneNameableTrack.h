// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneTrack.h"
#include "MovieSceneNameableTrack.generated.h"


/**
 * Base class for movie scene tracks that can be renamed by the user.
 */
UCLASS(abstract, MinimalAPI)
class UMovieSceneNameableTrack
	: public UMovieSceneTrack
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
public:

	/**
	 * Set the track's display name.
	 *
	 * @param NewName The name to set.
	 */
	MOVIESCENE_API void SetDisplayName(const FText& NewDisplayName);

	/**
	 * Can rename this track.
	 *
	 * @return Whether this track can be renamed.
	 */
	MOVIESCENE_API virtual bool CanRename() const { return true; }

public:

	// UMovieSceneTrack interface

	MOVIESCENE_API virtual FText GetDisplayName() const override;
	MOVIESCENE_API virtual FText GetDefaultDisplayName() const;

private:

	/** The track's human readable display name. */
	UPROPERTY(EditAnywhere, Category="Track")
	FText DisplayName;
#endif
};
