// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEnumTrack.h"
#include "MovieSceneEnumSection.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneEnumTrack::UMovieSceneEnumTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneEnumTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneEnumSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneEnumTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneEnumPropertySectionTemplate(*CastChecked<UMovieSceneEnumSection>(&InSection), *this);
}

void UMovieSceneEnumTrack::SetEnum(UEnum* InEnum)
{
	Enum = InEnum;
}


class UEnum* UMovieSceneEnumTrack::GetEnum() const
{
	return Enum;
}
