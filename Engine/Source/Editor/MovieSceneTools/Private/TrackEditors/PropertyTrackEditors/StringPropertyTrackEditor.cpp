// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/StringPropertyTrackEditor.h"
#include "Sections/StringPropertySection.h"


TSharedRef<ISequencerTrackEditor> FStringPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FStringPropertyTrackEditor(OwningSequencer));
}


TSharedRef<ISequencerSection> FStringPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FStringPropertyTrackEditor"));
	return MakeShareable(new FStringPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FStringPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FString>& NewGeneratedKeys, TArray<FString>& DefaultGeneratedKeys )
{
	void* CurrentObject = PropertyChangedParams.ObjectsThatChanged[0];
	void* PropertyValue = nullptr;
	for (int32 i = 0; i < PropertyChangedParams.PropertyPath.GetNumProperties(); i++)
	{
		if (UProperty* Property = PropertyChangedParams.PropertyPath.GetPropertyInfo(i).Property.Get())
		{
			CurrentObject = Property->ContainerPtrToValuePtr<FString>(CurrentObject, 0);
		}
	}

	const UStrProperty* StrProperty = Cast<const UStrProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if ( StrProperty )
	{
		FString StrPropertyValue = StrProperty->GetPropertyValue(CurrentObject);
		NewGeneratedKeys.Add(StrPropertyValue);
	}
}
