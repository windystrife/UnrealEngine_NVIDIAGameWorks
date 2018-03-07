// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneTrack.h"
#include "Curves/IntegralCurve.h"
#include "MovieSceneSpawnTrack.generated.h"

struct FMovieSceneEvaluationTrack;

/**
 * Handles when a spawnable should be spawned and destroyed
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneSpawnTrack
	: public UMovieSceneTrack
{
public:

	UMovieSceneSpawnTrack(const FObjectInitializer& Obj);
	GENERATED_BODY()

public:

	/** Get the object identifier that this spawn track controls */
	const FGuid& GetObjectId() const
	{
		return ObjectGuid;
	}

	/** Set the object identifier that this spawn track controls */
	void SetObjectId(const FGuid& InGuid)
	{
		ObjectGuid = InGuid;
	}

	static uint16 GetEvaluationPriority() { return uint16(0xFFF); }

public:

	// UMovieSceneTrack interface
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const override;
	virtual void PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	//~ UObject interface
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual ECookOptimizationFlags GetCookOptimizationFlags() const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

protected:

	/** All the sections in this track */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;

	/** The guid relating to the object we are to spawn and destroy */
	UPROPERTY()
	FGuid ObjectGuid;
};
