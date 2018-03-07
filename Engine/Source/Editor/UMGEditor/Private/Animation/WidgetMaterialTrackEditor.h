// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerTrackEditor.h"
#include "TrackEditors/MaterialTrackEditor.h"

class UMaterialInterface;
class UMovieSceneMaterialTrack;

/**
 * A specialized material track editor for widget materials
 */
class FWidgetMaterialTrackEditor
	: public FMaterialTrackEditor
{
public:

	FWidgetMaterialTrackEditor( TSharedRef<ISequencer> InSequencer );

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;

protected:

	// FMaterialtrackEditor interface

	virtual UMaterialInterface* GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack ) override;
};
