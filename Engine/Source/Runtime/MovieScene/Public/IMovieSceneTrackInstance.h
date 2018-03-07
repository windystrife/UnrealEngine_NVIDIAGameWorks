// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneAnimTypeID.h"

class FMovieSceneSequenceInstance;
class IMovieScenePlayer;

enum EMovieSceneUpdatePass
{
	MSUP_PreUpdate = 0x00000001,
	MSUP_Update = 0x00000002,
	MSUP_PostUpdate = 0x00000004,
	MSUP_All = MSUP_PreUpdate | MSUP_Update | MSUP_PostUpdate
};

struct EMovieSceneUpdateData
{
	float Position;
	float LastPosition;
	bool bPreroll;
	bool bJumpCut;
	/** Indicates that this update was caused by the owning movie scene stopping playback due
	    to the active sub-scene being deactivated. */
	bool bSubSceneDeactivate;
	bool bUpdateCameras;

	EMovieSceneUpdatePass UpdatePass;

	EMovieSceneUpdateData()
	{
		Position = 0.0f;
		LastPosition = 0.0f;
		bPreroll = false;
		bJumpCut = false;
		bSubSceneDeactivate = false;
		bUpdateCameras = true;
		UpdatePass = MSUP_PreUpdate;
	}
	EMovieSceneUpdateData(float InPosition, float InLastPosition)
	{
		Position = InPosition;
		LastPosition = InLastPosition;
		bPreroll = false;
		bJumpCut = false;
		bSubSceneDeactivate = false;
		bUpdateCameras = true;
		UpdatePass = MSUP_PreUpdate;
	}
};

/**
 * A track instance holds the live objects for a track.  
 */
class IMovieSceneTrackInstance
{
public:

	MOVIESCENE_API IMovieSceneTrackInstance();
	
	/** Virtual destructor. */
	virtual ~IMovieSceneTrackInstance() { }

	/**
	 * Save state of objects that this instance will be editing.
	 * @todo Sequencer: This is likely editor only
	 */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) = 0;

	/**
	 * Restore state of objects that this instance edited.
	 * @todo Sequencer: This is likely editor only
	 */
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) = 0;

	/**
	 * Main update function for track instances.  Called in game and in editor when we update a moviescene.
	 *
	 * @param UpdateData		The current and previous position of the moviescene that is playing. The update pass.
	 * @param RuntimeObjects	Runtime objects bound to this instance (if any)
	 * @param Player			The playback interface.  Contains state and some other functionality for runtime playback
	 */
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) = 0;
	
	/*
	 * Which update passes does this track instance evaluate in?
	 */
	virtual EMovieSceneUpdatePass HasUpdatePasses() { return MSUP_Update; }

	/**
	 * Whether or not this track instance needs to be updated when it's deactivated because it's in a sub-scene that's ending.
	 */
	virtual bool RequiresUpdateForSubSceneDeactivate() { return false; }

	/**
	 * Refreshes the current instance
	 */
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) = 0;

	/**
	 * Removes all instance data from this track instance
	 *
	 * Called before an instance is removed
	 */
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) = 0;

	/**
	* Evaluation order
	*/
	virtual float EvalOrder() { return 0.f; }

	const FMovieSceneAnimTypeID AnimTypeID;
};
