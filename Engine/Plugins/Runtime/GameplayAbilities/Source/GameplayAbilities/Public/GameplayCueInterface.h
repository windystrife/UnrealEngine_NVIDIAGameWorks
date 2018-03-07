// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Engine/NetSerialization.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "GameplayCueInterface.generated.h"

/** Interface for actors that wish to handle GameplayCue events from GameplayEffects. Native only because blueprints can't implement interfaces with native functions */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGameplayCueInterface: public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GAMEPLAYABILITIES_API IGameplayCueInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual void HandleGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	virtual void HandleGameplayCues(AActor *Self, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Returns true if the actor can currently accept gameplay cues associated with the given tag. Returns true by default. Allows actors to opt out of cues in cases such as pending death */
	virtual bool ShouldAcceptGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Return the cue sets used by this object. This is optional and it is possible to leave this list empty. */
	virtual void GetGameplayCueSets(TArray<class UGameplayCueSet*>& OutSets) const {}

	/** Default native handler, called if no tag matches found */
	virtual void GameplayCueDefaultHandler(EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Internal function to map ufunctions directly to gameplaycue tags */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = GameplayCue, meta = (BlueprintInternalUseOnly = "true"))
	void BlueprintCustomHandler(EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Call from a Cue handler event to continue checking for additional, more generic handlers. Called from the ability system blueprint library */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Ability|GameplayCue")
	virtual void ForwardGameplayCueToParent();

	static void DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	static void ClearTagToFunctionMap();

	IGameplayCueInterface() : bForwardToParent(false) {}

private:
	/** If true, keep checking for additional handlers */
	bool bForwardToParent;

};


/**
 *	This is meant to provide another way of using GameplayCues without having to go through GameplayEffects.
 *	E.g., it is convenient if GameplayAbilities can issue replicated GameplayCues without having to create
 *	a GameplayEffect.
 *	
 *	Essentially provides bare necessities to replicate GameplayCue Tags.
 */


struct FActiveGameplayCueContainer;

USTRUCT(BlueprintType)
struct FActiveGameplayCue : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayCue()	
	{
		bPredictivelyRemoved = false;
	}

	UPROPERTY()
	FGameplayTag GameplayCueTag;

	UPROPERTY()
	FPredictionKey PredictionKey;

	UPROPERTY()
	FGameplayCueParameters Parameters;

	/** Has this been predictively removed on the client? */
	UPROPERTY(NotReplicated)
	bool bPredictivelyRemoved;

	void PreReplicatedRemove(const struct FActiveGameplayCueContainer &InArray);
	void PostReplicatedAdd(const struct FActiveGameplayCueContainer &InArray);
	void PostReplicatedChange(const struct FActiveGameplayCueContainer &InArray) { }

	FString GetDebugString();
};

USTRUCT(BlueprintType)
struct FActiveGameplayCueContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray< FActiveGameplayCue >	GameplayCues;

	UPROPERTY()
	class UAbilitySystemComponent*	Owner;

	/** Should this container only replicate in minimal replication mode */
	bool bMinimalReplication;

	void AddCue(const FGameplayTag& Tag, const FPredictionKey& PredictionKey, const FGameplayCueParameters& Parameters);
	void RemoveCue(const FGameplayTag& Tag);

	/** Marks as predictively removed so that we dont invoke remove event twice due to onrep */
	void PredictiveRemove(const FGameplayTag& Tag);

	void PredictiveAdd(const FGameplayTag& Tag, FPredictionKey& PredictionKey);

	/** Does explicit check for gameplay cue tag */
	bool HasCue(const FGameplayTag& Tag) const;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms);

private:

	int32 GetGameStateTime(const UWorld* World) const;
};

template<>
struct TStructOpsTypeTraits< FActiveGameplayCueContainer > : public TStructOpsTypeTraitsBase2< FActiveGameplayCueContainer >
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};


/**
 *	Wrapper struct around a gameplaytag with the GameplayCue category. This also allows for a details customization
 */
USTRUCT(BlueprintType)
struct FGameplayCueTag
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories="GameplayCue"), Category="GameplayCue")
	FGameplayTag GameplayCueTag;

	bool IsValid() const
	{
		return GameplayCueTag.IsValid();
	}
};
