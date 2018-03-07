// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/BoolPropertyTrackEditor.h"
#include "Sections/BoolPropertySection.h"


TSharedRef<ISequencerTrackEditor> FBoolPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FBoolPropertyTrackEditor(OwningSequencer));
}


TSharedRef<ISequencerSection> FBoolPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FBoolPropertyTrackEditor"));
	return MakeShareable(new FBoolPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FBoolPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<bool>& NewGeneratedKeys, TArray<bool>& DefaultGeneratedKeys )
{
	NewGeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<bool>() );
}
