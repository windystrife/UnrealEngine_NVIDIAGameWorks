// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "Evaluation/MovieScenePreAnimatedState.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSpawnRegister.h"
#include "Containers/ArrayView.h"
#include "Evaluation/MovieSceneEvaluationState.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"


class UMovieSceneSequence;
class FViewportClient;
class IMovieSceneBindingOverridesInterface;
struct FMovieSceneRootEvaluationTemplateInstance;
class FMovieSceneSequenceInstance;

struct EMovieSceneViewportParams
{
	EMovieSceneViewportParams()
	{
		FadeAmount = 0.f;
		FadeColor = FLinearColor::Black;
		bEnableColorScaling = false;
	}

	enum SetViewportParam
	{
		SVP_FadeAmount   = 0x00000001,
		SVP_FadeColor    = 0x00000002,
		SVP_ColorScaling = 0x00000004,
		SVP_All          = SVP_FadeAmount | SVP_FadeColor | SVP_ColorScaling
	};

	SetViewportParam SetWhichViewportParam;

	float FadeAmount;
	FLinearColor FadeColor;
	FVector ColorScale; 
	bool bEnableColorScaling;
};

/**
 * Interface for movie scene players
 * Provides information for playback of a movie scene
 */
class IMovieScenePlayer
{
public:
	virtual ~IMovieScenePlayer() { }

	/**
	 * Access the evaluation template that we are playing back
	 */
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() = 0;

	/**
	 * Updates the perspective viewports with the actor to view through
	 *
	 * @param CameraObject The object, probably a camera, that the viewports should lock to
	 * @param UnlockIfCameraObject If this is not nullptr, release actor lock only if currently locked to this object.
	 * @param bJumpCut Whether this is a jump cut, ie. the cut jumps from one shot to another shot
	 */
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr, bool bJumpCut = false) = 0;

	/*
	 * Set the perspective viewport settings
	 *
	 * @param ViewportParamMap A map from the viewport client to its settings
	 */
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) = 0;

	/*
	 * Get the current perspective viewport settings
	 *
	 * @param ViewportParamMap A map from the viewport client to its settings
	 */
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const = 0;

	/** @return whether the player is currently playing, scrubbing, etc. */
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const = 0;

	/** 
	* @param PlaybackStatus The playback status to set
	*/
	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) = 0;

	/**
	 * Resolve objects bound to the specified binding ID
	 *
	 * @param InBindingId	The ID relating to the object(s) to resolve
	 * @param OutObjects	Container to populate with the bound objects
	 */
	MOVIESCENE_API virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const;

	/**
	 * Access the binding overrides interface for this player.
	 *
	 * @return A pointer to the binding overrides interface, or nullptr if one is not available
	 */
	virtual const IMovieSceneBindingOverridesInterface* GetBindingOverrides() const { return nullptr; }

	/**
	 * Obtain an object responsible for managing movie scene spawnables
	 */
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() { return NullRegister; }

	/*
	 * Called wehn an object is spawned by sequencer
	 * 
	 */
	virtual void OnObjectSpawned(UObject* InObject, const FMovieSceneEvaluationOperand& Operand) {}

	/**
	 * Called whenever an object binding has been resolved to give the player a chance to interact with the objects before they are animated
	 * 
	 * @param InGuid		The guid of the object binding that has been resolved
	 * @param InSequenceID	The ID of the sequence in which the object binding resides
	 * @param Objects		The array of objects that were resolved
	 */
	virtual void NotifyBindingUpdate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID, TArrayView<TWeakObjectPtr<>> Objects) { NotifyBindingsChanged(); }

	/**
	 * Called whenever any object bindings have changed
	 */
	virtual void NotifyBindingsChanged() {}

	/**
	 * Access the playback context for this movie scene player
	 */
	virtual UObject* GetPlaybackContext() const { return nullptr; }

	/**
	 * Access the event contexts for this movie scene player
	 */
	virtual TArray<UObject*> GetEventContexts() const { return TArray<UObject*>(); }

	/**
	 * Test whether this is a preview player or not. As such, playback range becomes insignificant for things like spawnables
	 */
	virtual bool IsPreview() const { return false; }

