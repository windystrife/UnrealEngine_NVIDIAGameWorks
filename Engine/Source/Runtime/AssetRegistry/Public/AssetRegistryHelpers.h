// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "AssetRegistryHelpers.generated.h"


USTRUCT(BlueprintType)
struct FTagAndValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Transient, Category = AssetData)
	FName Tag;

	UPROPERTY(BlueprintReadWrite, Transient, Category = AssetData)
	FString Value;
};

UCLASS(transient)
class UAssetRegistryHelpers : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Asset Registry")
	static TScriptInterface<IAssetRegistry> GetAssetRegistry();

	/** 
	 * Creates asset data from a UObject. 
	 *
	 * @param InAsset	The asset to create asset data for
	 * @param bAllowBlueprintClass	By default trying to create asset data for a blueprint class will create one for the UBlueprint instead
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static FAssetData CreateAssetData(const UObject* InAsset, bool bAllowBlueprintClass = false);

	/** Checks to see if this AssetData refers to an asset or is NULL */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static bool IsValid(const FAssetData& InAssetData);

	/** Returns true if this asset was found in a UAsset file */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static bool IsUAsset(const FAssetData& InAssetData);

	/** Returns true if the this asset is a redirector. */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static bool IsRedirector(const FAssetData& InAssetData);

	/** Returns the full name for the asset in the form: Class ObjectPath */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static FString GetFullName(const FAssetData& InAssetData);

	/** Convert to a SoftObjectPath for loading */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static FSoftObjectPath ToSoftObjectPath(const FAssetData& InAssetData);

	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static UClass* GetClass(const FAssetData& InAssetData);

	/** Returns the asset UObject if it is loaded or loads the asset if it is unloaded then returns the result */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static UObject* GetAsset(const FAssetData& InAssetData);

	/** Returns true if the asset is loaded */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static bool IsAssetLoaded(const FAssetData& InAssetData);

	/** Returns the name for the asset in the form: Class'ObjectPath' */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static FString GetExportTextName(const FAssetData& InAssetData);

	/** Gets the value associated with the given tag as a string */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static bool GetTagValue(const FAssetData& InAssetData, const FName& InTagName, FString& OutTagValue);

	/**
	 * Populates the FARFilters tags and values map with the passed in tags and values
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Registry")
	static FARFilter SetFilterTagsAndValues(const FARFilter& InFilter, const TArray<FTagAndValue>& InTagsAndValues);
};
