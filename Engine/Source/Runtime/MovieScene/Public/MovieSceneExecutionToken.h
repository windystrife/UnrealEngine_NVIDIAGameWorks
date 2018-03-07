// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneFwd.h"
#include "Misc/InlineValue.h"
#include "Evaluation/MovieSceneAnimTypeID.h"

class IMovieScenePlayer;
struct FMovieSceneContext;
struct FMovieSceneEvaluationOperand;
struct FPersistentEvaluationData;


/**
 * Base class for all pre-animated state tokens that apply to UObjects. Store any cached state in derived classes
 */
struct IMovieScenePreAnimatedToken
{
	virtual ~IMovieScenePreAnimatedToken() {}

	/**
	 * Restore state for the specified object, only called when this token was created with a bound object
	 *
	 * @param Object The object to restore state for
	 * @param Player The movie scene player that is responsible for playing back the sequence
	 */
	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) = 0;
};
typedef TInlineValue<IMovieScenePreAnimatedToken, 32> IMovieScenePreAnimatedTokenPtr;

/**
 * Type required for production of pre-animated state tokens.
 * Implemented as a type rather than a callback to ensure efficient construction (these types are often constructed, but rarely utilized)
 */
struct IMovieScenePreAnimatedTokenProducer
{
	virtual ~IMovieScenePreAnimatedTokenProducer(){}

	/**
	 * Perform any initial set up required to animate the specified object
	 * @note Only ever called when Object is in an unanimated state, as according to the AnimTypeID that this producer is operating on.
	 * 
	 * @param Object 		The object to initialize
	 */
	virtual void InitializeObjectForAnimation(UObject& Object) const
	{}

	/**
	 * Produce a token that can be used to return the specified object back to its current state
	 * @note Under some circumstances, the object may already be animated (for instance, after something has animated the object, but didn't restore state)
	 * 
	 * @param Object 		The object to initialize
	 *
	 * @return A valid pre animated token that will restore this object back to its current state
	 */
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const = 0;
};

/**
 * Base class for all pre-animated state tokens that do not apply to UObjects. Store any cached state in derived classes.
 */
struct IMovieScenePreAnimatedGlobalToken
{
	virtual ~IMovieScenePreAnimatedGlobalToken() {}

	/**
	 * Restore global state for a master track.
	 *
	 * @param Player The movie scene player that is responsible for playing back the sequence
	 */
	virtual void RestoreState(IMovieScenePlayer& Player) = 0;
};
typedef TInlineValue<IMovieScenePreAnimatedGlobalToken, 32> IMovieScenePreAnimatedGlobalTokenPtr;

/**
 * Type required for production of pre-animated state tokens.
 * Implemented as a type rather than a callback to ensure efficient construction (these types are often constructed, but rarely utilized)
 */
struct IMovieScenePreAnimatedGlobalTokenProducer
{
	virtual ~IMovieScenePreAnimatedGlobalTokenProducer() { }

	/**
	 * Perform any initial set up required to animate the playback environment
	 * @note Only ever called when environment is in an unanimated state, as according to the AnimTypeID that this producer is operating on.
	 */
	virtual void InitializeForAnimation() const {}

	/**
	 * Produce a token that can be used to return the playback environment back to its current state
	 * @note Under some circumstances, the environment may already be animated (for instance, after something has animated, but didn't restore state)
	 *
	 * @return A valid pre animated token that will restore the environment back to its current state
	 */
	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const = 0;
};

/**
 * Base class for all execution tokens that are produced by evaluation templates
 */
struct IMovieSceneExecutionToken
{
	virtual ~IMovieSceneExecutionToken() {}
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) = 0;
};

/** Identifiyable and sortable execution token. Gets evaluated before (Order <=0) or after (Order > 0) IMovieSceneExecutionTokens */
struct IMovieSceneSharedExecutionToken
{
	IMovieSceneSharedExecutionToken()
		: Order(0)
	{}
	
	virtual ~IMovieSceneSharedExecutionToken() {}

	/** Execute this token */
	virtual void Execute(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) = 0;

	/** The order at which this token should be executed. <= 0 executed before IMovieSceneExecutionTokens, > 0 after */
	int32 Order;
};

/** Stateless pre-animated state token producer that simply calls a static function as the token */
struct FStatelessPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	typedef void (*StaticFunction)(UObject&, IMovieScenePlayer&);

	FStatelessPreAnimatedTokenProducer(StaticFunction InFunction) : Function(InFunction) {}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		return FToken(Function);
	}
	
	struct FToken : IMovieScenePreAnimatedToken
	{
		FToken(StaticFunction InFunctionPtr) : FunctionPtr(InFunctionPtr) {}

		virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
		{
			(*FunctionPtr)(Object, Player);
		}

		StaticFunction FunctionPtr;
	};
	StaticFunction Function;
};

/** Templated stateless pre-animated state token producer that simply creates the templated type */
template<typename TokenType>
struct TStatelessPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		return TokenType();
	}
};

/** Templated pre-animated state token producer that forwards the object onto the templated type */
template<typename TokenType>
struct TForwardingPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		return TokenType(Object);
	}
};

