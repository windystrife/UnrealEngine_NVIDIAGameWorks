// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetBundleData.h"
#include "SubclassOf.h"
#include "DataAsset.generated.h"

/**
 * Base class for a simple asset containing data. The editor will list this in the content browser if you inherit from this class
 */
UCLASS(abstract, MinimalAPI)
class UDataAsset : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	// UObject interface
#if WITH_EDITORONLY_DATA
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
#endif

private:
	UPROPERTY(AssetRegistrySearchable)
	TSubclassOf<UDataAsset> NativeClass;
};

/**
 * A DataAsset that implements GetPrimaryAssetId and has asset bundle support, which makes it something that can be manually loaded/unloaded from the AssetManager
 */
UCLASS(abstract, MinimalAPI)
class UPrimaryDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// UObject interface
	ENGINE_API virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	ENGINE_API virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	/** This scans the class for AssetBundles metadata on asset properties and initializes the AssetBundleData with InitializeAssetBundlesFromMetadata */
	ENGINE_API virtual void UpdateAssetBundleData();

	/** Updates AssetBundleData */
	ENGINE_API virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;

protected:
	/** Asset Bundle data computed at save time. In cooked builds this is accessible from AssetRegistry */
	UPROPERTY()
	FAssetBundleData AssetBundleData;
#endif
};
