// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InterpTrackHelper.generated.h"

class AActor;
class UInterpGroup;
class UInterpTrack;

UCLASS()
class MATINEE_API UInterpTrackHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 * @param  Track	The track to get the actor for.
	 * @return Returns the actor for the group's track if one exists, NULL otherwise.
	 */
	 virtual AActor* GetGroupActor(const UInterpTrack* Track) const;

	/** Checks track-dependent criteria prior to adding a new track.
	 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
	 * Called on default object.
	 *
	 * @param Group The group that this track is being added to
	 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
	 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
	 * @param bAllowPrompts When true, we'll prompt for more information from the user with a dialog box if we need to
	 * @return Returns true if this track can be created and false if some criteria is not met (i.e. A named property is already controlled for this group).
	 */
	virtual	bool PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const { return true; }

	/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
	 * @param Track				Pointer to the track that was just created.
	 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
	 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
	 */
	virtual void  PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const { }

	/** Checks track-dependent criteria prior to adding a new keyframe.
	 * Responsible for any message-boxes or dialogs for selecting key-specific parameters.
	 * Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
	 *
	 * @param Track		Pointer to the currently selected track.
	 * @param KeyTime	The time that this Key becomes active.
	 * @return	Returns true if this key can be created and false if some criteria is not met (i.e. No related item selected in browser).
	 */
	virtual	bool PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const { return true; }

	/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
	 *
	 * @param Track		Pointer to the currently selected track.
	 * @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
	 */
	virtual void  PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const { }

	/** Returns the name of the new keyframe that has been added, valid while in the process of a rename operation */
	FName GetKeyframeAddDataName() const { return KeyframeAddDataName; }

protected:

	static FName KeyframeAddDataName;
};
