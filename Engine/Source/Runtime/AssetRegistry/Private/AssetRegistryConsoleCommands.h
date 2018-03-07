// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Runtime/AssetRegistry/Private/AssetRegistryPrivate.h"

#define LOCTEXT_NAMESPACE "AssetRegistry"

class FAssetRegistryConsoleCommands
{
public:
	const FAssetRegistryModule& Module;
	
	FAutoConsoleCommand GetByNameCommand;
	FAutoConsoleCommand GetByPathCommand;
	FAutoConsoleCommand GetByClassCommand;
	FAutoConsoleCommand GetByTagCommand;
	FAutoConsoleCommand GetDependenciesCommand;
	FAutoConsoleCommand GetReferencersCommand;
	FAutoConsoleCommand FindInvalidUAssetsCommand;

	FAssetRegistryConsoleCommands(const FAssetRegistryModule& InModule)
		: Module(InModule)
	,	GetByNameCommand(
		TEXT( "AssetRegistry.GetByName" ),
		*LOCTEXT("CommandText_GetByName", "Query the asset registry for assets matching the supplied package name").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetByName ) )
	,	GetByPathCommand(
		TEXT( "AssetRegistry.GetByPath" ),
		*LOCTEXT("CommandText_GetByPath", "Query the asset registry for assets matching the supplied package path").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetByPath ) )
	,	GetByClassCommand(
		TEXT( "AssetRegistry.GetByClass" ),
		*LOCTEXT("CommandText_GetByClass", "Query the asset registry for assets matching the supplied class").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetByClass ) )
	,	GetByTagCommand(
		TEXT( "AssetRegistry.GetByTag" ),
		*LOCTEXT("CommandText_GetByTag", "Query the asset registry for assets matching the supplied tag and value").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetByTag ) )
	,	GetDependenciesCommand(
		TEXT( "AssetRegistry.GetDependencies" ),
		*LOCTEXT("CommandText_GetDependencies", "Query the asset registry for dependencies for the specified package").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetDependencies ) )
	,	GetReferencersCommand(
		TEXT( "AssetRegistry.GetReferencers" ),
		*LOCTEXT("CommandText_GetReferencers", "Query the asset registry for referencers for the specified package").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::GetReferencers ) )
	,	FindInvalidUAssetsCommand(
		TEXT( "AssetRegistry.Debug.FindInvalidUAssets" ),
		*LOCTEXT("CommandText_FindInvalidUAssets", "Finds a list of all assets which are in UAsset files but do not share the name of the package").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FAssetRegistryConsoleCommands::FindInvalidUAssets ) )
	{}

	void GetByName(const TArray<FString>& Args)
	{
		if ( Args.Num() < 1 )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetByName PackageName"));
			return;
		}

		TArray<FAssetData> AssetData;
		const FName AssetPackageName = FName(*Args[0]);
		Module.Get().GetAssetsByPackageName(AssetPackageName, AssetData);
		UE_LOG(LogAssetRegistry, Log, TEXT("GetAssetsByPackageName for %s:"), *AssetPackageName.ToString());
		for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
		{
			AssetData[AssetIdx].PrintAssetData();
		}
	}

	void GetByPath(const TArray<FString>& Args)
	{
		if ( Args.Num() < 1 )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetByPath Path"));
			return;
		}

		TArray<FAssetData> AssetData;
		const FName AssetPath = FName(*Args[0]);
		Module.Get().GetAssetsByPath(AssetPath, AssetData);
		UE_LOG(LogAssetRegistry, Log, TEXT("GetAssetsByPath for %s:"), *AssetPath.ToString());
		for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
		{
			AssetData[AssetIdx].PrintAssetData();
		}
	}

	void GetByClass(const TArray<FString>& Args)
	{
		if ( Args.Num() < 1 )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetByClass Classname"));
			return;
		}

		TArray<FAssetData> AssetData;
		const FString Classname = Args[0];
		Module.Get().GetAssetsByClass(FName(*Classname), AssetData);
		UE_LOG(LogAssetRegistry, Log, TEXT("GetAssetsByClass for %s:"), *Classname);
		for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
		{
			AssetData[AssetIdx].PrintAssetData();
		}
	}

	void GetByTag(const TArray<FString>& Args)
	{
		if ( Args.Num() < 2 )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetByTag TagName TagValue"));
			return;
		}

		TMultiMap<FName, FString> TagsAndValues;
		TagsAndValues.Add(FName(*Args[0]), Args[1]);

		TArray<FAssetData> AssetData;
		Module.Get().GetAssetsByTagValues(TagsAndValues, AssetData);
		UE_LOG(LogAssetRegistry, Log, TEXT("GetAssetsByTagValues for Tag'%s' and Value'%s':"), *Args[0], *Args[1]);
		for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
		{
			AssetData[AssetIdx].PrintAssetData();
		}
	}

	void GetDependencies(const TArray<FString>& Args)
	{
 		if ( Args.Num() < 1 )
 		{
 			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetDependencies PackageName"));
 			return;
 		}
 
 		const FName PackageName = FName(*Args[0]);
 		TArray<FName> Dependencies;
 		
 		if ( Module.Get().GetDependencies(PackageName, Dependencies) )
 		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Dependencies for %s:"), *PackageName.ToString());
			for ( auto DependencyIt = Dependencies.CreateConstIterator(); DependencyIt; ++DependencyIt )
			{
				UE_LOG(LogAssetRegistry, Log, TEXT("   %s"), *(*DependencyIt).ToString());
			}
 		}
 		else
 		{
 			UE_LOG(LogAssetRegistry, Log, TEXT("Could not find dependency data for %s:"), *PackageName.ToString());
 		}
	}

	void GetReferencers(const TArray<FString>& Args)
	{
		if ( Args.Num() < 1 )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Usage: AssetRegistry.GetReferencers ObjectPath"));
			return;
		}

		const FName PackageName = FName(*Args[0]);
		TArray<FName> Referencers;

		if ( Module.Get().GetReferencers(PackageName, Referencers) )
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Referencers for %s:"), *PackageName.ToString());
			for ( auto ReferencerIt = Referencers.CreateConstIterator(); ReferencerIt; ++ReferencerIt )
			{
				UE_LOG(LogAssetRegistry, Log, TEXT("   %s"), *(*ReferencerIt).ToString());
			}
		}
		else
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Could not find referencer data for %s:"), *PackageName.ToString());
		}
	}

	void FindInvalidUAssets(const TArray<FString>& Args)
	{
		TArray<FAssetData> AllAssets;
		Module.Get().GetAllAssets(AllAssets);

		UE_LOG(LogAssetRegistry, Log, TEXT("Invalid UAssets:"));

		for (int32 AssetIdx = 0; AssetIdx < AllAssets.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AllAssets[AssetIdx];

			FString PackageFilename;
			if ( FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), NULL, &PackageFilename) )
			{
				if ( FPaths::GetExtension(PackageFilename, true) == FPackageName::GetAssetPackageExtension() && !AssetData.IsUAsset())
				{
					// This asset was in a package with a uasset extension but did not share the name of the package
					UE_LOG(LogAssetRegistry, Log, TEXT("%s"), *AssetData.ObjectPath.ToString());
				}
			}
		}
	}
};

#undef LOCTEXT_NAMESPACE
