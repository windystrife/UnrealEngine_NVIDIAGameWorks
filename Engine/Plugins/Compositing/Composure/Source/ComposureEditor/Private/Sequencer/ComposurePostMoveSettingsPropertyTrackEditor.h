// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyTrackEditor.h"
#include "MovieScene/MovieSceneComposurePostMoveSettingsTrack.h"
#include "MovieScene/MovieSceneComposurePostMoveSettingsSection.h"

/** A property track editor for FComposurePostMoveSettings properties. */
class FComposurePostMoveSettingsPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneComposurePostMoveSettingsTrack, UMovieSceneComposurePostMoveSettingsSection, FComposurePostMoveSettingsKey>
{
public:

	FComposurePostMoveSettingsPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneComposurePostMoveSettingsTrack, UMovieSceneComposurePostMoveSettingsSection, FComposurePostMoveSettingsKey>(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/**
	* Creates an instance of this class.  Called by a sequencer
	*
	* @param OwningSequencer The sequencer instance to be used by this tool
	* @return The new instance of this class
	*/
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	//~ ISequencerTrackEditor Interface

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

	//~ FMovieSceneTrackEditor interface

	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

	/**
	* Retrieve a list of all property types that this track editor animates
	*/
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromStructType("ComposurePostMoveSettings") });
	}

protected:

	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FComposurePostMoveSettingsKey>& NewGeneratedKeys, TArray<FComposurePostMoveSettingsKey>& DefaultGeneratedKeys) override;

private:

	/** Shows a dialog for importing post move animation from an external file. */
	void ShowImportPostMoveSettingsDialog(UMovieSceneComposurePostMoveSettingsTrack* PostMoveSettingsTrack);

	/** Imports post move settings from an external file to the specified track with the specified settings. */
	void ImportPostMoveSettings(FString ImportFilePath, float FrameInterval, int32 StartFrame, UMovieSceneComposurePostMoveSettingsTrack* PostMoveSettingsTrack);
	
	/** Handles closing the import settings dialog when the import it canceled. */
	void ImportCanceled();

private:
	TWeakPtr<SWindow> ImportDialog;
};