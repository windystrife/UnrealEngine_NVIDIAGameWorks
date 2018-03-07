// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "MovieSceneExecutionToken.h"
#include "MovieSceneSection.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "UObject/ObjectKey.h"

class IMovieScenePlayer;
class FMovieScenePreAnimatedState;

/** Enumeration that defines at what level to capture animating state for */
enum class ECapturePreAnimatedState : uint8
{
	/** Don't capture anything */
	None,
	/** Capture anything that animates, but only store such state globally across the sequence */
	Global,
	/** Capture anything for the current entity (track or section), such that it will be restored when that entity stops evaluating */
	Entity,
};

/** Structure that defines an entity key and animation type identifier combination */
struct FMovieSceneEntityAndAnimTypeID
{
	/** The entity that produced the aniamtion */
	FMovieSceneEvaluationKey EntityKey;
	/** The type of animation that was produced */
	FMovieSceneAnimTypeID AnimTypeID;

	friend bool operator==(const FMovieSceneEntityAndAnimTypeID& A, const FMovieSceneEntityAndAnimTypeID& B)
	{
		return A.AnimTypeID == B.AnimTypeID && A.EntityKey == B.EntityKey;
	}
};

/**
 * Internal structure that defines a pre animated token, and how many entities have referenced it.
 * Templated on token type in order to share logic between tokens that represent global state, and those that represent objects
 */
template<typename TokenType>
struct MOVIESCENE_API TPreAnimatedToken
{
	TPreAnimatedToken(TokenType&& InToken);

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	TPreAnimatedToken(TPreAnimatedToken&&) = default;
	TPreAnimatedToken& operator=(TPreAnimatedToken&&) = default;
#else
	TPreAnimatedToken(TPreAnimatedToken&& RHS);
	TPreAnimatedToken& operator=(TPreAnimatedToken&& RHS);
#endif

	/** The number of entities that are referencing this token (can be 0 where only global state has been saved) */
	uint32 EntityRefCount;

	/** The token that defines how to globally restore this object's state. This token should always be valid. */
	TokenType Token;

	/** An optional token that is valid when the current entity scope should restore to a different state than defined by Token. */
	TokenType OptionalEntityToken;
};

/** Template helper type definitions */
namespace MovieSceneImpl
{
	struct FNull
	{
		FNull Get() const { return *this; }
	};

	template<typename TokenType> struct TProducerType;
	template<typename TokenType> struct TPayloadType;

	template<> struct TProducerType<IMovieScenePreAnimatedTokenPtr> { typedef IMovieScenePreAnimatedTokenProducer Type; };
	template<> struct TProducerType<IMovieScenePreAnimatedGlobalTokenPtr> { typedef IMovieScenePreAnimatedGlobalTokenProducer Type; };

	template<> struct TPayloadType<IMovieScenePreAnimatedTokenPtr> { typedef TWeakObjectPtr<UObject> Type; };
	template<> struct TPayloadType<IMovieScenePreAnimatedGlobalTokenPtr> { typedef FNull Type; };
}

/**
 * Saved state for animation bound to a particular animated object
 */
template<typename TokenType>
struct MOVIESCENE_API TMovieSceneSavedTokens
{
	typedef typename MovieSceneImpl::TProducerType<TokenType>::Type ProducerType;
	typedef typename MovieSceneImpl::TPayloadType<TokenType>::Type PayloadType;

	TMovieSceneSavedTokens(PayloadType&& InPayload)
		: Payload(MoveTemp(InPayload))
	{}
	
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	TMovieSceneSavedTokens(TMovieSceneSavedTokens&&) = default;
	TMovieSceneSavedTokens& operator=(TMovieSceneSavedTokens&&) = default;
#else
	TMovieSceneSavedTokens(TMovieSceneSavedTokens&& RHS);
	TMovieSceneSavedTokens& operator=(TMovieSceneSavedTokens&& RHS);
#endif

	/**
	 * Called when animation is about to happen to cache of any existing state
	 *
	 * @param InCaptureMode 		Whether to capture for the specified associated key, globally, or not at all
	 * @param InAnimTypeID 			ID that uniquely iudentifies the type of animation token to store
	 * @param InAssociatedKey		When InCaptureMode == ECapturePreAnimatedState::Entity, defines the entity that is attempting to save the token
	 * @param InProducer			The producer responsible for creating the token, if necessary
	 * @param InParent				The pre animated state container that is making this request
	 */
	void OnPreAnimated(ECapturePreAnimatedState InCaptureMode, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey InAssociatedKey, const ProducerType& InProducer, FMovieScenePreAnimatedState& InParent);

	/**
	 * Forcefully restore all pre animated state tokens held by this container
	*
	 * @param Player 				Movie scene player that is restoring the tokens
	 */
	void Restore(IMovieScenePlayer& Player);

	/**
	 * Restore all pre animated state tokens held by this container that pass the specified filter predicate
	*
	 * @param Player 				Movie scene player that is restoring the tokens
	 * @param InFilter				Filter predicate that, when true, will restore tokens based on their animation type identifier
	 */
	void Restore(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter);

	/**
	 * Restore any pre animated state for the specified entity key, based on an optional filter
	 * @param Player			The movie scene player responsible for playing back the sequence
	 * @param EntityKey			A key for the specific entity (section or track) to restore data for
	 * @param InFilter			(optional) Filter that can optionally include/exclude specific types of animation
	 * @return True if the entity's pre-animated state was entirely restored, false if some state remains (ie, if it failed the filter)
	 */
	bool RestoreEntity(IMovieScenePlayer& Player, FMovieSceneEvaluationKey EntityKey, TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>> InFilter = TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>>());

	/**
	 * Reset all containers without applying or restoring any tokens
	 */
	void Reset();

private:

