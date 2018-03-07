// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/StreamableManager.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Templates/SubclassOf.h"

#include "AsyncActionLoadPrimaryAsset.generated.h"

/** Base class of all asset manager load calls */
UCLASS(Abstract)
class UAsyncActionLoadPrimaryAssetBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Execute the actual load */
	virtual void Activate() override;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted();

	/** Specific assets requested */
	TArray<FPrimaryAssetId> AssetsToLoad;

	/** Bundle states */
	TArray<FName> LoadBundles;

	/** Bundle states */
	TArray<FName> OldBundles;

	/** Handle of load request */
	TSharedPtr<FStreamableHandle> LoadHandle;

	enum class EAssetManagerOperation : uint8
	{
		Load,
		ChangeBundleStateMatching,
		ChangeBundleStateList
	};

	/** Which operation is being run */
	EAssetManagerOperation Operation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimaryAssetLoaded, UObject*, Loaded);

UCLASS()
class UAsyncActionLoadPrimaryAsset : public UAsyncActionLoadPrimaryAssetBase
{
	GENERATED_BODY()

public:
	/** 
	 * Load a primary asset into memory. The completed delegate will go off when the load succeeds or fails, you should cast the Loaded object to verify it is the correct type.
	 * If LoadBundles is specified, those bundles are loaded along with the asset
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", Category = "AssetManager", AutoCreateRefTerm = "LoadBundles"))
	static UAsyncActionLoadPrimaryAsset* AsyncLoadPrimaryAsset(FPrimaryAssetId PrimaryAsset, const TArray<FName>& LoadBundles);

	UPROPERTY(BlueprintAssignable)
	FOnPrimaryAssetLoaded Completed;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimaryAssetClassLoaded, TSubclassOf<UObject>, Loaded);

UCLASS()
class UAsyncActionLoadPrimaryAssetClass : public UAsyncActionLoadPrimaryAssetBase
{
	GENERATED_BODY()

public:
	/** 
	 * Load a primary asset class into memory. The completed delegate will go off when the load succeeds or fails, you should cast the Loaded class to verify it is the correct type 
	 * If LoadBundles is specified, those bundles are loaded along with the asset
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "AssetManager", AutoCreateRefTerm = "LoadBundles"))
	static UAsyncActionLoadPrimaryAssetClass* AsyncLoadPrimaryAssetClass(FPrimaryAssetId PrimaryAsset, const TArray<FName>& LoadBundles);

	UPROPERTY(BlueprintAssignable)
	FOnPrimaryAssetClassLoaded Completed;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimaryAssetListLoaded, const TArray<UObject*>&, Loaded);

UCLASS()
class UAsyncActionLoadPrimaryAssetList : public UAsyncActionLoadPrimaryAssetBase
{
	GENERATED_BODY()

public:
	/** 
	 * Load a list primary assets into memory. The completed delegate will go off when the load succeeds or fails, you should cast the Loaded object list to verify it is the correct type 
	 * If LoadBundles is specified, those bundles are loaded along with the asset list
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", Category = "AssetManager", AutoCreateRefTerm = "LoadBundles"))
	static UAsyncActionLoadPrimaryAssetList* AsyncLoadPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& LoadBundles);

	UPROPERTY(BlueprintAssignable)
	FOnPrimaryAssetListLoaded Completed;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimaryAssetClassListLoaded, const TArray<TSubclassOf<UObject>>&, Loaded);

UCLASS()
class UAsyncActionLoadPrimaryAssetClassList : public UAsyncActionLoadPrimaryAssetBase
{
	GENERATED_BODY()

public:
	/** 
	 * Load a list primary asset classes into memory. The completed delegate will go off when the load succeeds or fails, you should cast the Loaded object list to verify it is the correct type 
	 * If LoadBundles is specified, those bundles are loaded along with the asset list
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "AssetManager", AutoCreateRefTerm = "LoadBundles"))
	static UAsyncActionLoadPrimaryAssetClassList* AsyncLoadPrimaryAssetClassList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& LoadBundles);

	UPROPERTY(BlueprintAssignable)
	FOnPrimaryAssetClassListLoaded Completed;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPrimaryAssetBundlesChanged);

UCLASS()
class UAsyncActionChangePrimaryAssetBundles : public UAsyncActionLoadPrimaryAssetBase
{
	GENERATED_BODY()

public:
	/**
	 * Change the bundle state of all assets that match OldBundles to instead contain NewBundles. 
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "AssetManager"))
	static UAsyncActionChangePrimaryAssetBundles* AsyncChangeBundleStateForMatchingPrimaryAssets(const TArray<FName>& NewBundles, const TArray<FName>& OldBundles);

	/**
	 * Change the bundle state of assets in PrimaryAssetList. AddBundles are added and RemoveBundles are removed, both must be filled in but an empty array is allowed
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "AssetManager"))
	static UAsyncActionChangePrimaryAssetBundles* AsyncChangeBundleStateForPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& AddBundles, const TArray<FName>& RemoveBundles);

	UPROPERTY(BlueprintAssignable)
	FOnPrimaryAssetBundlesChanged Completed;

protected:

	/** Called from asset manager */
	virtual void HandleLoadCompleted() override;
};
