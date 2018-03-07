// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundConcurrency.generated.h"

class FAudioDevice;
struct FActiveSound;

UENUM()
namespace EMaxConcurrentResolutionRule
{
	enum Type
	{
		/** When Max Concurrent sounds are active do not start a new sound. */
		PreventNew,

		/** When Max Concurrent sounds are active stop the oldest and start a new one. */
		StopOldest,

		/** When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then do not start a new sound. */
		StopFarthestThenPreventNew,

		/** When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then stop the oldest. */
		StopFarthestThenOldest,

		/** Stop the lowest priority sound in the group. If all sounds are the same priority, then it will stop the oldest sound in the group. */
		StopLowestPriority,

		/** Stop the sound that is quietest in the group. */
		StopQuietest,

		/** Stop the lowest priority sound in the group. If all sounds are the same priority, then it won't play a new sound. */
		StopLowestPriorityThenPreventNew,
	};
}


USTRUCT(BlueprintType)
struct ENGINE_API FSoundConcurrencySettings
{
	GENERATED_USTRUCT_BODY()

	/** The max number of allowable concurrent active voices for voices playing in this concurrency group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (UIMin = "1", ClampMin = "1"))
	int32 MaxCount;

	/* Whether or not to limit the concurrency to per sound owner (i.e. the actor that plays the sound). If the sound doesn't have an owner, it falls back to global concurrency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency)
	uint32 bLimitToOwner:1;

	/** Which concurrency resolution policy to use if max voice count is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency)
	TEnumAsByte<enum EMaxConcurrentResolutionRule::Type> ResolutionRule;

	/** 
	 * The amount of attenuation to apply to older voice instances in this concurrency group. This reduces volume of older voices in a concurrency group as new voices play.
	 *
	 * AppliedVolumeScale = Math.Pow(DuckingScale, VoiceGeneration)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float VolumeScale;

	FSoundConcurrencySettings()
		: MaxCount(16)
		, bLimitToOwner(false)
		, ResolutionRule(EMaxConcurrentResolutionRule::StopFarthestThenOldest)
		, VolumeScale(1.0f)
	{}
};

UCLASS(BlueprintType, hidecategories=Object, editinlinenew, MinimalAPI)
class USoundConcurrency : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Settings)
	FSoundConcurrencySettings Concurrency;
};

struct FActiveSound;

/** USoundConcurrency unique object IDs. */
typedef uint32 FConcurrencyObjectID;

/** Sound owner object IDs */
typedef uint32 FSoundOwnerObjectID;

/** Sound instance (USoundBase) object ID. */
typedef uint32 FSoundObjectID;

typedef uint32 FConcurrencyGroupID;

/** Struct which is an array of active sound pointers for tracking concurrency */
class FConcurrencyGroup
{
	/** Array of active sounds for this concurrency group. */
	TArray<FActiveSound*> ActiveSounds;
	int32 MaxActiveSounds;
	FConcurrencyGroupID ConcurrencyGroupID;
	EMaxConcurrentResolutionRule::Type ResolutionRule;
	int32 Generation;

public:
	/** Constructor for the max concurrency active sound entry. */
	FConcurrencyGroup();
	FConcurrencyGroup(struct FActiveSound* ActiveSound);

	/** Retrieves the active sounds array. */
	TArray<FActiveSound*>& GetActiveSounds();

	/** Returns the number of active sounds in concurrency group. */
	const int32 GetNumActiveSounds() const { return ActiveSounds.Num(); }

	/** Retrieves the current generation */
	const int32 GetGeneration() const { return Generation; }

	/** Adds an active sound to the active sound array. */
	void AddActiveSound(struct FActiveSound* ActiveSound);

	/** Sorts the active sound array by volume */
	void StopQuietSoundsDueToMaxConcurrency();

