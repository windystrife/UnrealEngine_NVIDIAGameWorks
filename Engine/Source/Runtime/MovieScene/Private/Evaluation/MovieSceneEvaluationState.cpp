// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationState.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneBindingOverridesInterface.h"

DECLARE_CYCLE_STAT(TEXT("Find Bound Objects"), MovieSceneEval_FindBoundObjects, STATGROUP_MovieSceneEval);

FMovieSceneSharedDataId FMovieSceneSharedDataId::Allocate()
{
	static uint32 Counter = 0;

	FMovieSceneSharedDataId Value;
	Value.UniqueId = ++Counter;
	check(Counter != -1);
	return Value;
}

TArrayView<TWeakObjectPtr<>> FMovieSceneObjectCache::FindBoundObjects(const FGuid& InBindingID, IMovieScenePlayer& Player)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_FindBoundObjects)
	
	// Fast route - where everything's cached
	FBoundObjects* Bindings = BoundObjects.Find(InBindingID);
	if (Bindings && Bindings->bUpToDate)
	{
		return TArrayView<TWeakObjectPtr<>>(
			Bindings->Objects.GetData(),
			Bindings->Objects.Num()
			);
	}

	// Attempt to update the bindings
	UpdateBindings(InBindingID, Player);

	Bindings = BoundObjects.Find(InBindingID);
	if (Bindings)
	{
		return TArrayView<TWeakObjectPtr<>>(Bindings->Objects.GetData(), Bindings->Objects.Num());
	}

	// Just return nothing
	return TArrayView<TWeakObjectPtr<>>();
}

FGuid FMovieSceneObjectCache::FindObjectId(UObject& InObject, IMovieScenePlayer& Player)
{
	UMovieSceneSequence* Sequence = WeakSequence.Get();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return FGuid();
	}

	// @todo: Currently we nuke the entire object cache when attempting to find an object's ID to ensure that we do a 
	// complete lookup from scratch. This is required for UMG as it interchanges content slots without notifying sequencer.
	Clear(Player);

	TWeakObjectPtr<> ObjectToFind(&InObject);

	// Search all possessables
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();
		if (FindBoundObjects(ThisGuid, Player).Contains(ObjectToFind))
		{
			return ThisGuid;
		}
	}

	// Search all spawnables
	for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetSpawnable(Index).GetGuid();
		if (FindBoundObjects(ThisGuid, Player).Contains(ObjectToFind))
		{
			return ThisGuid;
		}
	}

	return FGuid();
}

void FMovieSceneObjectCache::InvalidateExpiredObjects()
{
	for (auto& Pair : BoundObjects)
	{
		if (!Pair.Value.bUpToDate)
		{
			continue;
		}

		for (TWeakObjectPtr<>& Ptr : Pair.Value.Objects)
		{
			if (!Ptr.Get())
			{
				Invalidate(Pair.Key);
				break;
			}
		}
	}
}

void FMovieSceneObjectCache::Invalidate(const FGuid& InGuid)
{
	// Don't manipulate the actual map structure, since this can be called from inside an iterator
	FBoundObjects* Cache = BoundObjects.Find(InGuid);
	if (Cache)
	{
		Cache->bUpToDate = false;

		auto* Children = ChildBindings.Find(InGuid);
		if (Children)
		{
			for (const FGuid& Child : *Children)
			{
				Invalidate(Child);
			}
		}
	}
}

void FMovieSceneObjectCache::Clear(IMovieScenePlayer& Player)
{
	BoundObjects.Reset();
	ChildBindings.Reset();

	Player.NotifyBindingsChanged();
}


void FMovieSceneObjectCache::SetSequence(UMovieSceneSequence& InSequence, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player)
{
	if (WeakSequence != &InSequence)
	{
		Clear(Player);
	}

	WeakSequence = &InSequence;
	SequenceID = InSequenceID;
}

