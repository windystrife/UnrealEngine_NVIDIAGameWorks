// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerLabelManager.h"
#include "MovieScene.h"


void FSequencerLabelManager::SetMovieScene(UMovieScene* InMovieScene)
{
	if (MovieScene != InMovieScene)
	{
		MovieScene = InMovieScene;
		LabelsChangedEvent.Broadcast();
	}
}


void FSequencerLabelManager::AddObjectLabel(const FGuid& ObjectId, const FString& Label)
{
	if (MovieScene.IsValid())
	{
		MovieScene->GetObjectsToLabels().FindOrAdd(ObjectId.ToString()).Strings.AddUnique(Label);
		MovieScene->MarkPackageDirty();
		LabelsChangedEvent.Broadcast();
	}
}

/**
	* Get an object's track label.
	*
	* @param ObjectId The object to get the label for.
	* @return The label string.
	*/
const FMovieSceneTrackLabels* FSequencerLabelManager::GetObjectLabels(const FGuid& ObjectId) const
{
	if (!MovieScene.IsValid())
	{
		return nullptr;
	}

	return &MovieScene->GetObjectsToLabels().FindOrAdd(ObjectId.ToString());
}


void FSequencerLabelManager::RemoveObjectLabel(const FGuid& ObjectId, const FString& Label)
{
	if (MovieScene.IsValid())
	{
		TMap<FString, FMovieSceneTrackLabels>& ObjectsToLabels = MovieScene->GetObjectsToLabels();

		if (ObjectId.IsValid())
		{
			ObjectsToLabels.FindOrAdd(ObjectId.ToString()).Strings.Remove(Label);

			// Add to dummy object id so that this label will stick around until intentionally removed by user
			ObjectsToLabels.FindOrAdd(TEXT("UnusedLabelsDummy")).Strings.Add(Label);
		}
		else
		{
			for (auto& LabelsPair : ObjectsToLabels)
			{
				LabelsPair.Value.Strings.Remove(Label);
			}
		}
		
		MovieScene->MarkPackageDirty();
		LabelsChangedEvent.Broadcast();
	}
}


int32 FSequencerLabelManager::GetAllLabels(TArray<FString>& OutLabels) const
{
	if (!MovieScene.IsValid())
	{
		return 0;
	}

	for (const auto& LabelsPair : MovieScene->GetObjectsToLabels())
	{
		for (const auto& Label : LabelsPair.Value.Strings)
		{
			OutLabels.AddUnique(Label);
		}
	}

	return OutLabels.Num();
}


bool FSequencerLabelManager::LabelExists(const FString& Label) const
{
	if (!MovieScene.IsValid())
	{
		return false;
	}

	for (const auto& LabelsPair : MovieScene->GetObjectsToLabels())
	{
		if (LabelsPair.Value.Strings.Contains(Label))
		{
			return true;
		}
	}

	return false;
}


bool FSequencerLabelManager::RenameLabel(const FString& OldLabel, const FString& NewLabel)
{
	if (!MovieScene.IsValid() || (OldLabel == NewLabel))
	{
		return false;
	}

	bool FoundOldLabel = false;

	for (auto& LabelsPair : MovieScene->GetObjectsToLabels())
	{
		TArray<FString>& Strings = LabelsPair.Value.Strings;
		int32 OldLabelIndex = Strings.IndexOfByKey(OldLabel);

		if (OldLabelIndex > INDEX_NONE)
		{
			Strings[OldLabelIndex] = NewLabel;
			FoundOldLabel = true;
		}
	}

	return FoundOldLabel;
}
