// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetData.h"
#include "AssetRenameManager.h"
#include "AssetTools.generated.h"

class FAssetFixUpRedirectors;
class FMenuBuilder;
class IClassTypeActions;
class UAutomatedAssetImportData;
class UFactory;

/** Parameters for importing specific set of files */
struct FAssetImportParams
{
	FAssetImportParams()
		: SpecifiedFactory(nullptr)
		, ImportData(nullptr)
		, bSyncToBrowser(true)
		, bForceOverrideExisting(false)
		, bAutomated(false)
	{}

	/** Factory to use for importing files */
	UFactory* SpecifiedFactory;
	/** Data used to determine rules for importing assets through the automated command line interface */
	const UAutomatedAssetImportData* ImportData;
	/** Whether or not to sync the content browser to the assets after import */
	bool bSyncToBrowser : 1;
	/** Whether or not we are forcing existing assets to be overriden without asking */
	bool bForceOverrideExisting : 1;
	/** Whether or not this is an automated import */
	bool bAutomated : 1;
};


/** For backwards compatibility */
typedef class UAssetToolsImpl FAssetTools;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UCLASS(transient)
class UAssetToolsImpl : public UObject, public IAssetTools
{
	GENERATED_BODY()
public:
	UAssetToolsImpl(const FObjectInitializer& ObjectInitializer);

