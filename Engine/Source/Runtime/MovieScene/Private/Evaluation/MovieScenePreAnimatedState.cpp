// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePreAnimatedState.h"

DECLARE_CYCLE_STAT(TEXT("Save Pre Animated State"), MovieSceneEval_SavePreAnimatedState, STATGROUP_MovieSceneEval);

namespace MovieSceneImpl
{
	void InitializeForAnimation(const IMovieScenePreAnimatedTokenProducer& Producer, UObject* Object)
	{
		checkSlow(Object);
		Producer.InitializeObjectForAnimation(*Object);
	}

	void InitializeForAnimation(const IMovieScenePreAnimatedGlobalTokenProducer& Producer, FNull)
	{
		Producer.InitializeForAnimation();
	}

	IMovieScenePreAnimatedTokenPtr CacheExistingState(const IMovieScenePreAnimatedTokenProducer& Producer, UObject* Object)
	{
		checkSlow(Object);
		return Producer.CacheExistingState(*Object);
	}

	IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState(const IMovieScenePreAnimatedGlobalTokenProducer& Producer, FNull)
	{
		return Producer.CacheExistingState();
	}

	void RestorePreAnimatedToken(TPreAnimatedToken<IMovieScenePreAnimatedTokenPtr>& Token, IMovieScenePlayer& Player, UObject* Object)
	{
		if (Object)
		{
			if (Token.OptionalEntityToken.IsValid())
			{
				Token.OptionalEntityToken->RestoreState(*Object, Player);
			}
			else
			{
				Token.Token->RestoreState(*Object, Player);
			}
		}
	}

	void RestorePreAnimatedToken(TPreAnimatedToken<IMovieScenePreAnimatedGlobalTokenPtr>& Token, IMovieScenePlayer& Player, FNull)
	{
		if (Token.OptionalEntityToken.IsValid())
		{
			Token.OptionalEntityToken->RestoreState(Player);
		}
		else
		{
			Token.Token->RestoreState(Player);
		}
	}

	void EntityHasAnimated(FMovieSceneEvaluationKey AssociatedKey, FMovieScenePreAnimatedState& Parent, FNull)
	{
		Parent.EntityHasAnimatedMaster(AssociatedKey);
	}

	void EntityHasAnimated(FMovieSceneEvaluationKey AssociatedKey, FMovieScenePreAnimatedState& Parent, UObject* InObject)
	{
		if (InObject)
		{
			Parent.EntityHasAnimatedObject(AssociatedKey, FObjectKey(InObject));
		}
	}
}

template<typename TokenType>
TPreAnimatedToken<TokenType>::TPreAnimatedToken(TokenType&& InToken)
	: EntityRefCount(0)
	, Token(MoveTemp(InToken))
{}

#if !PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
template<typename TokenType>
TPreAnimatedToken<TokenType>::TPreAnimatedToken(TPreAnimatedToken&& RHS)
	: EntityRefCount(RHS.EntityRefCount)
	, Token(MoveTemp(RHS.Token))
{
}
template<typename TokenType>
TPreAnimatedToken<TokenType>& TPreAnimatedToken<TokenType>::operator=(TPreAnimatedToken&& RHS)
{
	EntityRefCount = RHS.EntityRefCount;
	Token = MoveTemp(RHS.Token);
	return *this;
}

template<typename TokenType>
TMovieSceneSavedTokens<TokenType>::TMovieSceneSavedTokens(TMovieSceneSavedTokens&& RHS)
	: AnimatedEntities(MoveTemp(RHS.AnimatedEntities))
	, AllAnimatedTypeIDs(MoveTemp(RHS.AllAnimatedTypeIDs))
	, PreAnimatedTokens(MoveTemp(RHS.PreAnimatedTokens))
	, Payload(MoveTemp(RHS.Payload))
{
}
template<typename TokenType>
TMovieSceneSavedTokens<TokenType>& TMovieSceneSavedTokens<TokenType>::operator=(TMovieSceneSavedTokens&& RHS)
{
	AnimatedEntities = MoveTemp(RHS.AnimatedEntities);
	AllAnimatedTypeIDs = MoveTemp(RHS.AllAnimatedTypeIDs);
	PreAnimatedTokens = MoveTemp(RHS.PreAnimatedTokens);
	Payload = MoveTemp(RHS.Payload);
	return *this;
}
#endif

