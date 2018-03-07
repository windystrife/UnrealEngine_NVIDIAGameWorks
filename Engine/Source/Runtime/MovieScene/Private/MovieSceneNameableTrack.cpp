// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNameableTrack.h"


#define LOCTEXT_NAMESPACE "MovieSceneNameableTrack"


/* UMovieSceneNameableTrack interface
 *****************************************************************************/

#if WITH_EDITORONLY_DATA

void UMovieSceneNameableTrack::SetDisplayName(const FText& NewDisplayName)
{
	if (NewDisplayName.EqualTo(DisplayName))
	{
		return;
	}

	SetFlags(RF_Transactional);
	Modify();

	DisplayName = NewDisplayName;
}

#endif


/* UMovieSceneTrack interface
 *****************************************************************************/

#if WITH_EDITORONLY_DATA

FText UMovieSceneNameableTrack::GetDisplayName() const
{
	if (DisplayName.IsEmpty())
	{
		return GetDefaultDisplayName();
	}

	return DisplayName;
}

FText UMovieSceneNameableTrack::GetDefaultDisplayName() const

{ 
	return LOCTEXT("UnnamedTrackName", "Unnamed Track"); 
}

#endif


#undef LOCTEXT_NAMESPACE
