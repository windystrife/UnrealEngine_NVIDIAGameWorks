// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorPlacementInfo.h"
#include "IPlacementMode.h"
#include "GameFramework/Volume.h"

/**
 * Struct that defines an identifier for a particular placeable item in this module.
 * Only be obtainable through IPlacementModeModule::RegisterPlaceableItem
 */
class FPlacementModeID
{
public:
	FPlacementModeID(const FPlacementModeID&) = default;
	FPlacementModeID& operator=(const FPlacementModeID&) = default;

private:
	friend class FPlacementModeModule;

	FPlacementModeID() {}

	/** The category this item is held within */
	FName Category;

	/** Unique identifier (always universally unique across categories) */
	FGuid UniqueID;
};

/**
 * Struct providing information for a user category of placement objects
 */
struct FPlacementCategoryInfo
{
	FPlacementCategoryInfo(FText InDisplayName, FName InHandle, FString InTag, int32 InSortOrder = 0, bool bInSortable = true)
		: DisplayName(InDisplayName), UniqueHandle(InHandle), SortOrder(InSortOrder), TagMetaData(MoveTemp(InTag)), bSortable(bInSortable)
	{
	}

	/** This category's display name */
	FText DisplayName;

	/** A unique name for this category */
	FName UniqueHandle;

	/** Sort order for the category tab (lowest first) */
	int32 SortOrder;

	/** Optional tag meta data for the tab widget */
	FString TagMetaData;

	/** Optional generator function used to construct this category's tab content. Called when the tab is activated. */
	TFunction<TSharedRef<SWidget>()> CustomGenerator;

	/** Whether the items in this category are automatically sortable by name. False if the items are already sorted. */
	bool bSortable;
};

/**
 * Structure defining a placeable item in the placement mode panel
 */
struct FPlaceableItem
{
	/** Default constructor */
	FPlaceableItem()
		: Factory(nullptr)
	{}

	/** Constructor that takes a specific factory and asset */
	FPlaceableItem(UActorFactory* InFactory, const FAssetData& InAssetData, TOptional<int32> InSortOrder = TOptional<int32>())
		: Factory(InFactory)
		, AssetData(InAssetData)
		, SortOrder(InSortOrder)
	{
		bAlwaysUseGenericThumbnail = false;
		AutoSetDisplayName();
	}

	/** Constructor for any placeable class */
	FPlaceableItem(UClass& InAssetClass, TOptional<int32> InSortOrder = TOptional<int32>())
		: Factory(GEditor->FindActorFactoryByClass(&InAssetClass))
		, AssetData(Factory ? Factory->GetDefaultActorClass( FAssetData() ) : FAssetData())
		, SortOrder(InSortOrder)
	{
		bAlwaysUseGenericThumbnail = false;
		AutoSetDisplayName();
	}

	/** Constructor for any placeable class with associated asset data, brush and display name overrides */
	FPlaceableItem(
		UClass& InAssetClass,
		const FAssetData& InAssetData,
		FName InClassThumbnailBrushOverride = NAME_None,
		TOptional<FLinearColor> InAssetTypeColorOverride = TOptional<FLinearColor>(),
		TOptional<int32> InSortOrder = TOptional<int32>(),
		TOptional<FText> InDisplayName = TOptional<FText>()
	)
		: Factory(GEditor->FindActorFactoryByClass(&InAssetClass))
		, AssetData(InAssetData)
		, ClassThumbnailBrushOverride(InClassThumbnailBrushOverride)
		, AssetTypeColorOverride(InAssetTypeColorOverride)
		, SortOrder(InSortOrder)
	{
		bAlwaysUseGenericThumbnail = true;
		if (InDisplayName.IsSet())
		{
			DisplayName = InDisplayName.GetValue();
		}
		else
		{
			AutoSetDisplayName();
		}
	}

	/** Automatically set this item's display name from its class or asset */
	void AutoSetDisplayName()
	{
		const bool bIsClass = AssetData.GetClass() == UClass::StaticClass();
		const bool bIsVolume = bIsClass ? CastChecked<UClass>(AssetData.GetAsset())->IsChildOf(AVolume::StaticClass()) : false;
		const bool bIsActor = bIsClass ? CastChecked<UClass>(AssetData.GetAsset())->IsChildOf(AActor::StaticClass()) : false;

		if (Factory && !bIsVolume) // Factories give terrible names for volumes
		{
			DisplayName = Factory->GetDisplayName();
		}
		else if (bIsActor)
		{
			AActor* DefaultActor = CastChecked<AActor>(CastChecked<UClass>(AssetData.GetAsset())->ClassDefaultObject);
			DisplayName = FText::FromString(FName::NameToDisplayString(DefaultActor->GetClass()->GetName(), false));
		}
		else if (bIsClass)
		{
			DisplayName = FText::FromString(FName::NameToDisplayString(AssetData.AssetName.ToString(), false));
		}
		else
		{
			DisplayName = FText::FromName(AssetData.AssetName);
		}
	}

	/** The factory used to create an instance of this placeable item */
	UActorFactory* Factory;

	/** Asset data pertaining to the class */
	FAssetData AssetData;

	/** This item's display name */
	FText DisplayName;

	/** Optional override for the thumbnail brush (passed to FClassIconFinder::FindThumbnailForClass in the form ClassThumbnail.<override>) */
	FName ClassThumbnailBrushOverride;

	/** Whether to always use the generic thumbnail for this item or not */
	bool bAlwaysUseGenericThumbnail;

	/** Optional overridden color tint for the asset */
	TOptional<FLinearColor> AssetTypeColorOverride;

	/** Optional sort order (lowest first). Overrides default class name sorting. */
	TOptional<int32> SortOrder;
};

