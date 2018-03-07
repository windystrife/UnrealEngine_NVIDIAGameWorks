// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Containers/ArrayView.h"
#include "UObject/WeakObjectPtr.h"

class IMovieScenePlayer;
class UMovieSceneSequence;

/**
 * Object cache that looks up, resolves, and caches object bindings for a specific sequence
 */
struct FMovieSceneObjectCache
{
	/**
	 * Find all objects that are bound to the specified binding ID
	 * @note Will look up, and cache any objects if the cache has been invalidated
	 *
	 * @param InBindingID		The object binding GUID for a spawnable or posessable in a UMovieScene
	 * @param Player			The movie scene player that is playing back the sequence
	 * @return An iterable type of all objects bound to the specified ID.
	 */
	MOVIESCENE_API TArrayView<TWeakObjectPtr<>> FindBoundObjects(const FGuid& InBindingID, IMovieScenePlayer& Player);

	/**
	 * Set the sequence that this cache applies to
	 *
	 * @param InSequence		The sequence that this cache applies to
	 * @param InSequenceID		The ID of the sequence within the master sequence
	 */
	MOVIESCENE_API void SetSequence(UMovieSceneSequence& InSequence, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player);

	/**
	 * Attempt deduce the posessable or spawnable that relates to the specified object
	 * @note Will forcably resolve any out of date bindings in the entire sequence
	 *
	 * @param InObject			The object whose binding ID is to be find
	 * @param Player			The movie scene player that is playing back the sequence
	 * @return The object's spawnable or possessable GUID, or a zero GUID if it was not found
	 */
	MOVIESCENE_API FGuid FindObjectId(UObject& InObject, IMovieScenePlayer& Player);

	/**
	 * Invalidate any object bindings for objects that have been destroyed
	 */
	MOVIESCENE_API void InvalidateExpiredObjects();

	/**
	 * Invalidate the object bindings for a specific object binding ID
	 *
	 * @param InGuid			The object binding ID to invalidate bindings for
	 */
	MOVIESCENE_API void Invalidate(const FGuid& InGuid);

	/**
	 * Completely erase all knowledge of, anc caches for all object bindings
	 */
	void Clear(IMovieScenePlayer& Player);

	/**
	 * Get the sequence that this cache relates to
	 */
	UMovieSceneSequence* GetSequence() const { return WeakSequence.Get(); }

private:
	/**
	 * Update the bindings for the specified GUID
	 *
	 * @param InGuid			The object binding ID to update bindings for
	 * @param Player			The movie scene player that is playing back the sequence
	 */
	void UpdateBindings(const FGuid& InGuid, IMovieScenePlayer& Player);

	struct FBoundObjects
	{
		bool bUpToDate;
		TArray<TWeakObjectPtr<>, TInlineAllocator<1>> Objects;
	};

private:

	/** The sequence that we're caching objects for */
	TWeakObjectPtr<UMovieSceneSequence> WeakSequence;

	/** The sequence ID of the sequence within the master sequence */
	FMovieSceneSequenceID SequenceID;

	template<typename ValueType>
	struct TFastGuidKeyFuncs : BaseKeyFuncs<TPair<FGuid,ValueType>,FGuid,false>
	{
		typedef typename TTypeTraits<FGuid>::ConstPointerType KeyInitType;
		typedef const TPairInitializer<typename TTypeTraits<FGuid>::ConstInitType, typename TTypeTraits<ValueType>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A,KeyInitType B)
		{
			return A == B;
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.A ^ Key.B ^ Key.C ^ Key.D;
		}
	};

	/** A map of bound objects */
	TMap<FGuid, FBoundObjects, FDefaultSetAllocator, TFastGuidKeyFuncs<FBoundObjects>> BoundObjects;

	/** Map of child bindings for any given object binding */
	typedef TArray<FGuid, TInlineAllocator<4>> FGuidArray;
	TMap<FGuid, FGuidArray, FDefaultSetAllocator, TFastGuidKeyFuncs<FGuidArray>> ChildBindings;
};

/**
 * Provides runtime evaluation functions with the ability to look up state from the main game environment
 */
struct FMovieSceneEvaluationState
{
	/**
	 * Assign a sequence to a specific ID
	 *
	 * @param InSequenceID		The sequence ID to assign to
	 * @param InSequence		The sequence to assign
	 */
	void AssignSequence(FMovieSceneSequenceIDRef InSequenceID, UMovieSceneSequence& InSequence, IMovieScenePlayer& Player);

	/**
	 * Attempt to locate a sequence from its ID
	 *
	 * @param InSequenceID		The sequence ID to lookup
	 */
	MOVIESCENE_API UMovieSceneSequence* FindSequence(FMovieSceneSequenceIDRef InSequenceID) const;

	/**
	 * Attempt deduce the posessable or spawnable that relates to the specified object
	 * @note Will forcably resolve any out of date bindings in the entire sequence
	 *
	 * @param InObject			The object whose binding ID is to be find
	 * @param Player			The movie scene player that is playing back the sequence
	 * @return The object's spawnable or possessable GUID, or a zero GUID if it was not found
	 */
	MOVIESCENE_API FGuid FindObjectId(UObject& Object, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player);

	/**
	 * Find an object cache pertaining to the specified sequence
	 *
	 * @param InSequenceID		The sequence ID to lookup
	 */
	FORCEINLINE FMovieSceneObjectCache* FindObjectCache(FMovieSceneSequenceIDRef SequenceID)
	{
		return ObjectCaches.Find(SequenceID);
	}

	/**
	 * Get an object cache pertaining to the specified sequence
	 *
	 * @param InSequenceID		The sequence ID to lookup
	 */
	FORCEINLINE FMovieSceneObjectCache& GetObjectCache(FMovieSceneSequenceIDRef SequenceID)
	{
		FMovieSceneObjectCache* Cache = ObjectCaches.Find(SequenceID);
		if (!Cache)
		{
			Cache = &ObjectCaches.Add(SequenceID, FMovieSceneObjectCache());
		}
		return *Cache;
	}

	/**
	 * Remove the object cache pertaining to the specified sequence
	 *
	 * @param InSequenceID		The sequence ID to remove
	 */
	FORCEINLINE void RemoveObjectCache(FMovieSceneSequenceIDRef SequenceID)
	{
		ObjectCaches.Remove(SequenceID);
	}

	/**
	 * Invalidate any object caches that may now contain expired objects
	 */
	MOVIESCENE_API void InvalidateExpiredObjects();

	/**
	 * Forcably invalidate the specified object binding in the specified sequence
	 *
	 * @param InGuid			The object binding ID to invalidate
	 * @param InSequenceID		The sequence ID to which the object binding belongs
	 */
	MOVIESCENE_API void Invalidate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID);

	/**
	 * Forcably clear all object caches
	 */
	MOVIESCENE_API void ClearObjectCaches(IMovieScenePlayer& Player);

	/** A map of persistent evaluation data mapped by movie scene evaluation entity (i.e, a given track or section) */
	TMap<FMovieSceneEvaluationKey, TUniquePtr<IPersistentEvaluationData>> PersistentEntityData;

	/** A map of persistent evaluation data mapped by shared evaluation key. Such data can be accessed from anywhere given an operand and a unique identifier. */
	TMap<FSharedPersistentDataKey, TUniquePtr<IPersistentEvaluationData>> PersistentSharedData;

private:

	/** Maps of bound objects, arranged by template ID */
	TMap<FMovieSceneSequenceID, FMovieSceneObjectCache> ObjectCaches;
};
