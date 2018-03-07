// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ActorReferencePropertyTrackEditor.h"
#include "GameFramework/Actor.h"
#include "Sections/ActorReferencePropertySection.h"


TSharedRef<ISequencerTrackEditor> FActorReferencePropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FActorReferencePropertyTrackEditor(OwningSequencer));
}


TSharedRef<ISequencerSection> FActorReferencePropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FActorReferencePropertyTrackEditor"));
	return MakeShareable(new FActorReferencePropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FActorReferencePropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FGuid>& NewGeneratedKeys, TArray<FGuid>& DefaultGeneratedKeys )
{

	AActor* NewReferencedActor = PropertyChangedParams.GetPropertyValue<AActor*>();
	if ( NewReferencedActor != nullptr )
	{
		FGuid ActorGuid = GetSequencer()->GetHandleToObject( NewReferencedActor );
		if ( ActorGuid.IsValid() )
		{
			NewGeneratedKeys.Add( ActorGuid );
		}
	}
}
