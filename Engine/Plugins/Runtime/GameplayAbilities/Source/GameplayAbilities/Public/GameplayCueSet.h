// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueSet.generated.h"

USTRUCT()
struct FGameplayCueNotifyData
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueNotifyData()
	: LoadedGameplayCueClass(nullptr)
	, ParentDataIdx( INDEX_NONE )
	{
	}

	UPROPERTY(EditAnywhere, Category=GameplayCue)
	FGameplayTag GameplayCueTag;

	UPROPERTY(EditAnywhere, Category=GameplayCue, meta=(AllowedClasses="GameplayCueNotify"))
	FSoftObjectPath GameplayCueNotifyObj;

	UPROPERTY(transient)
	UClass* LoadedGameplayCueClass;

	int32 ParentDataIdx;
};

struct FGameplayCueReferencePair
{
	FGameplayTag GameplayCueTag;
	FSoftObjectPath StringRef;

	FGameplayCueReferencePair(const FGameplayTag& InGameplayCueTag, const FSoftObjectPath& InStringRef)
		: GameplayCueTag(InGameplayCueTag)
		, StringRef(InStringRef)
	{}
};

/**
 *	A set of gameplay cue actors to handle gameplay cue events
 */
UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueSet : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	/** Handles the cue event by spawning the cue actor. Returns true if the event was handled. */
	virtual bool HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	/** Adds a list of cues to the set */
	virtual void AddCues(const TArray<FGameplayCueReferencePair>& CuesToAdd);

	/** Removes all cues from the set matching any of the supplied tags */
	virtual void RemoveCuesByTags(const FGameplayTagContainer& TagsToRemove);

	/** Removes all cues from the set matching the supplied string refs */
	virtual void RemoveCuesByStringRefs(const TArray<FSoftObjectPath>& CuesToRemove);

	/** Nulls reference to the loaded class. Note this doesn't remove the entire cue from the internal data structure, just the hard ref to the loaded class */
	virtual void RemoveLoadedClass(UClass* Class);

	/** Returns filenames of everything we know about (loaded or not) */
	virtual void GetFilenames(TArray<FString>& Filenames) const;

	/** Extracts all soft object paths pointing to Cues */
	virtual void GetSoftObjectPaths(TArray<FSoftObjectPath>& List) const;

#if WITH_EDITOR

	void CopyCueDataToSetForEditorPreview(FGameplayTag Tag, UGameplayCueSet* DestinationSet);

	/** Updates an existing cue */
	virtual void UpdateCueByStringRefs(const FSoftObjectPath& CueToRemove, FString NewPath);

#endif

	/** Removes all cues from the set */
	virtual void Empty();

	virtual void PrintCues() const;
	
	UPROPERTY(EditAnywhere, Category=CueSet)
	TArray<FGameplayCueNotifyData> GameplayCueData;

	/** Maps GameplayCue Tag to index into above GameplayCues array. */
	TMap<FGameplayTag, int32> GameplayCueDataMap;

	static FGameplayTag	BaseGameplayCueTag();

protected:
	virtual bool HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters& Parameters);
	virtual void BuildAccelerationMap_Internal();
};