void FMovieSceneObjectCache::UpdateBindings(const FGuid& InGuid, IMovieScenePlayer& Player)
{
	FBoundObjects* Bindings = &BoundObjects.FindOrAdd(InGuid);
	Bindings->Objects.Reset();

	if (auto* Children = ChildBindings.Find(InGuid))
	{
		for (const FGuid& Child : *Children)
		{
			Invalidate(Child);
		}
	}
	ChildBindings.Remove(InGuid);

	// Find the sequence for the current
	UMovieSceneSequence* Sequence = WeakSequence.Get();
	if (!ensure(Sequence))
	{
		return;
	}

	const bool bUseParentsAsContext = Sequence->AreParentContextsSignificant();
	
	UObject* Context = Player.GetPlaybackContext();

	const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(InGuid);
	if (Possessable)
	{
		UObject* ResolutionContext = Context;

		// Because these are ordered parent-first, the parent must have already been bound, if it exists
		if (Possessable->GetParent().IsValid())
		{
			ChildBindings.FindOrAdd(Possessable->GetParent()).AddUnique(InGuid);

			TArrayView<TWeakObjectPtr<>> ParentBoundObjects = FindBoundObjects(Possessable->GetParent(), Player);

			// Refresh bindings in case of map changes
			Bindings = BoundObjects.Find(InGuid);
			for (TWeakObjectPtr<> Parent : ParentBoundObjects)
			{
				if (bUseParentsAsContext)
				{
					ResolutionContext = Parent.Get();
					if (!ResolutionContext)
					{
						continue;
					}
				}

				TArray<UObject*, TInlineAllocator<1>> FoundObjects;
				Player.ResolveBoundObjects(InGuid, SequenceID, *Sequence, ResolutionContext, FoundObjects);
				for (UObject* Object : FoundObjects)
				{
					Bindings->Objects.Add(Object);
				}
			}
		}
		else
		{
			TArray<UObject*, TInlineAllocator<1>> FoundObjects;
			Player.ResolveBoundObjects(InGuid, SequenceID, *Sequence, ResolutionContext, FoundObjects);
			for (UObject* Object : FoundObjects)
			{
				Bindings->Objects.Add(Object);
			}
		}
	}
	else
	{
		// Probably a spawnable then (or an phantom)
		bool bUseDefault = true;

		// Allow external overrides for spawnables
		const IMovieSceneBindingOverridesInterface* Overrides = Player.GetBindingOverrides();
		if (Overrides)
		{
			TArray<UObject*, TInlineAllocator<1>> FoundObjects;
			bUseDefault = Overrides->LocateBoundObjects(InGuid, SequenceID, FoundObjects);
			for (UObject* Object : FoundObjects)
			{
				Bindings->Objects.Add(Object);
			}
		}

		// If we have no overrides, or they want to allow the default spawnable, do that now
		if (bUseDefault)
		{
			UObject* SpawnedObject = Player.GetSpawnRegister().FindSpawnedObject(InGuid, SequenceID);
			if (SpawnedObject)
			{
				Bindings->Objects.Add(SpawnedObject);
			}
		}
	}

	if (Bindings->Objects.Num())
	{
		Bindings->bUpToDate = true;
		Player.NotifyBindingUpdate(InGuid, SequenceID, Bindings->Objects);
	}
}

void FMovieSceneEvaluationState::InvalidateExpiredObjects()
{
	for (auto& Pair : ObjectCaches)
	{
		Pair.Value.InvalidateExpiredObjects();
	}
}

void FMovieSceneEvaluationState::Invalidate(const FGuid& InGuid, FMovieSceneSequenceIDRef SequenceID)
{
	FMovieSceneObjectCache* Cache = ObjectCaches.Find(SequenceID);
	if (Cache)
	{
		Cache->Invalidate(InGuid);
	}
}

void FMovieSceneEvaluationState::ClearObjectCaches(IMovieScenePlayer& Player)
{
	for (auto& Pair : ObjectCaches)
	{
		Pair.Value.Clear(Player);
	}
}

void FMovieSceneEvaluationState::AssignSequence(FMovieSceneSequenceIDRef InSequenceID, UMovieSceneSequence& InSequence, IMovieScenePlayer& Player)
{
	GetObjectCache(InSequenceID).SetSequence(InSequence, InSequenceID, Player);
}

UMovieSceneSequence* FMovieSceneEvaluationState::FindSequence(FMovieSceneSequenceIDRef InSequenceID) const
{
	const FMovieSceneObjectCache* Cache = ObjectCaches.Find(InSequenceID);
	return Cache ? Cache->GetSequence() : nullptr;
}

FGuid FMovieSceneEvaluationState::FindObjectId(UObject& Object, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player)
{
	FMovieSceneObjectCache* Cache = ObjectCaches.Find(InSequenceID);
	return Cache ? Cache->FindObjectId(Object, Player) : FGuid();
}
