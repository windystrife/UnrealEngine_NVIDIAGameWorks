// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSequenceHierarchy.h"

FMovieSceneObjectBindingID FMovieSceneObjectBindingID::ResolveLocalToRoot(FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy& Hierarchy) const
{
	FMovieSceneSequenceID NewSequenceID = FMovieSceneSequenceID(uint32(SequenceID));

	if (Space == EMovieSceneObjectBindingSpace::Local && LocalSequenceID != MovieSceneSequenceID::Root)
	{
		while (LocalSequenceID != MovieSceneSequenceID::Root)
		{
			const FMovieSceneSequenceHierarchyNode* CurrentNode = Hierarchy.FindNode(LocalSequenceID);
			const FMovieSceneSubSequenceData* SubData = Hierarchy.FindSubData(LocalSequenceID);
			if (!ensureAlwaysMsgf(CurrentNode && SubData, TEXT("Malformed sequence hierarchy")))
			{
				return FMovieSceneObjectBindingID(Guid, NewSequenceID);
			}

			NewSequenceID = NewSequenceID.AccumulateParentID(SubData->DeterministicSequenceID);
			LocalSequenceID = CurrentNode->ParentID;
		}
	}

	return FMovieSceneObjectBindingID(Guid, NewSequenceID);
}