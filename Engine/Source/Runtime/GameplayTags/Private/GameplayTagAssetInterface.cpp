// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagAssetInterface.h"

UGameplayTagAssetInterface::UGameplayTagAssetInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool IGameplayTagAssetInterface::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);

	return OwnedTags.HasTag(TagToCheck);
}

bool IGameplayTagAssetInterface::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);

	return OwnedTags.HasAll(TagContainer);
}

bool IGameplayTagAssetInterface::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);

	return OwnedTags.HasAny(TagContainer);
}
