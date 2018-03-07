// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneWidgetMaterialTrack.h"
#include "Animation/WidgetMaterialTrackUtilities.h"
#include "Animation/MovieSceneWidgetMaterialTemplate.h"

UMovieSceneWidgetMaterialTrack::UMovieSceneWidgetMaterialTrack( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}


FMovieSceneEvalTemplatePtr UMovieSceneWidgetMaterialTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneWidgetMaterialSectionTemplate(*CastChecked<UMovieSceneParameterSection>(&InSection), *this);
}


FName UMovieSceneWidgetMaterialTrack::GetTrackName() const
{ 
	return TrackName;
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneWidgetMaterialTrack::GetDefaultDisplayName() const
{
	return FText::Format(NSLOCTEXT("UMGAnimation", "MaterialTrackFormat", "{0} Material"), FText::FromName( TrackName ) );
}
#endif


void UMovieSceneWidgetMaterialTrack::SetBrushPropertyNamePath( TArray<FName> InBrushPropertyNamePath )
{
	BrushPropertyNamePath = InBrushPropertyNamePath;
	TrackName = WidgetMaterialTrackUtilities::GetTrackNameFromPropertyNamePath( BrushPropertyNamePath );
}
