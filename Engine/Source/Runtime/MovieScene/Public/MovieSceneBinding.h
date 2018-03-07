// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneBinding.generated.h"

class UMovieSceneTrack;

/**
 * A set of tracks bound to runtime objects
 */
USTRUCT()
struct FMovieSceneBinding
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor. */
	FMovieSceneBinding() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InObjectGuid
	 * @param InBindingName
	 * @param InTracks
	 */
	FMovieSceneBinding(const FGuid& InObjectGuid, const FString& InBindingName, const TArray<UMovieSceneTrack*>& InTracks)
		: ObjectGuid(InObjectGuid)
		, BindingName(InBindingName)
		, Tracks(InTracks)
	{ }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InObjectGuid
	 * @param InBindingName
	 */
	FMovieSceneBinding(const FGuid& InObjectGuid, const FString& InBindingName)
		: ObjectGuid(InObjectGuid)
		, BindingName(InBindingName)
	{ }

	/**
	 * @return The time range of all tracks in this binding
	 */
	TRange<float> GetTimeRange() const;

	/**
	* Set the object guid
	*
	* @param InObjectGuid
	*/
	void SetObjectGuid(const FGuid& InObjectGuid)
	{
		ObjectGuid = InObjectGuid;
	}

	/**
	 * @return The guid of runtime objects in this binding
	 */
	const FGuid& GetObjectGuid() const
	{
		return ObjectGuid;
	}

	/**
	 * Set display name of the binding
	 */
	void SetName(const FString& InBindingName)
	{
		BindingName = InBindingName;
	} 

	/**
	 * @return The display name of the binding
	 */
	const FString& GetName() const
	{
		return BindingName;
	}

	/**
	 * Adds a new track to this binding
	 *
	 * @param NewTrack	The track to add
	 */
	void MOVIESCENE_API AddTrack(UMovieSceneTrack& NewTrack);

	/**
	 * Removes a track from this binding
	 * 
	 * @param Track	The track to remove
	 * @return true if the track was successfully removed, false if the track could not be found
	 */
	bool RemoveTrack(UMovieSceneTrack& Track);

	/**
	 * @return All tracks in this binding
	 */
	const TArray<UMovieSceneTrack*>& GetTracks() const
	{
		return Tracks;
	}

#if WITH_EDITOR
	/**
	 * Perform cook-time optimization on this object binding
	 * @param bShouldRemoveObject 		(Out) Boolean that will set to true if this whole binding should be considered redundant.
	 */
	void PerformCookOptimization(bool& bShouldRemoveObject);

#endif

private:

	/** Object binding guid for runtime objects */
	UPROPERTY()
	FGuid ObjectGuid;
	
	/** Display name */
	UPROPERTY()
	FString BindingName;

	/** All tracks in this binding */
	UPROPERTY(Instanced)
	TArray<UMovieSceneTrack*> Tracks;
};
