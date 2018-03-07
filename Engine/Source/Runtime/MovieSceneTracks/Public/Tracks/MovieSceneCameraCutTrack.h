// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneCameraCutTrack.generated.h"

/**
 * Handles manipulation of CameraCut properties in a movie scene.
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraCutTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_BODY()
	UMovieSceneCameraCutTrack( const FObjectInitializer& ObjectInitializer );
	
public:

	/** 
	 * Adds a new CameraCut at the specified time.
	 *	
	 * @param CameraHandle Handle to the camera that the CameraCut switches to when active.
	 * @param TimeRange The range within this track's movie scene where the CameraCut is initially placed.
	 */
	MOVIESCENETRACKS_API void AddNewCameraCut(FGuid CameraHandle, float Time);

public:

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

#if WITH_EDITOR
	virtual void OnSectionMoved(UMovieSceneSection& Section) override;
#endif

protected:

	float FindEndTimeForCameraCut(float StartTime);

private:

	/** All movie scene sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
