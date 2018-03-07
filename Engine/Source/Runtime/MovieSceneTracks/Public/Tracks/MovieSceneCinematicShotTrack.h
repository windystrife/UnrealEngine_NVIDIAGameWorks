// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/InlineValue.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "MovieSceneCinematicShotTrack.generated.h"

class UMovieSceneSequence;
class UMovieSceneSubSection;

/**
 * A track that holds consecutive sub sequences.
 */
UCLASS(MinimalAPI)
class UMovieSceneCinematicShotTrack
	: public UMovieSceneSubTrack
{
	GENERATED_BODY()

public:

	UMovieSceneCinematicShotTrack(const FObjectInitializer& ObjectInitializer);
	
	// UMovieSceneSubTrack interface

	MOVIESCENETRACKS_API virtual UMovieSceneSubSection* AddSequence(UMovieSceneSequence* Sequence, float StartTime, float Duration, const bool& bInsertSequence = false);

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool SupportsMultipleRows() const override;
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetRowCompilerRules() const override;
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetTrackCompilerRules() const override;
	
#if WITH_EDITOR
	virtual void OnSectionMoved(UMovieSceneSection& Section) override;
#endif

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif
};
