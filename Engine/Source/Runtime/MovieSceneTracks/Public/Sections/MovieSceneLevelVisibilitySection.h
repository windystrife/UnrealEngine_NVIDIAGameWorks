// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "MovieSceneLevelVisibilitySection.generated.h"


/**
 * Visibility options for the level visibility section.
 */
UENUM()
enum class ELevelVisibility : uint8
{
	/** The streamed levels should be visible. */
	Visible,

	/** The streamed levels should be hidden. */
	Hidden
};


/**
 * A section for use with the movie scene level visibility track, which controls streamed level visibility.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneLevelVisibilitySection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	ELevelVisibility GetVisibility() const;
	void SetVisibility(ELevelVisibility InVisibility);

	TArray<FName>* GetLevelNames();
	const TArray<FName>& GetLevelNames() const { return LevelNames; }

public:

	//~ UMovieSceneSection interface
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Whether or not the levels in this section should be visible or hidden. */
	UPROPERTY(EditAnywhere, Category = LevelVisibility)
	ELevelVisibility Visibility;

	/** The short names of the levels who's visibility is controlled by this section. */
	UPROPERTY(EditAnywhere, Category = LevelVisibility)
	TArray<FName> LevelNames;
};
