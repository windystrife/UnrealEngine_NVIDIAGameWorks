// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class UMaterial;
class UMaterialInterface;
class UMovieSceneMaterialTrack;

/**
 * Track editor for material parameters.
 */
class MOVIESCENETOOLS_API FMaterialTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/** Constructor. */
	FMaterialTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FMaterialTrackEditor() { }

public:

	// ISequencerTrackEditor interface

	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget( const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params ) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;

protected:

	/** Gets a material interface for a specific object binding and material track. */
	virtual UMaterialInterface* GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack ) = 0;

private:

	/** Provides the contents of the add parameter menu. */
	TSharedRef<SWidget> OnGetAddParameterMenuContent( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack );

	/** Gets a material for a specific object binding and track */
	UMaterial* GetMaterialForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack );

	/** Adds a scalar parameter and initial key to a material track.
	 * @param ObjectBinding The object binding which owns the material track.
	 * @param MaterialTrack The track to Add the section to.
	 * @param ParameterName The name of the parameter to add an initial key for.
	 */
	void AddScalarParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FName ParameterName );

	/** Adds a color parameter and initial key to a material track.
	* @param ObjectBinding The object binding which owns the material track.
	* @param MaterialTrack The track to Add the section to.
	* @param ParameterName The name of the parameter to add an initial key for.
	*/
	void AddColorParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FName ParameterName );
};


/**
 * A specialized material track editor for component materials
 */
class FComponentMaterialTrackEditor
	: public FMaterialTrackEditor
{
public:

	FComponentMaterialTrackEditor( TSharedRef<ISequencer> InSequencer );

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;

protected:

	// FMaterialtrackEditor interface

	virtual UMaterialInterface* GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack ) override;
};