/** Structure of built-in placement categories. Defined as functions to enable external use without linkage */
struct FBuiltInPlacementCategories
{
	static FName RecentlyPlaced()	{ static FName Name("RecentlyPlaced");	return Name; }
	static FName Basic()			{ static FName Name("Basic");			return Name; }
	static FName Lights()			{ static FName Name("Lights");			return Name; }
	static FName Visual()			{ static FName Name("Visual");			return Name; }
	static FName Volumes()			{ static FName Name("Volumes");			return Name; }
	static FName AllClasses()		{ static FName Name("AllClasses");		return Name; }
};

class IPlacementModeModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IPlacementModeModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IPlacementModeModule >( "PlacementMode" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "PlacementMode" );
	}

	/**
	 * Add the specified assets to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced( const TArray< UObject* >& Assets, UActorFactory* FactoryUsed = NULL ) = 0;
	
	/**
	 * Add the specified asset to the recently placed items list
	 */
	virtual void AddToRecentlyPlaced( UObject* Asset, UActorFactory* FactoryUsed = NULL ) = 0;

	/**
	 * Get a copy of the recently placed items
	 */
	virtual const TArray< FActorPlacementInfo >& GetRecentlyPlaced() const = 0;

	/**
	 * @return the event that is broadcast whenever the list of recently placed assets changes
	 */
	DECLARE_EVENT_OneParam( IPlacementMode, FOnRecentlyPlacedChanged, const TArray< FActorPlacementInfo >& /*NewRecentlyPlaced*/ );
	virtual FOnRecentlyPlacedChanged& OnRecentlyPlacedChanged() = 0;

	/**
	 * @return the event that is broadcast whenever the list of all placeable assets changes
	 */
	DECLARE_EVENT( IPlacementMode, FOnAllPlaceableAssetsChanged );
	virtual FOnAllPlaceableAssetsChanged& OnAllPlaceableAssetsChanged() = 0;

	/**
	 * @return the event that is broadcast whenever a placement mode enters a placing session
	 */
	DECLARE_EVENT_OneParam( IPlacementMode, FOnStartedPlacingEvent, const TArray< UObject* >& /*Assets*/ );
	virtual FOnStartedPlacingEvent& OnStartedPlacing() = 0;
	virtual void BroadcastStartedPlacing( const TArray< UObject* >& Assets ) = 0;

	/**
	 * @return the event that is broadcast whenever a placement mode exits a placing session
	 */
	DECLARE_EVENT_OneParam( IPlacementMode, FOnStoppedPlacingEvent, bool /*bWasSuccessfullyPlaced*/ );
	virtual FOnStoppedPlacingEvent& OnStoppedPlacing() = 0;
	virtual void BroadcastStoppedPlacing( bool bWasSuccessfullyPlaced ) = 0;

public:

	/**
	 * Register a new category of placement items
	 *
	 * @param Info		Information pertaining to the category
	 * @return true on success, false on failure (probably if the category's unique handle is already in use)
	 */
	virtual bool RegisterPlacementCategory(const FPlacementCategoryInfo& Info) = 0;

	/**
	 * Retrieve an already registered category
	 *
	 * @param UniqueHandle	The unique handle of the category to retrieve
	 * @return Ptr to the category's information structure, or nullptr if it does not exit
	 */
	virtual const FPlacementCategoryInfo* GetRegisteredPlacementCategory(FName UniqueHandle) const = 0;

	/**
	 * Populate the specified array with all registered category information, sorted by SortOrder
	 *
	 * @param OutCategories	The array to populate with registered category inforamtion
	 */
	virtual void GetSortedCategories(TArray<FPlacementCategoryInfo>& OutCategories) const = 0;

	/**
	 * Unregister a previously registered category
	 *
	 * @param UniqueHandle	The unique handle of the category to unregister
	 */
	virtual void UnregisterPlacementCategory(FName Handle) = 0;

	/**
	 * Register a new placeable item for the specified category
	 *
	 * @param Item			The placeable item to register
	 * @param CategoryName	Unique handle to the category in which to place this item
	 * @return Optional unique identifier for the registered item, or empty on failure (if the category doesn't exist)
	 */
	virtual TOptional<FPlacementModeID> RegisterPlaceableItem(FName CategoryName, const TSharedRef<FPlaceableItem>& Item) = 0;

	/**
	 * Unregister a previously registered placeable item
	 *
	 * @param ID			Unique identifier for the item. Will have been obtained from a previous call to RegisterPlaceableItem
	 */
	virtual void UnregisterPlaceableItem(FPlacementModeID ID) = 0;

	/**
	 * Get all the items in a given category, unsorted
	 *
	 * @param Category		The unique handle of the category to get items for
	 * @param OutItems		Array to populate with the items in this category
	 */
	virtual void GetItemsForCategory(FName Category, TArray<TSharedPtr<FPlaceableItem>>& OutItems) const = 0;

	/**
	 * Get all the items in a given category, filtered by the specified predicate
	 *
	 * @param Category		The unique handle of the category to get items for
	 * @param OutItems		Array to populate with the items in this category
	 * @param Filter 		Filter predicate used to filter out items. Return true to pass the filter, false otherwise
	 */
	virtual void GetFilteredItemsForCategory(FName Category, TArray<TSharedPtr<FPlaceableItem>>& OutItems, TFunctionRef<bool(const TSharedPtr<FPlaceableItem>&)> Filter) const = 0;

	/**
	 * Instruct the category associated with the specified unique handle that it should regenerate its items
	 *
	 * @param Category		The unique handle of the category to get items for
	 */
	virtual void RegenerateItemsForCategory(FName Category) = 0;
};