public:

	/**
	 * Locate objects bound to the specified object guid, in the specified sequence
	 * @note: Objects lists are cached internally until they are invalidate.
	 *
	 * @param ObjectBindingID 		The object to resolve
	 * @param SequenceID 			ID of the sequence to resolve for
	 *
	 * @return Iterable list of weak object pointers pertaining to the specified GUID
	 */
	TArrayView<TWeakObjectPtr<>> FindBoundObjects(const FGuid& ObjectBindingID, FMovieSceneSequenceIDRef SequenceID)
	{
		FMovieSceneObjectCache* Cache = State.FindObjectCache(SequenceID);
		if (Cache)
		{
			return Cache->FindBoundObjects(ObjectBindingID, *this);
		}

		return TArrayView<TWeakObjectPtr<>>();
	}

	/**
	 * Locate objects bound to the specified sequence operand
	 * @note: Objects lists are cached internally until they are invalidate.
	 *
	 * @param Operand 			The movie scene operand to resolve
	 *
	 * @return Iterable list of weak object pointers pertaining to the specified GUID
	 */
	TArrayView<TWeakObjectPtr<>> FindBoundObjects(const FMovieSceneEvaluationOperand& Operand)
	{
		return FindBoundObjects(Operand.ObjectBindingID, Operand.SequenceID);
	}

	/**
	 * Attempt to find the object binding ID for the specified object, in the specified sequence
	 * @note: Will forcably resolve all out-of-date object mappings in the sequence
	 *
	 * @param InObject 			The object to find a GUID for
	 * @param SequenceID 		The sequence ID to search within
	 *
	 * @return The guid of the object's binding, or zero guid if it was not found
	 */
	FGuid FindObjectId(UObject& InObject, FMovieSceneSequenceIDRef SequenceID)
	{
		return State.FindObjectId(InObject, SequenceID, *this);
	}

	/**
	 * Attempt to save specific state for the specified token state before it animates an object.
	 * @note: Will only call IMovieSceneExecutionToken::CacheExistingState if no state has been previously cached for the specified token type
	 *
	 * @param InObject			The object to cache state for
	 * @param InTokenType		Unique marker that identifies the originating token type
	 * @param InProducer		Producer implementation that defines how to create the preanimated token, if it doesn't already exist
	 */
	FORCEINLINE void SavePreAnimatedState(UObject& InObject, FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& InProducer)
	{
		PreAnimatedState.SavePreAnimatedState(InTokenType, InProducer, InObject);
	}

	/**
	 * Attempt to save specific state for the specified token state before it mutates state.
	 * @note: Will only call IMovieSceneExecutionToken::CacheExistingState if no state has been previously cached for the specified token type
	 *
	 * @param InTokenType		Unique marker that identifies the originating token type
	 * @param InProducer		Producer implementation that defines how to create the preanimated token, if it doesn't already exist
	 */
	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& InProducer)
	{
		PreAnimatedState.SavePreAnimatedState(InTokenType, InProducer);
	}

	/**
	 * Attempt to save specific state for the specified token state before it animates an object.
	 * @note: Will only call IMovieSceneExecutionToken::CacheExistingState if no state has been previously cached for the specified token type
	 *
	 * @param InObject			The object to cache state for
	 * @param InTokenType		Unique marker that identifies the originating token type
	 * @param InProducer		Producer implementation that defines how to create the preanimated token, if it doesn't already exist
	 * @param CaptureEntity		The entity key to associate this animated state with
	 */
	FORCEINLINE void SavePreAnimatedState(UObject& InObject, FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& InProducer, FMovieSceneEvaluationKey CaptureEntity)
	{
		PreAnimatedState.SavePreAnimatedState(InTokenType, InProducer, InObject, ECapturePreAnimatedState::Entity, CaptureEntity);
	}

	/**
	 * Attempt to save specific state for the specified token state before it mutates state.
	 * @note: Will only call IMovieSceneExecutionToken::CacheExistingState if no state has been previously cached for the specified token type
	 *
	 * @param InTokenType		Unique marker that identifies the originating token type
	 * @param InProducer		Producer implementation that defines how to create the preanimated token, if it doesn't already exist
	 * @param CaptureEntity		The entity key to associate this animated state with
	 */
	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& InProducer, FMovieSceneEvaluationKey CaptureEntity)
	{
		PreAnimatedState.SavePreAnimatedState(InTokenType, InProducer, ECapturePreAnimatedState::Entity, CaptureEntity);
	}

	/**
	 * Restore all pre-animated state
	 */
	void RestorePreAnimatedState()
	{
		PreAnimatedState.RestorePreAnimatedState(*this);
		State.ClearObjectCaches(*this);
	}

	/**
	 * Restore any pre-animated state that has been cached for the specified object
	 *
	 * @param Object			The object to restore
	 */
	void RestorePreAnimatedState(UObject& Object)
	{
		PreAnimatedState.RestorePreAnimatedState(*this, Object);
	}

	/**
	 * Restore any pre-animated state that has been cached for the specified object
	 *
	 * @param Object			The object to restore
	 * @param InFilter			Filter that defines whether specific anim types should be restored
	 */
	void RestorePreAnimatedState(UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
	{
		PreAnimatedState.RestorePreAnimatedState(*this, Object, InFilter);
	}

	/**
	 * Restore any pre-animated state that has been cached from the specified entity (a section or, less commonly, a track)
	 *
	 * @param EntityKey			The key to the entity that we want to restore state for (typically retrieved from PersistentData.GetSectionKey())
	 */
	FORCEINLINE void RestorePreAnimatedState(const FMovieSceneEvaluationKey& EntityKey)
	{
		PreAnimatedState.RestorePreAnimatedState(*this, EntityKey);
	}

public:

	/** Evaluation state that stores global state to do with the playback operation */
	FMovieSceneEvaluationState State;

	/** Container that stores any per-animated state tokens  */
	FMovieScenePreAnimatedState PreAnimatedState;

public:

	DEPRECATED(4.15, "Sequence Instances have been deprecated in favor of a template approach (see GetEvaluationTemplate).")
	MOVIESCENE_API TSharedRef<FMovieSceneSequenceInstance> GetRootMovieSceneSequenceInstance();

	DEPRECATED(4.15, "Please use FindBoundObjects directly.")
	MOVIESCENE_API void GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& Guid, TArray<TWeakObjectPtr<>>& OutRuntimeObjects);
	
private:

	/** Null register that asserts on use */
	FNullMovieSceneSpawnRegister NullRegister;
};
