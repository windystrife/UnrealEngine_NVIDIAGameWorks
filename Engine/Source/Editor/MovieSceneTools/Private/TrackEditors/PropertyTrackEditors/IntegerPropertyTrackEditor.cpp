// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/IntegerPropertyTrackEditor.h"
#include "Sections/IntegerPropertySection.h"


TSharedRef<ISequencerTrackEditor> FIntegerPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FIntegerPropertyTrackEditor(OwningSequencer));
}


TSharedRef<ISequencerSection> FIntegerPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	if (PropertyTrack != nullptr)
	{
		return MakeShareable(new FIntegerPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
	}
	else
	{
		return MakeShareable(new FIntegerPropertySection(SectionObject, Track.GetDisplayName()));
	}
}


void FIntegerPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<int32>& NewGeneratedKeys, TArray<int32>& DefaultGeneratedKeys )
{
	NewGeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<int32>() );
}
