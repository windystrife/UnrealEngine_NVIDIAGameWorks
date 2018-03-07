// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "MovieSceneWidgetMaterialTrack.generated.h"

/**
 * A material track which is specialized for materials which are owned by widget brushes.
 */
UCLASS(MinimalAPI)
class UMovieSceneWidgetMaterialTrack
	: public UMovieSceneMaterialTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual FName GetTrackName() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

public:

	/** Gets name of the brush property which has the material to animate. */
	const TArray<FName>& GetBrushPropertyNamePath() const { return BrushPropertyNamePath; }

	/** Sets the name of the brush property which has the material to animate. */
	UMG_API void SetBrushPropertyNamePath( TArray<FName> InBrushPropertyNamePath );

private:

	/** The name of the brush property which will be animated by this track. */
	UPROPERTY()
	TArray<FName> BrushPropertyNamePath;

	/** The name of this track, generated from the property name path. */
	UPROPERTY()
	FName TrackName;
};
