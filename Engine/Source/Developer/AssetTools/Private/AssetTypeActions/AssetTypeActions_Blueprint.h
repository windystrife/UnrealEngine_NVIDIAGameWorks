// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_ClassTypeBase.h"

struct FAssetData;
class FMenuBuilder;
class IClassTypeActions;
class UFactory;

class ASSETTOOLS_API FAssetTypeActions_Blueprint : public FAssetTypeActions_ClassTypeBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Blueprint", "Blueprint Class"); }
	virtual FColor GetTypeColor() const override { return FColor( 63, 126, 255 ); }
	virtual UClass* GetSupportedClass() const override { return UBlueprint::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool CanMerge() const override;
	virtual void Merge(UObject* InObject) override;
	virtual void Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset, const FOnMergeResolved& ResolutionCallback) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Blueprint | EAssetTypeCategories::Basic; }
	virtual void PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;

	// FAssetTypeActions_ClassTypeBase Implementation
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActions(const FAssetData& AssetData) const override;

protected:
	/** Whether or not this asset can create derived blueprints */
	virtual bool CanCreateNewDerivedBlueprint() const;

	/** Return the factory responsible for creating this type of Blueprint */
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const;

private:
	/** Handler for when EditDefaults is selected */
	void ExecuteEditDefaults(TArray<TWeakObjectPtr<UBlueprint>> Objects);

	/** Handler for when NewDerivedBlueprint is selected */
	void ExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprint> InObject);

	/** Returns true if the blueprint is data only */
	bool ShouldUseDataOnlyEditor( const UBlueprint* Blueprint ) const;

	/** Returns the tooltip to display when attempting to derive a Blueprint */
	FText GetNewDerivedBlueprintTooltip(TWeakObjectPtr<UBlueprint> InObject);

	/** Returns TRUE if you can derive a Blueprint */
	bool CanExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprint> InObject);
};
