// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "BlueprintNativeCodeGenManifest.generated.h"

struct FAssetData;
struct FBlueprintNativeCodeGenPaths;

/*******************************************************************************
 * FCodeGenAssetRecord
 ******************************************************************************/

USTRUCT()
struct FConvertedAssetRecord
{
	GENERATED_USTRUCT_BODY()

public:
	FConvertedAssetRecord() {}
	FConvertedAssetRecord(const FAssetData& AssetInfo, const FBlueprintNativeCodeGenPaths& TargetPaths, const FCompilerNativizationOptions& NativizationOptions);

public:
	UPROPERTY()
	UClass* AssetType;

	// cannot use a FSoftObjectPath, as the json serializer has problems 
	// with some asset paths (for example, I had a folder named 'Folder()')
	UPROPERTY()
	FString TargetObjPath; 

	UPROPERTY()
	FString GeneratedHeaderPath;

	UPROPERTY()
	FString GeneratedCppPath;
};

/*******************************************************************************
 * FUnconvertedDependencyRecord
 ******************************************************************************/

USTRUCT()
struct FUnconvertedDependencyRecord
{
	GENERATED_USTRUCT_BODY()

public:
	FUnconvertedDependencyRecord() {}
	FUnconvertedDependencyRecord(const FString& InGeneratedWrapperPath);

public:
	UPROPERTY()
	FString GeneratedWrapperPath;
};

/*******************************************************************************
 * FBlueprintNativeCodeGenPaths
 ******************************************************************************/

struct FBlueprintNativeCodeGenPaths
{
public:
	enum ESourceFileType
	{
		HFile, CppFile,
	};

	static FBlueprintNativeCodeGenPaths GetDefaultCodeGenPaths(const FName PlatformName);
	static FString GetDefaultPluginPath(const FName PlatformName);
	static FString GetDefaultManifestFilePath(const FName PlatformName, const int32 ChunkId = -1);
	
private:
	friend struct FBlueprintNativeCodeGenManifest;
	FBlueprintNativeCodeGenPaths(const FString& PluginName, const FString& TargetDir, const FName PlatformName);

public:
	/**  */
	FString ManifestFilename(const int32 ChunkId = -1) const;
	FString ManifestFilePath(const int32 ChunkId = -1) const;

	/**  */
	const FString& GetPluginName() const { return PluginName; }
	FString PluginRootDir() const;
	FString PluginFilePath() const;
	FString PluginSourceDir() const;

	/**  */
	FString RuntimeModuleDir() const;
	FString RuntimeModuleName() const;
	FString RuntimeBuildFile() const;
	FString RuntimeSourceDir(ESourceFileType SourceType) const;
	FString RuntimeModuleFile(ESourceFileType SourceType) const;
	FString RuntimePCHFilename() const;

	/** */
	FName GetTargetPlatformName() const { return PlatformName; }

private:
	FString PluginsDir;
	FString PluginName;
	FName   PlatformName;
};

/*******************************************************************************
 * FBlueprintNativeCodeGenManifest
 ******************************************************************************/
 
USTRUCT()
struct FBlueprintNativeCodeGenManifest
{
	GENERATED_USTRUCT_BODY()

	typedef FName FAssetId;
	typedef TMap<FAssetId, FConvertedAssetRecord> FConversionRecord;
	typedef TMap<FAssetId, FUnconvertedDependencyRecord> FUnconvertedRecord;

public: 
	FBlueprintNativeCodeGenManifest(int32 ManifestId = -1); // default ctor for USTRUCT() system
	FBlueprintNativeCodeGenManifest(const FString& PluginPath, const FCompilerNativizationOptions& InCompilerNativizationOptionsm, int32 ManifestId = -1);
	FBlueprintNativeCodeGenManifest(const FCompilerNativizationOptions& InCompilerNativizationOptions, int32 ManifestId = -1);
	FBlueprintNativeCodeGenManifest(const FString& ManifestFilePath);


	/**
	 * @return A utility object that you can query for various file/directory paths used/targeted by the conversion process.
	 */
	FBlueprintNativeCodeGenPaths GetTargetPaths() const;

	/**
	 * Logs an entry detailing the specified asset's conversion (the asset's 
	 * name, the resulting cpp/h files, etc.). Will return an existing entry, 
	 * if one exists for the specified asset.
	 * 
	 * @param  AssetInfo    Defines the asset that you plan on converting (and want to make an entry for).
	 * @return A record detailing the asset's expected conversion.
	 */
	FConvertedAssetRecord& CreateConversionRecord(const FAssetId Key, const FAssetData& AssetInfo);

	/**
	 * 
	 * 
	 * @param  UnconvertedAssetKey    
	 * @param  AssetObj    
	 * @param  ReferencingAsset    
	 * @return 
	 */
	FUnconvertedDependencyRecord& CreateUnconvertedDependencyRecord(const FAssetId UnconvertedAssetKey, const FAssetData& AssetInfo);

	/**
	 * Records module dependencies for the specified asset.
	 * 
	 * @param  Package    The asset you want to collect dependencies for.
	 */
	void GatherModuleDependencies(UPackage* Package);

	/**
	 * Adds module, to dependency list
	 * 
	 * @param  Package    The module you want to add.
	 */
	void AddSingleModuleDependency(UPackage* Package);

	/** 
	 * @return A list of all known modules that this plugin will depend on. 
	 */
	const TArray<UPackage*>& GetModuleDependencies() const { return ModuleDependencies; }

	/** 
	 * @return A list of all asset conversion that have been recorded. 
	 */
	const FConversionRecord& GetConversionRecord() const { return ConvertedAssets; }

	/**
	 * @return A list of all asset conversion that require stubs.
	 */
	const FUnconvertedRecord& GetUnconvertedDependencies() const { return UnconvertedDependencies; }

	/**
	 * @return compiler nativization options
	 */
	const FCompilerNativizationOptions& GetCompilerNativizationOptions() const { return NativizationOptions; }

	/**
	 * Saves this manifest as json, to its target destination (which it was 
	 * setup with).
	 * 
	 * @return True if the save was a success, otherwise false.
	 */
	bool Save() const;

	/**
	 * Merges the OtherManifest into the current manifest
	 */
	void Merge(const FBlueprintNativeCodeGenManifest& OtherManifest);

	/** */
	int32 GetManifestChunkId() const { return ManifestChunkId; }

private:
	/**
	 * 
	 */
	void InitDestPaths(const FString& PluginPath);

	/**
	 * @return The destination directory for the plugin and all its related files.
	 */
	FString GetTargetDir() const;

	/**
	 * Empties the manifest, leaving only the destination directory and file
	 * names intact.
	 */
	void Clear();

private:
	/** To uniquely identify related manifests (split between child cook processes), so the files remain distinct. */
	UPROPERTY()
	int32 ManifestChunkId;

	UPROPERTY()
	FString PluginName;

	/** Target plugin directory (relative to the project's directory) */
	UPROPERTY()
	FString OutputDir;

	UPROPERTY()
	TArray<UPackage*> ModuleDependencies;

	/**  */
	UPROPERTY()
	TMap<FName, FConvertedAssetRecord> ConvertedAssets;

	UPROPERTY()
	TMap<FName, FUnconvertedDependencyRecord> UnconvertedDependencies;

	UPROPERTY()
	FCompilerNativizationOptions NativizationOptions;
};
