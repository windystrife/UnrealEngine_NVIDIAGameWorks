// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/VectorPropertyTrackEditor.h"
#include "Sections/VectorPropertySection.h"
#include "MovieSceneVectorTrack.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "UnrealEdGlobals.h"
#include "Classes/Editor/UnrealEdEngine.h"


FName FVectorPropertyTrackEditor::XName( "X" );
FName FVectorPropertyTrackEditor::YName( "Y" );
FName FVectorPropertyTrackEditor::ZName( "Z" );
FName FVectorPropertyTrackEditor::WName( "W" );


TSharedRef<ISequencerTrackEditor> FVectorPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FVectorPropertyTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> FVectorPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FVectorTrackEditor"));
	return MakeShareable(new FVectorPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}

void FVectorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FVectorKey>& NewGeneratedKeys, TArray<FVectorKey>& DefaultGeneratedKeys )
{
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );

	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2D = StructName == NAME_Vector2D;
	bool bIsVector = StructName == NAME_Vector;
	bool bIsVector4 = StructName == NAME_Vector4;

	FVector4 VectorValues;
	int32 Channels;

	if ( bIsVector2D )
	{
		FVector2D Vector2DValue = PropertyChangedParams.GetPropertyValue<FVector2D>();
		VectorValues.X = Vector2DValue.X;
		VectorValues.Y = Vector2DValue.Y;
		Channels = 2;
	}
	else if ( bIsVector )
	{
		FVector Vector3DValue = PropertyChangedParams.GetPropertyValue<FVector>();
		VectorValues.X = Vector3DValue.X;
		VectorValues.Y = Vector3DValue.Y;
		VectorValues.Z = Vector3DValue.Z;
		Channels = 3;
	}
	else // if ( bIsVector4 )
	{
		VectorValues = PropertyChangedParams.GetPropertyValue<FVector4>();
		Channels = 4;
	}

	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;

	TArray<FVectorKey>& XKeys = ChannelName == NAME_None || ChannelName == XName ? NewGeneratedKeys : DefaultGeneratedKeys;
	XKeys.Add( FVectorKey( EKeyVectorChannel::X, VectorValues.X ) );

	TArray<FVectorKey>& YKeys = ChannelName == NAME_None || ChannelName == YName ? NewGeneratedKeys : DefaultGeneratedKeys;
	YKeys.Add( FVectorKey( EKeyVectorChannel::Y, VectorValues.Y ) );

	if ( Channels >= 3 )
	{
		TArray<FVectorKey>& ZKeys = ChannelName == NAME_None || ChannelName == ZName ? NewGeneratedKeys : DefaultGeneratedKeys;
		ZKeys.Add( FVectorKey( EKeyVectorChannel::Z, VectorValues.Z ) );
	}

	if ( Channels >= 4 )
	{
		TArray<FVectorKey>& WKeys = ChannelName == NAME_None || ChannelName == WName ? NewGeneratedKeys : DefaultGeneratedKeys;
		WKeys.Add( FVectorKey( EKeyVectorChannel::W, VectorValues.W ) );
	}
}

void FVectorPropertyTrackEditor::InitializeNewTrack( UMovieSceneVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams )
{
	FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>::InitializeNewTrack( NewTrack, PropertyChangedParams );
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	FName StructName = StructProp->Struct->GetFName();

	if ( StructName == NAME_Vector2D )
	{
		NewTrack->SetNumChannelsUsed( 2 );
	}
	if ( StructName == NAME_Vector )
	{
		NewTrack->SetNumChannelsUsed( 3 );
	}
	if ( StructName == NAME_Vector4 )
	{
		NewTrack->SetNumChannelsUsed( 4 );
	}
}

void CopyInterpVectorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackVectorProp* MatineeVectorTrack, UMovieSceneVectorTrack* VectorTrack)
{
	if (FMatineeImportTools::CopyInterpVectorTrack(MatineeVectorTrack, VectorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FVectorPropertyTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackVectorProp* MatineeVectorTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeVectorTrack = Cast<UInterpTrackVectorProp>( CopyPasteObject );
		if ( MatineeVectorTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneVectorTrack* VectorTrack = Cast<UMovieSceneVectorTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrack", "Paste Matinee Vector Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrackTooltip", "Pastes keys from a Matinee vector track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpVectorTrack, GetSequencer().ToSharedRef(), MatineeVectorTrack, VectorTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeVectorTrack != nullptr && MatineeVectorTrack->GetNumKeys() > 0 && VectorTrack != nullptr && VectorTrack->GetNumChannelsUsed() == 3; } ) ) );

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}