	/** Array defining how whether (and how) particular entities have evaluated */
	TArray<FMovieSceneEntityAndAnimTypeID, TInlineAllocator<8>> AnimatedEntities;

	/** Array of anim type IDs whose indices correspond to PreAnimatedTokens for efficient lookup */
	TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AllAnimatedTypeIDs;

	/** Array of tokens stored at the end of the class - these are rarely accessed */
	TArray<TPreAnimatedToken<TokenType>> PreAnimatedTokens;

	/** Payload stored with tokens */
	PayloadType Payload;
};

/**
 * Class that caches pre-animated state for objects that were manipulated by sequencer
 */
class FMovieScenePreAnimatedState
{
public:

	/**
	 * Default construction
	 */
	FMovieScenePreAnimatedState()
		: MasterTokens(MovieSceneImpl::FNull())
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::None;
	}

	FMovieScenePreAnimatedState(const FMovieScenePreAnimatedState&) = delete;
	FMovieScenePreAnimatedState& operator=(const FMovieScenePreAnimatedState&) = delete;

	/**
	 * Check whether we're currently caching pre-animated state at a global level
	 */
	bool IsGlobalCaptureEnabled() const
	{
		return DefaultGlobalCaptureMode == ECapturePreAnimatedState::Global;
	}

	/**
	 * Enable this cache and allow it to start caching state
	 */
	void EnableGlobalCapture()
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::Global;
		if (CurrentCaptureState == ECapturePreAnimatedState::None)
		{
			CurrentCaptureState = DefaultGlobalCaptureMode;
		}
	}
	
	/**
	 * Disable this cache, preventing it from caching state
	 */
	void DisableGlobalCapture()
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::None;
		if (CurrentCaptureState == ECapturePreAnimatedState::Global)
		{
			CurrentCaptureState = DefaultGlobalCaptureMode;
		}
	}

public:

	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject)
	{
		SavePreAnimatedState(InTokenType, Producer, InObject, CurrentCaptureState, CapturingStateFor);
	}

	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& Producer)
	{
		SavePreAnimatedState(InTokenType, Producer, CurrentCaptureState, CapturingStateFor);
	}

	void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject, ECapturePreAnimatedState CaptureState, FMovieSceneEvaluationKey CaptureEntity)
	{
		if (CaptureState == ECapturePreAnimatedState::None)
		{
			return;
		}

		FObjectKey ObjectKey(&InObject);

		auto* Container = ObjectTokens.Find(ObjectKey);
		if (!Container)
		{
			Container = &ObjectTokens.Add(ObjectKey, TMovieSceneSavedTokens<IMovieScenePreAnimatedTokenPtr>(&InObject));
		}

		Container->OnPreAnimated(CaptureState, InTokenType, CaptureEntity, Producer, *this);
	}

	void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& Producer, ECapturePreAnimatedState CaptureState, FMovieSceneEvaluationKey CaptureEntity)
	{
		if (CaptureState == ECapturePreAnimatedState::None)
		{
			return;
		}

		MasterTokens.OnPreAnimated(CaptureState, InTokenType, CaptureEntity, Producer, *this);
	}

	void RestorePreAnimatedState(IMovieScenePlayer& Player, const FMovieSceneEvaluationKey& Key)
	{
		RestorePreAnimatedStateImpl(Player, Key, TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>>());
	}

	void RestorePreAnimatedState(IMovieScenePlayer& Player, const FMovieSceneEvaluationKey& Key, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
	{
		RestorePreAnimatedStateImpl(Player, Key, InFilter);
	}

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player);

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object);

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter);

public:

	void SetCaptureEntity(FMovieSceneEvaluationKey InEntity, EMovieSceneCompletionMode InCompletionMode)
	{
		CapturingStateFor = InEntity;

		switch(InCompletionMode)
		{
		case EMovieSceneCompletionMode::RestoreState: 	CurrentCaptureState = ECapturePreAnimatedState::Entity;	break;
		case EMovieSceneCompletionMode::KeepState: 		CurrentCaptureState = DefaultGlobalCaptureMode;	break;
		}
	}

	void EntityHasAnimatedObject(FMovieSceneEvaluationKey EntityKey, FObjectKey ObjectKey)
	{
		EntityToAnimatedObjects.FindOrAdd(EntityKey).Add(ObjectKey);
	}

	void EntityHasAnimatedMaster(FMovieSceneEvaluationKey EntityKey)
	{
		EntityToAnimatedObjects.FindOrAdd(EntityKey).Add(FObjectKey());
	}

protected:

	MOVIESCENE_API void RestorePreAnimatedStateImpl(IMovieScenePlayer& Player, const FMovieSceneEvaluationKey& Key, TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>> InFilter);

private:

	/** Map from object key to preanimated tokens that restore it back to its previous state */
	TMap<FObjectKey, TMovieSceneSavedTokens<IMovieScenePreAnimatedTokenPtr>> ObjectTokens;

	/** Global pre animated tokens that aren't bound to particular objects */
	TMovieSceneSavedTokens<IMovieScenePreAnimatedGlobalTokenPtr> MasterTokens;

	/** Map from evaluation key to objects that it has animated (used for efficient restoration of entities) */
	TMap<FMovieSceneEvaluationKey, TArray<FObjectKey, TInlineAllocator<4>>> EntityToAnimatedObjects;

	/** Entity key that is currently being evaluated. */
	FMovieSceneEvaluationKey CapturingStateFor;

	/** Whether we are to capture state for the current entity, globally, or not at all. */
	ECapturePreAnimatedState CurrentCaptureState;

	/** Defines whether we should capture state globally or not */
	ECapturePreAnimatedState DefaultGlobalCaptureMode;
};
