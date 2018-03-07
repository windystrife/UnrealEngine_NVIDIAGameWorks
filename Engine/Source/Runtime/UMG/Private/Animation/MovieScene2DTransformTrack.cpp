// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "MovieSceneCommonHelpers.h"
#include "Animation/MovieScene2DTransformTemplate.h"

UMovieScene2DTransformTrack::UMovieScene2DTransformTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(48, 227, 255, 65);
#endif

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}

UMovieSceneSection* UMovieScene2DTransformTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieScene2DTransformSection::StaticClass(), NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieScene2DTransformTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieScene2DTransformSectionTemplate(*CastChecked<const UMovieScene2DTransformSection>(&InSection), *this);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool UMovieScene2DTransformTrack::Eval(float Position, float LastPosition, FWidgetTransform& InOutTransform) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime(Sections, Position);

	if(Section)
	{
		const UMovieScene2DTransformSection* TransformSection = CastChecked<UMovieScene2DTransformSection>(Section);

		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		InOutTransform = TransformSection->Eval(Position, InOutTransform);
	}

	return (Section != nullptr);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
