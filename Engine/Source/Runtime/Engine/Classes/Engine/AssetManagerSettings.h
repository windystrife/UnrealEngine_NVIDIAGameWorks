// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetManagerTypes.h"
#include "Engine/DeveloperSettings.h"
#include "AssetManagerSettings.generated.h"

/** Simple structure for redirecting an old asset name/path to a new one */
USTRUCT()
struct FAssetManagerRedirect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = AssetRedirect)
	FString Old;

	UPROPERTY(EditAnywhere, Category = AssetRedirect)
	FString New;

	friend inline bool operator==(const FAssetManagerRedirect& A, const FAssetManagerRedirect& B)
	{
		return A.Old == B.Old && A.New == B.New;
	}
};

/** Simple structure to allow overriding asset rules for a specific primary asset. This can be used to set chunks */
USTRUCT()
struct FPrimaryAssetRulesOverride
{
	GENERATED_BODY()

	/** Which primary asset to override the rules for */
	UPROPERTY(EditAnywhere, Category = AssetRedirect)
	FPrimaryAssetId PrimaryAssetId;	

	/** What to overrides the rules with */
	UPROPERTY(EditAnywhere, Category = AssetRedirect, meta = (ShowOnlyInnerProperties))
	FPrimaryAssetRules Rules;
};

/** Settings for the Asset Management framework, which can be used to discover, load, and audit game-specific asset types */
UCLASS(config = Game, defaultconfig, notplaceable, meta = (DisplayName = "Asset Manager"))
class ENGINE_API UAssetManagerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAssetManagerSettings() : bOnlyCookProductionAssets(false), bShouldGuessTypeAndNameInEditor(true), bShouldAcquireMissingChunksOnLoad(false) {}

	/** List of asset types to scan at startup */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager")
	TArray<FPrimaryAssetTypeInfo> PrimaryAssetTypesToScan;

	/** List of directories to exclude from scanning for Primary Assets, useful to exclude test assets */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> DirectoriesToExclude;

	/** List of specific asset rule overrides */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager")
	TArray<FPrimaryAssetRulesOverride> PrimaryAssetRules;

	/** If true, DevelopmentCook assets will error when they are cooked */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager")
	bool bOnlyCookProductionAssets;

	/** If true, PrimaryAsset Type/Name will be implied for assets in the editor (cooked builds always must be explicit). This allows guessing for content that hasn't been resaved yet */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager")
	bool bShouldGuessTypeAndNameInEditor;

	/** If true, this will query the platform chunk install interface to request missing chunks for any requested primary asset loads */
	UPROPERTY(config, EditAnywhere, Category = "Asset Manager")
	bool bShouldAcquireMissingChunksOnLoad;

	/** Redirect from Type:Name to Type:NameNew */
	UPROPERTY(config, EditAnywhere, Category = "Redirects")
	TArray<FAssetManagerRedirect> PrimaryAssetIdRedirects;

	/** Redirect from Type to TypeNew */
	UPROPERTY(config, EditAnywhere, Category = "Redirects")
	TArray<FAssetManagerRedirect> PrimaryAssetTypeRedirects;

	/** Redirect from /game/assetpath to /game/assetpathnew */
	UPROPERTY(config, EditAnywhere, Category = "Redirects")
	TArray<FAssetManagerRedirect> AssetPathRedirects;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};