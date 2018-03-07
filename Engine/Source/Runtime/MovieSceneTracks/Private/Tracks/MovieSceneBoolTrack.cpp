// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneBoolTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneSection* UMovieSceneBoolTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneBoolSection::StaticClass(), NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieSceneBoolTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneBoolPropertySectionTemplate(*CastChecked<const UMovieSceneBoolSection>(&InSection), *this);
}

bool UMovieSceneBoolTrack::Eval( float Position, float LastPostion, bool& InOutBool ) const
{	
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if( Section )
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		InOutBool = CastChecked<UMovieSceneBoolSection>( Section )->Eval( Position, InOutBool );
	}

	return (Section != nullptr);
}