	FConcurrencyGroupID GetID() const { return ConcurrencyGroupID; }
};

typedef TMap<FConcurrencyGroupID, FConcurrencyGroup> FConcurrencyGroups;

struct FSoundInstanceEntry
{
	TMap<FSoundObjectID, FConcurrencyGroupID> SoundInstanceToConcurrencyGroup;

	FSoundInstanceEntry(FSoundObjectID SoundObjectID, FConcurrencyGroupID GroupID)
	{
		SoundInstanceToConcurrencyGroup.Add(SoundObjectID, GroupID);
	}
};

/** Type for mapping an object id to a concurrency entry. */
typedef TMap<FConcurrencyObjectID, FConcurrencyGroupID> FConcurrencyMap;

struct FOwnerConcurrencyMapEntry
{
	FConcurrencyMap ConcurrencyObjectToConcurrencyGroup;

	FOwnerConcurrencyMapEntry(FConcurrencyObjectID ConcurrencyObjectID, FConcurrencyGroupID GroupID)
	{
		ConcurrencyObjectToConcurrencyGroup.Add(ConcurrencyObjectID, GroupID);
	}
};

/** Maps owners to concurrency maps */
typedef TMap<FSoundOwnerObjectID, FOwnerConcurrencyMapEntry> FOwnerConcurrencyMap;

/** Maps owners to sound instances */
typedef TMap<FSoundOwnerObjectID, FSoundInstanceEntry> FOwnerPerSoundConcurrencyMap;

/** Maps sound object ids to active sound array for global concurrency limiting */
typedef TMap<FSoundObjectID, FConcurrencyGroupID> FPerSoundToActiveSoundsMap;

class FSoundConcurrencyManager
{
public:
	FSoundConcurrencyManager(class FAudioDevice* InAudioDevice);
	ENGINE_API ~FSoundConcurrencyManager();

	/** Returns a newly allocated active sound given the input active sound struct. Will return nullptr if the active sound concurrency evaluation doesn't allow for it. */
	FActiveSound* CreateNewActiveSound(const FActiveSound& NewActiveSound);

	/** Removes the active sound from the manager to remove it from concurrency tracking. */
	void RemoveActiveSound(FActiveSound* ActiveSound);

	/** Stops any active sounds due to max concurrency quietest sound resolution rule */
	void StopQuietSoundsDueToMaxConcurrency();

private: // Methods
	/** This function handles the concurrency evaluation that happens per USoundConcurrencyObject */
	FActiveSound* HandleConcurrencyEvaluation(const FActiveSound& NewActiveSound);

	/** This function handles the concurrency evaluation that happens per-voice rather than per USoundConcurrencyObject */
	FActiveSound* HandleConcurrencyEvaluationOverride(const FActiveSound& NewActiveSound);

	/** Resolves the concurrency resolution rule for a given sound and its concurrency group */
	FActiveSound* ResolveConcurrency(const FActiveSound& NewActiveSound, FConcurrencyGroup& ConcurrencyGroup);

	FActiveSound* MakeNewActiveSound(const FActiveSound& NewActiveSound);

	FConcurrencyGroupID MakeNewConcurrencyGroupAndSound(FActiveSound** OutActiveSound, const FActiveSound& NewActiveSound);

private: // Data

	/** Owning audio device ptr for the concurrency manager. */
	FAudioDevice* AudioDevice;

	/** Global concurrency map that maps individual sounds instances to shared USoundConcurrency UObjects. */
	FConcurrencyMap ConcurrencyMap;

	FOwnerConcurrencyMap OwnerConcurrencyMap;

	/** A map of owners to concurrency maps for sounds which are concurrency-limited per sound owner. */
	FOwnerPerSoundConcurrencyMap OwnerPerSoundConcurrencyMap;

	FPerSoundToActiveSoundsMap SoundObjectToActiveSounds;

	/** A map of concurrency active sound ID to concurrency active sounds */
	FConcurrencyGroups ConcurrencyGroups;

};
