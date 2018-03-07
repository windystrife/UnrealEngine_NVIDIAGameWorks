// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpawnTrackEditor.h"

/**
* A property track editor for bindings for animation controllers
*/
class FControlRigBindingTrackEditor : public FSpawnTrackEditor
{
public:

	/**
	* Factory function to create an instance of this class (called by a sequencer).
	*
	* @param InSequencer The sequencer instance to be used by this tool.
	* @return The new instance of this class.
	*/
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	/**
	* Creates and initializes a new instance.
	*
	* @param InSequencer The sequencer instance to be used by this tool.
	*/
	FControlRigBindingTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override { return TSharedPtr<SWidget>(); }
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;

private:

	/** Callback for executing the "Binding Track" menu entry. */
	void HandleAddBindingTrackMenuEntryExecute(FGuid ObjectBinding);
	bool CanAddBindingTrack(FGuid ObjectBinding) const;
};
