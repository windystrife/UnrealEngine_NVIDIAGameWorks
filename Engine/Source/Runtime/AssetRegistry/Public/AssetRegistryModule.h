// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/AssetRegistryInterface.h"
#include "IAssetRegistry.h"

namespace AssetRegistryConstants
{
	const FName ModuleName("AssetRegistry");
}

/**
 * Asset registry module
 */
class FAssetRegistryModule : public IAssetRegistryInterface
{

public:

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override;

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override;

	/** Gets the asset registry singleton */
	virtual IAssetRegistry& Get() const;

	/** Tick the asset registry with the supplied timestep */
	static void TickAssetRegistry(float DeltaTime)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().Tick(DeltaTime);
	}

	/** Notifies the asset registry of a new in-memory asset */
	static void AssetCreated(UObject* NewAsset)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetCreated(NewAsset);
		}
	}

	/** Notifies the asset registry than an in-memory asset was deleted */
	static void AssetDeleted(UObject* DeletedAsset)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetDeleted(DeletedAsset);
		}
	}

	/** Notifies the asset registry that an in-memory asset was renamed */
	static void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetRenamed(RenamedAsset, OldObjectPath);
		}
	}

	/** Notifies the asset registry that an in-memory package was deleted */
	static void PackageDeleted(UPackage* DeletedPackage)
	{
		if (GIsEditor)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().PackageDeleted(DeletedPackage);
		}
	}

	/** Access the dependent package names for a given source package */
	void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) override
	{
		Get().GetDependencies(InPackageName, OutDependencies, InDependencyType);
	}

private:
	TWeakObjectPtr<class UAssetRegistryImpl> AssetRegistry;
	class FAssetRegistryConsoleCommands* ConsoleCommands;
};
