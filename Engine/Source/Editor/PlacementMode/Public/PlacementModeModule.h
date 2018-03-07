// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPlacementModeModule.h"

#include "Framework/MultiBox/MultiBoxExtender.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlacementModeCategoryRefreshed, FName /*CategoryName*/)

struct FPlacementCategory : FPlacementCategoryInfo
{
	FPlacementCategory(const FPlacementCategoryInfo& SourceInfo)
		: FPlacementCategoryInfo(SourceInfo)
	{

	}

	FPlacementCategory(FPlacementCategory&& In)
		: FPlacementCategoryInfo(MoveTemp(In))
		, Items(MoveTemp(In.Items))
	{}

	FPlacementCategory& operator=(FPlacementCategory&& In)
	{
		FPlacementCategoryInfo::operator=(MoveTemp(In));
		Items = MoveTemp(In.Items);
		return *this;
	}

	TMap<FGuid, TSharedPtr<FPlaceableItem>> Items;
};

static TOptional<FLinearColor> GetBasicShapeColorOverride();

class FPlacementModeModule : public IPlacementModeModule
{
public:

	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void PreUnloadCallback() override;

	DECLARE_DERIVED_EVENT(FPlacementModeModule, IPlacementModeModule::FOnRecentlyPlacedChanged, FOnRecentlyPlacedChanged);
	virtual FOnRecentlyPlacedChanged& OnRecentlyPlacedChanged() override { return RecentlyPlacedChanged; }

	DECLARE_DERIVED_EVENT(FPlacementModeModule, IPlacementModeModule::FOnAllPlaceableAssetsChanged, FOnAllPlaceableAssetsChanged);
	virtual FOnAllPlaceableAssetsChanged& OnAllPlaceableAssetsChanged() override { return AllPlaceableAssetsChanged; }

	FOnPlacementModeCategoryRefreshed& OnPlacementModeCategoryRefreshed() { return PlacementModeCategoryRefreshed; }

	void BroadcastPlacementModeCategoryRefreshed(FName CategoryName) { PlacementModeCategoryRefreshed.Broadcast(CategoryName); }

	/**
	 * Add the specified assets to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced(const TArray< UObject* >& PlacedObjects, UActorFactory* FactoryUsed = NULL) override;

	void OnAssetRemoved(const FAssetData& /*InRemovedAssetData*/);

	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	void OnAssetAdded(const FAssetData& AssetData);

	/**
	 * Add the specified asset to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced(UObject* Asset, UActorFactory* FactoryUsed = NULL) override;

	/**
	 * Get a copy of the recently placed items list
	 */
	virtual const TArray< FActorPlacementInfo >& GetRecentlyPlaced() const override
	{
		return RecentlyPlaced;
	}

	/** @return the event that is broadcast whenever the placement mode enters a placing session */
	DECLARE_DERIVED_EVENT(FPlacementModeModule, IPlacementModeModule::FOnStartedPlacingEvent, FOnStartedPlacingEvent);
	virtual FOnStartedPlacingEvent& OnStartedPlacing() override
	{
		return StartedPlacingEvent;
	}
	virtual void BroadcastStartedPlacing(const TArray< UObject* >& Assets) override
	{
		StartedPlacingEvent.Broadcast(Assets);
	}

	/** @return the event that is broadcast whenever the placement mode exits a placing session */
	DECLARE_DERIVED_EVENT(FPlacementModeModule, IPlacementModeModule::FOnStoppedPlacingEvent, FOnStoppedPlacingEvent);
	virtual FOnStoppedPlacingEvent& OnStoppedPlacing() override
	{
		return StoppedPlacingEvent;
	}
	virtual void BroadcastStoppedPlacing(bool bWasSuccessfullyPlaced) override
	{
		StoppedPlacingEvent.Broadcast(bWasSuccessfullyPlaced);
	}

public:

	virtual bool RegisterPlacementCategory(const FPlacementCategoryInfo& Info);

	virtual const FPlacementCategoryInfo* GetRegisteredPlacementCategory(FName CategoryName) const override
	{
		return Categories.Find(CategoryName);
	}

	virtual void UnregisterPlacementCategory(FName Handle);

	virtual void GetSortedCategories(TArray<FPlacementCategoryInfo>& OutCategories) const;

	virtual TOptional<FPlacementModeID> RegisterPlaceableItem(FName CategoryName, const TSharedRef<FPlaceableItem>& InItem);

	virtual void UnregisterPlaceableItem(FPlacementModeID ID);

	virtual void GetItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems) const;

	virtual void GetFilteredItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems, TFunctionRef<bool(const TSharedPtr<FPlaceableItem>&)> Filter) const;

	virtual void RegenerateItemsForCategory(FName Category) override;

private:

	void RefreshRecentlyPlaced();

	void RefreshVolumes();

	void RefreshAllPlaceableClasses();

	FGuid CreateID();

	FPlacementModeID CreateID(FName InCategory);

private:

	TMap<FName, FPlacementCategory> Categories;

	TArray< FActorPlacementInfo >	RecentlyPlaced;
	FOnRecentlyPlacedChanged		RecentlyPlacedChanged;

	FOnAllPlaceableAssetsChanged	AllPlaceableAssetsChanged;
	FOnPlacementModeCategoryRefreshed PlacementModeCategoryRefreshed;

	FOnStartedPlacingEvent			StartedPlacingEvent;
	FOnStoppedPlacingEvent			StoppedPlacingEvent;

	TArray< TSharedPtr<FExtender> > ContentPaletteFiltersExtenders;
	TArray< TSharedPtr<FExtender> > PaletteExtenders;
};

IMPLEMENT_MODULE(FPlacementModeModule, PlacementMode);