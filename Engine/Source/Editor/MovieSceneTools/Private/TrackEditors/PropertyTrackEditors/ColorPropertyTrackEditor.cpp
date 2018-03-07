// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ColorPropertyTrackEditor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Sections/ColorPropertySection.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorProp.h"


FName FColorPropertyTrackEditor::RedName( "R" );
FName FColorPropertyTrackEditor::GreenName( "G" );
FName FColorPropertyTrackEditor::BlueName( "B" );
FName FColorPropertyTrackEditor::AlphaName( "A" );
FName FColorPropertyTrackEditor::SpecifiedColorName( "SpecifiedColor" );

TSharedRef<ISequencerTrackEditor> FColorPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FColorPropertyTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FColorPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FColorPropertyTrackEditor"));
	return MakeShareable(new FColorPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FColorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FColorKey>& NewGeneratedKeys, TArray<FColorKey>& DefaultGeneratedKeys )
{
	UProperty* Property = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
	if (!Property)
	{
		return;
	}

	const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
	FName StructName = StructProp->Struct->GetFName();
	FName PropertyName = Property->GetFName();

	bool bIsFColor = StructName == NAME_Color;
	bool bIsFLinearColor = StructName == NAME_LinearColor;
	bool bIsSlateColor = StructName == FName( "SlateColor" );

	FLinearColor ColorValue;

	if (bIsFColor)
	{
		ColorValue = FLinearColor( PropertyChangedParams.GetPropertyValue<FColor>() );
	}
	else
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FLinearColor>();
	}

	if( StructProp->HasMetaData("HideAlphaChannel") )
	{
		ColorValue.A = 1;
	}

	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;

	TArray<FColorKey>& RedKeys = ChannelName == NAME_None || ChannelName == RedName || ChannelName == SpecifiedColorName ? NewGeneratedKeys : DefaultGeneratedKeys;
	RedKeys.Add( FColorKey( EKeyColorChannel::Red, ColorValue.R, bIsSlateColor ) );

	TArray<FColorKey>& GreenKeys = ChannelName == NAME_None || ChannelName == GreenName || ChannelName == SpecifiedColorName ? NewGeneratedKeys : DefaultGeneratedKeys;
	GreenKeys.Add( FColorKey( EKeyColorChannel::Green, ColorValue.G, bIsSlateColor ) );

	TArray<FColorKey>& BlueKeys =  ChannelName == NAME_None || ChannelName == BlueName || ChannelName == SpecifiedColorName ? NewGeneratedKeys : DefaultGeneratedKeys;
	BlueKeys.Add( FColorKey( EKeyColorChannel::Blue, ColorValue.B, bIsSlateColor ) );

	TArray<FColorKey>& AlphaKeys = ChannelName == NAME_None || ChannelName == AlphaName || ChannelName == SpecifiedColorName ? NewGeneratedKeys : DefaultGeneratedKeys;
	AlphaKeys.Add( FColorKey( EKeyColorChannel::Alpha, ColorValue.A, bIsSlateColor ) );
}

void CopyInterpColorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackColorProp* ColorPropTrack, UMovieSceneColorTrack* ColorTrack)
{
	if (FMatineeImportTools::CopyInterpColorTrack(ColorPropTrack, ColorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void CopyInterpLinearColorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackLinearColorProp* LinearColorPropTrack, UMovieSceneColorTrack* ColorTrack)
{
	if (FMatineeImportTools::CopyInterpLinearColorTrack(LinearColorPropTrack, ColorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FColorPropertyTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackColorProp* ColorPropTrack = nullptr;
	UInterpTrackLinearColorProp* LinearColorPropTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		ColorPropTrack = Cast<UInterpTrackColorProp>( CopyPasteObject );
		LinearColorPropTrack = Cast<UInterpTrackLinearColorProp>( CopyPasteObject );
		if ( ColorPropTrack != nullptr || LinearColorPropTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeColorTrack", "Paste Matinee Color Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeColorTrackTooltip", "Pastes keys from a Matinee color track into this track." ),
		FSlateIcon(),
		FUIAction(
			ColorPropTrack != nullptr ? 
			FExecuteAction::CreateStatic( &CopyInterpColorTrack, GetSequencer().ToSharedRef(), ColorPropTrack, ColorTrack ) : 
			FExecuteAction::CreateStatic( &CopyInterpLinearColorTrack, GetSequencer().ToSharedRef(), LinearColorPropTrack, ColorTrack ),			
			FCanExecuteAction::CreateLambda( [=]()->bool { return ((ColorPropTrack != nullptr && ColorPropTrack->GetNumKeys() > 0) || (LinearColorPropTrack != nullptr && LinearColorPropTrack->GetNumKeys() > 0)) && ColorTrack != nullptr; } ) ) );

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}

