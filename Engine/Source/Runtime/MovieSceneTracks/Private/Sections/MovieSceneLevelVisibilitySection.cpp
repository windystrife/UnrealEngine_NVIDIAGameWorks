// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Evaluation/MovieSceneLevelVisibilityTemplate.h"


UMovieSceneLevelVisibilitySection::UMovieSceneLevelVisibilitySection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Visibility = ELevelVisibility::Visible;
}


ELevelVisibility UMovieSceneLevelVisibilitySection::GetVisibility() const
{
	return Visibility;
}


void UMovieSceneLevelVisibilitySection::SetVisibility( ELevelVisibility InVisibility )
{
	Visibility = InVisibility;
}


TArray<FName>* UMovieSceneLevelVisibilitySection::GetLevelNames()
{
	return &LevelNames;
}


FMovieSceneEvalTemplatePtr UMovieSceneLevelVisibilitySection::GenerateTemplate() const
{
	return FMovieSceneLevelVisibilitySectionTemplate(*this);
}


TOptional<float> UMovieSceneLevelVisibilitySection::GetKeyTime(FKeyHandle KeyHandle) const
{
	return TOptional<float>();
}


void UMovieSceneLevelVisibilitySection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	// do nothing
}
