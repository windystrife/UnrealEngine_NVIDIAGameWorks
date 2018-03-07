// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/BytePropertyTrackEditor.h"
#include "Sections/BytePropertySection.h"
#include "UObject/EnumProperty.h"


TSharedRef<ISequencerTrackEditor> FBytePropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FBytePropertyTrackEditor(OwningSequencer));
}


TSharedRef<ISequencerSection> FBytePropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieSceneByteTrack* ByteTrack = Cast<UMovieSceneByteTrack>(&Track);
	checkf(ByteTrack != nullptr, TEXT("Incompatible track in FBoolPropertyTrackEditor"));
	return MakeShareable(new FBytePropertySection(GetSequencer().Get(), ObjectBinding, ByteTrack->GetPropertyName(), ByteTrack->GetPropertyPath(),
		SectionObject, Track.GetDisplayName(), ByteTrack->GetEnum()));
}


UEnum* GetEnumForByteTrack(TSharedPtr<ISequencer> Sequencer, const FGuid& OwnerObjectHandle, FName PropertyName, UMovieSceneByteTrack* ByteTrack)
{
	TSet<UEnum*> PropertyEnums;

	for (TWeakObjectPtr<> WeakObject : Sequencer->FindObjectsInCurrentSequence(OwnerObjectHandle))
	{
		UObject* RuntimeObject = WeakObject.Get();
		if (!RuntimeObject)
		{
			continue;
		}

		UProperty* Property = RuntimeObject->GetClass()->FindPropertyByName(PropertyName);
		if (Property != nullptr)
		{
			UEnum* Enum = nullptr;

			if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
			}
			else if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				Enum = ByteProperty->Enum;
			}

			if (Enum != nullptr)
			{
				PropertyEnums.Add(Enum);
			}
		}
	}

	UEnum* TrackEnum;

	if (PropertyEnums.Num() == 1)
	{
		TrackEnum = PropertyEnums.Array()[0];
	}
	else
	{
		TrackEnum = nullptr;
	}

	return TrackEnum;
}


UMovieSceneTrack* FBytePropertyTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* NewTrack = FPropertyTrackEditor::AddTrack(FocusedMovieScene, ObjectHandle, TrackClass, UniqueTypeName);
	UMovieSceneByteTrack* ByteTrack = Cast<UMovieSceneByteTrack>(NewTrack);
	UEnum* TrackEnum = GetEnumForByteTrack(GetSequencer(), ObjectHandle, UniqueTypeName, ByteTrack);

	if (TrackEnum != nullptr)
	{
		ByteTrack->SetEnum(TrackEnum);
	}

	return NewTrack;
}


void FBytePropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<uint8>& NewGeneratedKeys, TArray<uint8>& DefaultGeneratedKeys )
{
	NewGeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<uint8>() );
}
