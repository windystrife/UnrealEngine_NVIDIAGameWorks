// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UMovieScene;
struct FMovieSceneTrackLabels;

class FSequencerLabelManager
{
public:

	/**
	 * Set the movie scene containing the labels to manage.
	 *
	 * @param InMovieScene The movie scene.
	 */
	void SetMovieScene(UMovieScene* InMovieScene);

public:

	/**
	 * Add a label to the specified object.
	 *
	 * @param ObjectId The unique identifier of the object.
	 * @param Label The label to add.
	 * @see RemoveObjectLabel
	 */
	void AddObjectLabel(const FGuid& ObjectId, const FString& Label);

	/**
	 * Get an object's track label.
	 *
	 * @param ObjectId The object to get the label for.
	 * @return The label string.
	 */
	const FMovieSceneTrackLabels* GetObjectLabels(const FGuid& ObjectId) const;

	/**
	 * Remove a label from the specified object.
	 *
	 * @param ObjectId The unique identifier of the object (or invalid GUID to remove from all objects).
	 * @param Label The label to remove.
	 * @see AddObjectLabel
	 */
	void RemoveObjectLabel(const FGuid& ObjectId, const FString& Label);

public:

	/**
	 * Get all known track labels.
	 *
	 * @param OutLabels Will contain the collection of known labels.
	 * @return The number labels returned.
	 */
	int32 GetAllLabels(TArray<FString>& OutLabels) const;

	/**
	 * Check whether the specified track label exists.
	 *
	 * @param Label The label to check.
	 * @return true if the label exists, false otherwise.
	 */
	bool LabelExists(const FString& Label) const;

	/**
	 * Rename the specified label to the given string.
	 *
	 * @param OldLabel The label to rename.
	 * @param NewLabel The new label string.
	 * @return true if at least one label was renamed, false otherwise.
	 */
	bool RenameLabel(const FString& OldLabel, const FString& NewLabel);

public:

	/** Get an event delegate that is executed when the movie scene's labels changed. */
	DECLARE_EVENT(FSequencerLabelManager, FOnLabelsChanged)
	FOnLabelsChanged& OnLabelsChanged()
	{
		return LabelsChangedEvent;
	}

private:

	/** An event delegate that is executed when the movie scene's labels changed. */
	FOnLabelsChanged LabelsChangedEvent;

	/** The movie scene containing the labels. */
	TWeakObjectPtr<UMovieScene> MovieScene;
};
