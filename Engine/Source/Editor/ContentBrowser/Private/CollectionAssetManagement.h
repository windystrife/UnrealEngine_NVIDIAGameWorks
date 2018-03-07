// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "CollectionManagerTypes.h"
#include "Styling/SlateTypes.h"

/** Handles the collection management for the given assets */
class FCollectionAssetManagement
{
public:
	/** Constructor */
	FCollectionAssetManagement();

	/** Destructor */
	~FCollectionAssetManagement();

	/** Set the assets that we are currently observing and managing the collection state of */
	void SetCurrentAssets(const TArray<FAssetData>& CurrentAssets);

	/** Add the current assets to the given collection */
	void AddCurrentAssetsToCollection(FCollectionNameType InCollectionKey);

	/** Remove the current assets from the given collection */
	void RemoveCurrentAssetsFromCollection(FCollectionNameType InCollectionKey);

	/** Return whether or not the given collection should be enabled in any management UIs */
	bool IsCollectionEnabled(FCollectionNameType InCollectionKey) const;

	/** Get the check box state the given collection should use in any management UIs */
	ECheckBoxState GetCollectionCheckState(FCollectionNameType InCollectionKey) const;

private:
	/** Update the internal state used to track the check box status for each collection */
	void UpdateAssetManagementState();

	/** Handles an on collection renamed event */
	void HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection);

	/** Handles an on collection updated event */
	void HandleCollectionUpdated(const FCollectionNameType& Collection);

	/** Handles an on collection destroyed event */
	void HandleCollectionDestroyed(const FCollectionNameType& Collection);

	/** Handles assets being added to a collection */
	void HandleAssetsAddedToCollection(const FCollectionNameType& Collection, const TArray<FName>& AssetsAdded);

	/** Handles assets being removed from a collection */
	void HandleAssetsRemovedFromCollection(const FCollectionNameType& Collection, const TArray<FName>& AssetsRemoved);

	/** Set of asset paths that we are currently observing and managing the collection state of */
	TSet<FName> CurrentAssetPaths;

	/** Mapping between a collection and its asset management state (based on the current assets). A missing item is assumed to be unchecked */
	TMap<FCollectionNameType, ECheckBoxState> AssetManagementState;

	/** Delegate handles */
	FDelegateHandle OnCollectionRenamedHandle;
	FDelegateHandle OnCollectionDestroyedHandle;
	FDelegateHandle OnCollectionUpdatedHandle;
	FDelegateHandle OnAssetsAddedHandle;
	FDelegateHandle OnAssetsRemovedHandle;
};
