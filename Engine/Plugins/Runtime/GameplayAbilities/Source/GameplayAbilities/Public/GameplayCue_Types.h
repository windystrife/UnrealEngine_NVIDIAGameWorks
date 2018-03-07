// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "UObject/ObjectKey.h"
#include "GameplayEffect.h"
#include "GameplayCue_Types.generated.h"

class AGameplayCueNotify_Actor;
class UAbilitySystemComponent;
class UGameplayCueSet;

UENUM()
enum class EGameplayCuePayloadType : uint8
{
	EffectContext,
	CueParameters,
	FromSpec,
};

/** Structure to keep track of pending gameplay cues that haven't been applied yet. */
USTRUCT()
struct FGameplayCuePendingExecute
{
	GENERATED_USTRUCT_BODY()

	FGameplayCuePendingExecute()
	: PayloadType(EGameplayCuePayloadType::EffectContext)
	, OwningComponent(NULL)
	{
	}
	
	TArray<FGameplayTag, TInlineAllocator<1> > GameplayCueTags;
	
	/** Prediction key that spawned this cue */
	UPROPERTY()
	FPredictionKey PredictionKey;

	/** What type of payload is attached to this cue */
	UPROPERTY()
	EGameplayCuePayloadType PayloadType;

	/** What component to send the cue on */
	UPROPERTY()
	UAbilitySystemComponent* OwningComponent;

	/** If this cue is from a spec, here's the copy of that spec */
	UPROPERTY()
	FGameplayEffectSpecForRPC FromSpec;

	/** Store the full cue parameters or just the effect context depending on type */
	UPROPERTY()
	FGameplayCueParameters CueParameters;
};

/** Struct for pooling and preallocating gameplaycuenotify_actor classes. This data is per world and used to track what actors are available to recycle and which classes need to preallocate instances of those actors */
USTRUCT()
struct FPreallocationInfo
{
	GENERATED_USTRUCT_BODY()

	TMap<UClass*, TArray<AGameplayCueNotify_Actor*> >	PreallocatedInstances;

	UPROPERTY(transient)
	TArray<TSubclassOf<AGameplayCueNotify_Actor>>	ClassesNeedingPreallocation;

	FObjectKey OwningWorldKey;
};

/** Struct that is used by the gameplaycue manager to tie an instanced gameplaycue to the calling gamecode. Usually this is just the target actor, but can also be unique per instigator/sourceobject */
struct FGCNotifyActorKey
{
	FGCNotifyActorKey()
	{

	}

	FGCNotifyActorKey(AActor* InTargetActor, UClass* InCueClass, AActor* InInstigatorActor=nullptr, const UObject* InSourceObj=nullptr)
	{
		TargetActor = FObjectKey(InTargetActor);
		OptionalInstigatorActor = FObjectKey(InInstigatorActor);
		OptionalSourceObject = FObjectKey(InSourceObj);
		CueClass = FObjectKey(InCueClass);
	}

	

	FObjectKey	TargetActor;
	FObjectKey	OptionalInstigatorActor;
	FObjectKey	OptionalSourceObject;
	FObjectKey	CueClass;

	FORCEINLINE bool operator==(const FGCNotifyActorKey& Other) const
	{
		return TargetActor == Other.TargetActor && CueClass == Other.CueClass &&
				OptionalInstigatorActor == Other.OptionalInstigatorActor && OptionalSourceObject == Other.OptionalSourceObject;
	}
};

FORCEINLINE uint32 GetTypeHash(const FGCNotifyActorKey& Key)
{
	return GetTypeHash(Key.TargetActor)	^
			GetTypeHash(Key.OptionalInstigatorActor) ^
			GetTypeHash(Key.OptionalSourceObject) ^
			GetTypeHash(Key.CueClass);
}

/**
 *	FScopedGameplayCueSendContext
 *	Add this around code that sends multiple gameplay cues to allow grouping them into a smalkler number of cues for more efficient networking
 */
struct GAMEPLAYABILITIES_API FScopedGameplayCueSendContext
{
	FScopedGameplayCueSendContext();
	~FScopedGameplayCueSendContext();
};

/** Delegate for when GC notifies are added or removed from manager */
DECLARE_MULTICAST_DELEGATE(FOnGameplayCueNotifyChange);