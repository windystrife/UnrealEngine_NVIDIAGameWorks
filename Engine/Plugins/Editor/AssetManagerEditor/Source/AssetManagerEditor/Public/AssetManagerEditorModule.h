// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/ITargetPlatform.h"
#include "AssetRegistryState.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetManagerEditor, Log, All);

DECLARE_DELEGATE_RetVal(FText, FOnGetPrimaryAssetDisplayText);
DECLARE_DELEGATE_OneParam(FOnSetPrimaryAssetType, FPrimaryAssetType);
DECLARE_DELEGATE_OneParam(FOnSetPrimaryAssetId, FPrimaryAssetId);

class SToolTip;
class SWidget;
struct FAssetData;

/**
 * The Asset Manager Editor module handles creating UI for asset management and exposes several commands
 */
class ASSETMANAGEREDITOR_API IAssetManagerEditorModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IAssetManagerEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >( "AssetManagerEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "AssetManagerEditor" );
	}

	/** Creates a simple version of a Primary Asset Type selector, not bound to a PropertyHandle */
	static TSharedRef<SWidget> MakePrimaryAssetTypeSelector(FOnGetPrimaryAssetDisplayText OnGetDisplayText, FOnSetPrimaryAssetType OnSetType, bool bAllowClear = true);

	/** Creates a simple version of a Primary Asset Id selector, not bound to a PropertyHandle */
	static TSharedRef<SWidget> MakePrimaryAssetIdSelector(FOnGetPrimaryAssetDisplayText OnGetDisplayText, FOnSetPrimaryAssetId OnSetId, bool bAllowClear = true, TArray<FPrimaryAssetType> AllowedTypes = TArray<FPrimaryAssetType>());

	/** Called to get list of valid primary asset types */
	static void GeneratePrimaryAssetTypeComboBoxStrings(TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems, bool bAllowClear);

	/** Called for asset picker to see rather to show asset */
	static bool OnShouldFilterPrimaryAsset(const FAssetData& InAssetData, TArray<FPrimaryAssetType> AllowedTypes);

	/** Custom column names */
	static const FName ResourceSizeName;
	static const FName DiskSizeName;
	static const FName ManagedResourceSizeName;
	static const FName ManagedDiskSizeName;
	static const FName TotalUsageName;
	static const FName CookRuleName;
	static const FName ChunksName;

	/** Returns the value of a "virtual" column for an asset data, this will query the AssetManager for you and takes current platform into account */
	virtual FString GetValueForCustomColumn(FAssetData& AssetData, FName ColumnName, ITargetPlatform* TargetPlatform, const FAssetRegistryState* PlatformState) = 0;

	/** Gets the list of target platforms that are available */
	virtual void GetAvailableTargetPlatforms(TArray<ITargetPlatform*>& AvailablePlatforms) = 0;

	/** Returns the asset registry state used by a specific target platform. This will load it on demand if needed */
	virtual FAssetRegistryState* GetAssetRegistryStateForTargetPlatform(ITargetPlatform* TargetPlatform) = 0;
};
