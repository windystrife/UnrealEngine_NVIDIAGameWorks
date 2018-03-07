// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneInitialValueStore.h"
#include "MovieSceneBlendingActuator.h"

struct FMovieSceneRemoveInitialValueToken : IMovieScenePreAnimatedToken
{
	FMovieSceneRemoveInitialValueToken(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
		: WeakActuator(InWeakActuator)
	{}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		TSharedPtr<IMovieSceneBlendingActuator> Store = WeakActuator.Pin();
		if (Store.IsValid())
		{
			Store->RemoveInitialValueForObject(FObjectKey(&Object));
		}
	}

private:
	/** The store to remove the initial value from */
	TWeakPtr<IMovieSceneBlendingActuator> WeakActuator;
};

struct FMovieSceneRemoveInitialGlobalValueToken : IMovieScenePreAnimatedGlobalToken
{
	FMovieSceneRemoveInitialGlobalValueToken(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
		: WeakActuator(InWeakActuator)
	{}

	virtual void RestoreState(IMovieScenePlayer& Player) override
	{
		TSharedPtr<IMovieSceneBlendingActuator> Store = WeakActuator.Pin();
		if (Store.IsValid())
		{
			Store->RemoveInitialValueForObject(FObjectKey());
		}
	}

private:
	/** The store to remove the initial value from */
	TWeakPtr<IMovieSceneBlendingActuator> WeakActuator;
};

FMovieSceneRemoveInitialValueTokenProducer::FMovieSceneRemoveInitialValueTokenProducer(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
	: WeakActuator(InWeakActuator)
{
}

IMovieScenePreAnimatedTokenPtr FMovieSceneRemoveInitialValueTokenProducer::CacheExistingState(UObject& Object) const
{
	return FMovieSceneRemoveInitialValueToken(WeakActuator);
}

FMovieSceneRemoveInitialGlobalValueTokenProducer::FMovieSceneRemoveInitialGlobalValueTokenProducer(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
	: WeakActuator(InWeakActuator)
{
}

IMovieScenePreAnimatedGlobalTokenPtr FMovieSceneRemoveInitialGlobalValueTokenProducer::CacheExistingState() const
{
	return FMovieSceneRemoveInitialGlobalValueToken(WeakActuator);
}