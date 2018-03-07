// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"

class FComponentClassComboEntry;

DECLARE_MULTICAST_DELEGATE(FOnComponentTypeListChanged);

typedef TSharedPtr<class FComponentClassComboEntry> FComponentClassComboEntryPtr;

struct FComponentTypeEntry
{
	/** Name of the component, as typed by the user */
	FString ComponentName;

	/** Name of the component, corresponds to asset name for blueprint components */
	FString ComponentAssetName;

	/** Optional pointer to the UClass, will be nullptr for blueprint components that aren't loaded */
	class UClass* ComponentClass;
};

struct UNREALED_API FComponentTypeRegistry
{
	static FComponentTypeRegistry& Get();

	/**
	 * Called when the user changes the text in the search box.
	 * @OutComponentList Pointer that will be set to the (globally shared) component type list
	 * @return Deleate that can be used to handle change notifications. change notifications are raised when entries are 
	 *	added or removed from the component type list
	 */
	FOnComponentTypeListChanged& SubscribeToComponentList(TArray<FComponentClassComboEntryPtr>*& OutComponentList);
	FOnComponentTypeListChanged& SubscribeToComponentList(const TArray<FComponentTypeEntry>*& OutComponentList);
	FOnComponentTypeListChanged& GetOnComponentTypeListChanged();

	/**
	 * Called when a specific class has been updated and should force the component type registry to update as well
	 */
	void InvalidateClass(TSubclassOf<UActorComponent> ClassToUpdate);
private:
	void OnProjectHotReloaded( bool bWasTriggeredAutomatically );

private:
	FComponentTypeRegistry();
	~FComponentTypeRegistry();
	struct FComponentTypeRegistryData* Data;
};
