// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencer.h"
#include "MovieSceneTrackEditor.h"

class AActor;
class FMenuBuilder;
class USceneComponent;

/**
 * Track editor actor picker
 */
class FActorPickerTrackEditor : public FMovieSceneTrackEditor
{
public:

	FActorPickerTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Is this actor pickable? */
	virtual bool IsActorPickable( const AActor* const ParentActor, FGuid ObjectBinding, UMovieSceneSection* InSection) { return false; }

	/** Actor socket was picked */
	virtual void ActorSocketPicked(const FName SocketName, USceneComponent* Component, AActor* ParentActor, FGuid ObjectBinding, UMovieSceneSection* Section) {}

	/** Show a sub menu of the pickable actors */
	void ShowActorSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneSection* Section);

	/** Actor was picked */
	void ActorPicked(AActor* ParentActor, FGuid ObjectBinding, UMovieSceneSection* Section);

	/** Actor component was picked */
	void ActorComponentPicked(FString ComponentName, AActor* ParentActor, FGuid ObjectBinding, UMovieSceneSection* Section);

private: 

	/** Interactively pick an actor from the viewport */
	void PickActorInteractive(FGuid ObjectBinding, UMovieSceneSection* Section);

};
