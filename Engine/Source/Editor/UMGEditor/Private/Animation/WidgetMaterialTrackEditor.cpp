// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/WidgetMaterialTrackEditor.h"
#include "Components/Widget.h"
#include "Animation/MovieSceneWidgetMaterialTrack.h"
#include "Animation/WidgetMaterialTrackUtilities.h"


FWidgetMaterialTrackEditor::FWidgetMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMaterialTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerTrackEditor> FWidgetMaterialTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable( new FWidgetMaterialTrackEditor( OwningSequencer ) );
}


bool FWidgetMaterialTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneWidgetMaterialTrack::StaticClass();
}


UMaterialInterface* FWidgetMaterialTrackEditor::GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	for (TWeakObjectPtr<> WeakObjectPtr : GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding))
	{
		UWidget* Widget = Cast<UWidget>( WeakObjectPtr.Get() );
		UMovieSceneWidgetMaterialTrack* WidgetMaterialTrack = Cast<UMovieSceneWidgetMaterialTrack>( MaterialTrack );
		if ( Widget != nullptr && WidgetMaterialTrack != nullptr )
		{
			FWidgetMaterialHandle Handle = WidgetMaterialTrackUtilities::GetMaterialHandle( Widget, WidgetMaterialTrack->GetBrushPropertyNamePath() );
			if (Handle.IsValid() )
			{
				return Handle.GetMaterial();
			}
		}
	}
	return nullptr;
}