template<typename TokenType>
void TMovieSceneSavedTokens<TokenType>::OnPreAnimated(ECapturePreAnimatedState CaptureState, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey AssociatedKey, const ProducerType& Producer, FMovieScenePreAnimatedState& Parent)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SavePreAnimatedState)

	if (CaptureState == ECapturePreAnimatedState::None)
	{
		return;
	}

	if (CaptureState == ECapturePreAnimatedState::Entity)
	{
		FMovieSceneEntityAndAnimTypeID EntityAndTypeID{AssociatedKey, InAnimTypeID};

		// If the entity key and anim type combination already exists in the animated entities array, we've already got a preanimated token reference
		if (AnimatedEntities.Contains(EntityAndTypeID))
		{
			return;
		}

		AnimatedEntities.Add(EntityAndTypeID);
	}

	auto ResolvedPayload = Payload.Get();

	int32 TokenIndex = AllAnimatedTypeIDs.IndexOfByKey(InAnimTypeID);
	if (TokenIndex == INDEX_NONE)
	{
		// Create the token, and update the arrays
		AllAnimatedTypeIDs.Add(InAnimTypeID);
		PreAnimatedTokens.Add(TokenType(MovieSceneImpl::CacheExistingState(Producer, ResolvedPayload)));

		// If we're capturing for the entity as well, increment the ref count
		if (CaptureState == ECapturePreAnimatedState::Entity)
		{
			++PreAnimatedTokens.Last().EntityRefCount;
			MovieSceneImpl::EntityHasAnimated(AssociatedKey, Parent, ResolvedPayload);
		}

		// Never been animated, so call initialize on the producer (after we've cached the existing state)
		MovieSceneImpl::InitializeForAnimation(Producer, ResolvedPayload);
	}
	else if (CaptureState == ECapturePreAnimatedState::Entity)
	{
		// We already have a token animated
		TPreAnimatedToken<TokenType>& Token = PreAnimatedTokens[TokenIndex];

		if (Token.EntityRefCount == 0)
		{
			// If the ref count is 0, a previous entity must have animated, but been set to 'keep state'. In this case, we need to define an additional token to ensure we restore to the correct (current) value.
			// Don't call InitializeForAnimation here, as we've clearly already done so (a token exists for it)
			Token.OptionalEntityToken = MovieSceneImpl::CacheExistingState(Producer, ResolvedPayload);
		}

		// Increment the reference count regardless of whether we just created the token or not (we always need a reference)
		++Token.EntityRefCount;
		MovieSceneImpl::EntityHasAnimated(AssociatedKey, Parent, ResolvedPayload);
	}
}

template<typename TokenType>
void TMovieSceneSavedTokens<TokenType>::Restore(IMovieScenePlayer& Player)
{
	auto ResolvedPayload = Payload.Get();

	// Restore in reverse
	for (int32 Index = PreAnimatedTokens.Num() - 1; Index >= 0; --Index)
	{
		MovieSceneImpl::RestorePreAnimatedToken(PreAnimatedTokens[Index], Player, ResolvedPayload);
	}

	Reset();
}