	// IAssetTools implementation
	virtual void RegisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& NewActions) override;
	virtual void UnregisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& ActionsToRemove) override;
	virtual void GetAssetTypeActionsList( TArray<TWeakPtr<IAssetTypeActions>>& OutAssetTypeActionsList ) const override;
	virtual TWeakPtr<IAssetTypeActions> GetAssetTypeActionsForClass( UClass* Class ) const override;
	virtual EAssetTypeCategories::Type RegisterAdvancedAssetCategory(FName CategoryKey, FText CategoryDisplayName) override;
	virtual EAssetTypeCategories::Type FindAdvancedAssetCategory(FName CategoryKey) const override;
	virtual void GetAllAdvancedAssetCategories(TArray<FAdvancedAssetCategory>& OutCategoryList) const override;
	virtual void RegisterClassTypeActions(const TSharedRef<IClassTypeActions>& NewActions) override;
	virtual void UnregisterClassTypeActions(const TSharedRef<IClassTypeActions>& ActionsToRemove) override;
	virtual void GetClassTypeActionsList( TArray<TWeakPtr<IClassTypeActions>>& OutClassTypeActionsList ) const override;
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActionsForClass( UClass* Class ) const override;
	virtual bool GetAssetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder, bool bIncludeHeading = true ) override;
	virtual UObject* CreateAsset(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) override;
	virtual UObject* CreateAsset(UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) override;
	virtual UObject* CreateAssetWithDialog(UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) override;
	virtual UObject* CreateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) override;
	virtual UObject* DuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) override;
	virtual UObject* DuplicateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) override;
	virtual void RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames) const override;
	virtual void FindSoftReferencesToObject(FSoftObjectPath TargetObject, TArray<UObject*>& ReferencingObjects) const override;
	virtual TArray<UObject*> ImportAssets(const FString& DestinationPath) override;
	virtual TArray<UObject*> ImportAssetsWithDialog(const FString& DestinationPath) override;
	virtual TArray<UObject*> ImportAssets(const TArray<FString>& Files, const FString& DestinationPath, UFactory* ChosenFactory, bool bSyncToBrowser = true, TArray<TPair<FString, FString>>* FilesAndDestinations = nullptr) const override;
	virtual TArray<UObject*> ImportAssetsAutomated(const UAutomatedAssetImportData* ImportData) const override;
	virtual void ExportAssets(const TArray<FString>& AssetsToExport, const FString& ExportPath) const override;
	virtual void ExportAssets(const TArray<UObject*>& AssetsToExport, const FString& ExportPath) const override;
	virtual void ExportAssetsWithDialog(const TArray<UObject*>& AssetsToExport, bool bPromptForIndividualFilenames) const override;
	virtual void ExportAssetsWithDialog(const TArray<FString>& AssetsToExport, bool bPromptForIndividualFilenames) const override;

	virtual void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName) const override;
	virtual bool AssetUsesGenericThumbnail( const FAssetData& AssetData ) const override;
	virtual void DiffAgainstDepot(UObject* InObject, const FString& InPackagePath, const FString& InPackageName) const override;
	virtual void DiffAssets(UObject* OldAsset1, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual FString DumpAssetToTempFile(UObject* Asset) const override;
	virtual bool CreateDiffProcess(const FString& DiffCommand, const FString& OldTextFilename, const FString& NewTextFilename, const FString& DiffArgs = FString("")) const override;
	virtual void MigratePackages(const TArray<FName>& PackageNamesToMigrate) const override;
	virtual void FixupReferencers(const TArray<UObjectRedirector*>& Objects) const override;
	virtual FAssetPostRenameEvent& OnAssetPostRename() override { return AssetRenameManager->OnAssetPostRenameEvent(); }
	virtual void ExpandDirectories(const TArray<FString>& Files, const FString& DestinationPath, TArray<TPair<FString, FString>>& FilesAndDestinations) const override;
public:
	/** Gets the asset tools singleton as a FAssetTools for asset tools module use */
	static UAssetToolsImpl& Get();

	/** Syncs the primary content browser to the specified assets, whether or not it is locked. Most syncs that come from AssetTools -feel- like they came from the content browser, so this is okay. */
	void SyncBrowserToAssets(const TArray<UObject*>& AssetsToSync);
	void SyncBrowserToAssets(const TArray<FAssetData>& AssetsToSync);

	/** The manager to handle renaming assets */
	TSharedPtr<FAssetRenameManager> AssetRenameManager;

	/** The manager to handle fixing up redirectors */
	TSharedPtr<FAssetFixUpRedirectors> AssetFixUpRedirectors;
private:
	/** Checks to see if a package is marked for delete then ask the user if he would like to check in the deleted file before he can continue. Returns true when it is safe to proceed. */
	bool CheckForDeletedPackage(const UPackage* Package) const;

	/** Returns true if the supplied Asset name and package are currently valid for creation. */
	bool CanCreateAsset(const FString& AssetName, const FString& PackageName, const FText& OperationText) const;

	/** Begins the package migration, after assets have been discovered */
	void PerformMigratePackages(TArray<FName> PackageNamesToMigrate) const;

	/** Copies files after the final list was confirmed */
	void MigratePackages_ReportConfirmed(TArray<FString> ConfirmedPackageNamesToMigrate) const;

	/** Gets the dependencies of the specified package recursively */
	void RecursiveGetDependencies(const FName& PackageName, TSet<FName>& AllDependencies) const;

	/** Records the time taken for an import and reports it to engine analytics, if available */
	static void OnNewImportRecord(UClass* AssetType, const FString& FileExtension, bool bSucceeded, bool bWasCancelled, const FDateTime& StartTime);

	/** Records what assets users are creating */
	static void OnNewCreateRecord(UClass* AssetType, bool bDuplicated);

	/** Internal method that performs the actual asset importing */
	TArray<UObject*> ImportAssetsInternal(const TArray<FString>& Files, const FString& RootDestinationPath, TArray<TPair<FString, FString>> *FilesAndDestinationsPtr, const FAssetImportParams& ImportParams) const;

	/** Internal method to export assets.  If no export path is created a user will be prompted for one.  if bPromptIndividualFilenames is true a user will be asked per file */
	void ExportAssetsInternal(const TArray<UObject*>& ObjectsToExport, bool bPromptIndividualFilenames, const FString& ExportPath) const;

private:
	/** The list of all registered AssetTypeActions */
	TArray<TSharedRef<IAssetTypeActions>> AssetTypeActionsList;

	/** The list of all registered ClassTypeActions */
	TArray<TSharedRef<IClassTypeActions>> ClassTypeActionsList;

	/** The categories that have been allocated already */
	TMap<FName, FAdvancedAssetCategory> AllocatedCategoryBits;
	
	/** The next user category bit to allocate (set to 0 when there are no more bits left) */
	uint32 NextUserCategoryBit;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
