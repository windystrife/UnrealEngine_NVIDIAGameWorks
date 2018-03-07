// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/ObjectResource.h"
#include "UObject/PackageFileSummary.h"

struct FAssetData;
class FPackageDependencyData;

class FPackageReader : public FArchiveUObject
{
public:
	FPackageReader();
	~FPackageReader();

	enum class EOpenPackageResult : uint8
	{
		Success,
		NoLoader,
		MalformedTag,
		VersionTooOld,
		VersionTooNew,
		CustomVersionMissing,
	};

	/** Creates a loader for the filename */
	bool OpenPackageFile(const FString& PackageFilename, EOpenPackageResult* OutErrorCode = nullptr);
	bool OpenPackageFile(FArchive* Loader, EOpenPackageResult* OutErrorCode = nullptr);
	bool OpenPackageFile(EOpenPackageResult* OutErrorCode = nullptr);

	/** Reads information from the asset registry data table and converts it to FAssetData */
	bool ReadAssetRegistryData(TArray<FAssetData*>& AssetDataList);

	/** Attempts to get the class name of an object from the thumbnail cache for packages older than VER_UE4_ASSET_REGISTRY_TAGS */
	bool ReadAssetDataFromThumbnailCache(TArray<FAssetData*>& AssetDataList);

	/** Creates asset data reconstructing all the required data from cooked package info */
	bool ReadAssetRegistryDataIfCookedPackage(TArray<FAssetData*>& AssetDataList, TArray<FString>& CookedPackageNamesWithoutAssetData);

	/** Reads information used by the dependency graph */
	bool ReadDependencyData(FPackageDependencyData& OutDependencyData);

	/** Serializers for different package maps */
	void SerializeNameMap();
	void SerializeImportMap(TArray<FObjectImport>& OutImportMap);
	void SerializeExportMap(TArray<FObjectExport>& OutExportMap);
	void SerializeSoftPackageReferenceList(TArray<FName>& OutSoftPackageReferenceList);
	void SerializeSearchableNamesMap(FPackageDependencyData& OutDependencyData);

	/** Returns flags the asset package was saved with */
	uint32 GetPackageFlags() const;

	// Farchive implementation to redirect requests to the Loader
	void Serialize( void* V, int64 Length );
	bool Precache( int64 PrecacheOffset, int64 PrecacheSize );
	void Seek( int64 InPos );
	int64 Tell();
	int64 TotalSize();
	FArchive& operator<<( FName& Name );
	virtual FString GetArchiveName() const override
	{
		return PackageFilename;
	}

private:
	FString PackageFilename;
	FArchive* Loader;
	FPackageFileSummary PackageFileSummary;
	TArray<FName> NameMap;
	int64 PackageFileSize;
};