template<typename TokenType>
void TMovieSceneSavedTokens<TokenType>::Restore(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
{
	auto ResolvedPayload = Payload.Get();
	
	for (int32 TokenIndex = AllAnimatedTypeIDs.Num() - 1; TokenIndex >= 0; --TokenIndex)
	{
		FMovieSceneAnimTypeID ThisTokenID = AllAnimatedTypeIDs[TokenIndex];
		if (InFilter(ThisTokenID))
		{
			MovieSceneImpl::RestorePreAnimatedToken(PreAnimatedTokens[TokenIndex], Player, ResolvedPayload);

			AllAnimatedTypeIDs.RemoveAtSwap(TokenIndex, 1, false);
			PreAnimatedTokens.RemoveAtSwap(TokenIndex, 1, false);

			AnimatedEntities.RemoveAll(
				[=](const FMovieSceneEntityAndAnimTypeID& InEntityAndAnimType)
				{
					return InEntityAndAnimType.AnimTypeID == ThisTokenID;
				}
			);
		}
	}
}

template<typename TokenType>
bool TMovieSceneSavedTokens<TokenType>::RestoreEntity(IMovieScenePlayer& Player, FMovieSceneEvaluationKey EntityKey, TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>> InFilter)
{
	TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AnimTypesToRestore;

	bool bEntityHasBeenEntirelyRestored = true;
	for (int32 LUTIndex = AnimatedEntities.Num() - 1; LUTIndex >= 0; --LUTIndex)
	{
		FMovieSceneEntityAndAnimTypeID EntityAndAnimType = AnimatedEntities[LUTIndex];
		if (EntityAndAnimType.EntityKey == EntityKey)
		{
			if (!InFilter.IsSet() || InFilter.GetValue()(EntityAndAnimType.AnimTypeID))
			{
				// Ask that this anim type have a reference removed
				AnimTypesToRestore.Add(EntityAndAnimType.AnimTypeID);

				// This entity is no longer animating this anim type ID
				AnimatedEntities.RemoveAtSwap(LUTIndex);
			}
			else
			{
				bEntityHasBeenEntirelyRestored = false;
			}
		}
	}

	auto ResolvedPayload = Payload.Get();
	for (int32 TokenIndex = AllAnimatedTypeIDs.Num() - 1; TokenIndex >= 0; --TokenIndex)
	{
		FMovieSceneAnimTypeID ThisTokenID = AllAnimatedTypeIDs[TokenIndex];
		if (AnimTypesToRestore.Contains(ThisTokenID) && --PreAnimatedTokens[TokenIndex].EntityRefCount == 0)
		{
			TPreAnimatedToken<TokenType>& Token = PreAnimatedTokens[TokenIndex];
			MovieSceneImpl::RestorePreAnimatedToken(Token, Player, ResolvedPayload);
			
			// Where an optiona entity token exists, the global stored stae differs from the entity saved state,
			// so we only want to null out the entity token leaving the global state still saved
			if (Token.OptionalEntityToken.IsValid())
			{
				Token.OptionalEntityToken.Reset();
			}
			else
			{
				AllAnimatedTypeIDs.RemoveAtSwap(TokenIndex, 1, false);
				PreAnimatedTokens.RemoveAtSwap(TokenIndex, 1, false);
			}
		}
	}

	return bEntityHasBeenEntirelyRestored;
}

template<typename TokenType>
void TMovieSceneSavedTokens<TokenType>::Reset()
{
	AnimatedEntities.Reset();
	AllAnimatedTypeIDs.Reset();
	PreAnimatedTokens.Reset();
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player)
{
	for (auto& Pair : ObjectTokens)
	{
		Pair.Value.Restore(Player);
	}

	MasterTokens.Restore(Player);

	ObjectTokens.Reset();
	EntityToAnimatedObjects.Reset();
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object)
{
	FObjectKey ObjectKey(&Object);

	auto* FoundObjectTokens = ObjectTokens.Find(ObjectKey);
	if (FoundObjectTokens)
	{
		FoundObjectTokens->Restore(Player);
	}

	for (auto& Pair : EntityToAnimatedObjects)
	{
		Pair.Value.Remove(ObjectKey);
	}
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
{
	auto* FoundObjectTokens = ObjectTokens.Find(&Object);
	if (FoundObjectTokens)
	{
		FoundObjectTokens->Restore(Player, InFilter);
	}
}

void FMovieScenePreAnimatedState::RestorePreAnimatedStateImpl(IMovieScenePlayer& Player, const FMovieSceneEvaluationKey& Key, TOptional<TFunctionRef<bool(FMovieSceneAnimTypeID)>> InFilter)
{
	auto* AnimatedObjects = EntityToAnimatedObjects.Find(Key);
	if (!AnimatedObjects)
	{
		return;
	}

	bool bEntityHasBeenEntirelyRestored = true;
	for (FObjectKey ObjectKey : *AnimatedObjects)
	{
		if (ObjectKey == FObjectKey())
		{
			bEntityHasBeenEntirelyRestored = MasterTokens.RestoreEntity(Player, Key, InFilter) && bEntityHasBeenEntirelyRestored;
		}
		else if (auto* FoundState = ObjectTokens.Find(ObjectKey))
		{
			bEntityHasBeenEntirelyRestored = FoundState->RestoreEntity(Player, Key, InFilter) && bEntityHasBeenEntirelyRestored;
		}
	}

	if (bEntityHasBeenEntirelyRestored)
	{
		EntityToAnimatedObjects.Remove(Key);
	}
}

/** Explicit, exported template instantiations */
template struct MOVIESCENE_API TMovieSceneSavedTokens<IMovieScenePreAnimatedTokenPtr>;
template struct MOVIESCENE_API TMovieSceneSavedTokens<IMovieScenePreAnimatedGlobalTokenPtr>;

template struct MOVIESCENE_API TPreAnimatedToken<IMovieScenePreAnimatedTokenPtr>;
template struct MOVIESCENE_API TPreAnimatedToken<IMovieScenePreAnimatedGlobalTokenPtr>;