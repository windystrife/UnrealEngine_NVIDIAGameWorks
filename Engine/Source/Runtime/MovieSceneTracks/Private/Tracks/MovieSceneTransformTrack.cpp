// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneTransformTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Evaluation/MovieScenePropertyTemplates.h"


UMovieSceneTransformTrack::UMovieSceneTransformTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(65, 173, 164, 65);
#endif

	SupportedBlendTypes = FMovieSceneBlendTypeField::All();

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

UMovieSceneSection* UMovieSceneTransformTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieScene3DTransformSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneTransformTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneTransformPropertySectionTemplate(*CastChecked<UMovieScene3DTransformSection>(&InSection), *this);
}

FMovieSceneInterrogationKey UMovieSceneTransformTrack::GetInterrogationKey()
{
	static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
