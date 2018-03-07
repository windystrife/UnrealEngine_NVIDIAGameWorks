// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/InlineValue.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneSubTrack.generated.h"

class UMovieSceneSequence;
class UMovieSceneSubSection;
struct FMovieSceneSegmentCompilerRules;

/**
 * A track that holds sub-sequences within a larger sequence.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneSubTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:

	UMovieSceneSubTrack( const FObjectInitializer& ObjectInitializer );

	/**
	 * Adds a movie scene section at the requested time.
	 *
	 * @param Sequence The sequence to add
	 * @param StartTime The time to add the section at
	 * @param Duration The duration of the section
	 * @param bInsertSequence Whether or not to insert the sequence and push existing sequences out
	 * @return The newly created sub section
	 */
	virtual UMovieSceneSubSection* AddSequence(UMovieSceneSequence* Sequence, float StartTime, float Duration, const bool& bInsertSequence = false);

	/**
	 * Check whether this track contains the given sequence.
	 *
	 * @param Sequence The sequence to find.
	 * @param Recursively Whether to search for the sequence in sub-sequences.
	 * @return true if the sequence is in this track, false otherwise.
	 */
	bool ContainsSequence(const UMovieSceneSequence& Sequence, bool Recursively = false) const;

	/**
	 * Add a new sequence to record
	 */
	virtual UMovieSceneSubSection* AddSequenceToRecord();

public:

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveAllAnimationData() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool SupportsMultipleRows() const override;
	virtual void GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

protected:
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetTrackCompilerRules() const override;

protected:

	/** All movie scene sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
