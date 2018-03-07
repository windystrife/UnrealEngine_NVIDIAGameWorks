// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PackageReader.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "Serialization/ArchiveAsync.h"
#include "Misc/PackageName.h"
#include "AssetRegistryPrivate.h"
#include "AssetData.h"
#include "AssetRegistry.h"

FPackageReader::FPackageReader()
{
	Loader = nullptr;
	PackageFileSize = 0;
	ArIsLoading		= true;
	ArIsPersistent	= true;
}

FPackageReader::~FPackageReader()
{
	if (Loader)
	{
		delete Loader;
	}
}

bool FPackageReader::OpenPackageFile(const FString& InPackageFilename, EOpenPackageResult* OutErrorCode)
{
	PackageFilename = InPackageFilename;
	Loader = IFileManager::Get().CreateFileReader(*PackageFilename);
	return OpenPackageFile(OutErrorCode);
}

bool FPackageReader::OpenPackageFile(FArchive* InLoader, EOpenPackageResult* OutErrorCode)
{
	Loader = InLoader;
	PackageFilename = Loader->GetArchiveName();
	return OpenPackageFile(OutErrorCode);
}

bool FPackageReader::OpenPackageFile(EOpenPackageResult* OutErrorCode)
{
	auto SetPackageErrorCode = [&](const EOpenPackageResult InErrorCode)
	{
		if (OutErrorCode)
		{
			*OutErrorCode = InErrorCode;
		}
	};

	if( Loader == nullptr )
	{
		// Couldn't open the file
		SetPackageErrorCode(EOpenPackageResult::NoLoader);
		return false;
	}

	// Read package file summary from the file
	*this << PackageFileSummary;

	// Validate the summary.

	// Make sure this is indeed a package
	if( PackageFileSummary.Tag != PACKAGE_FILE_TAG )
	{
		// Unrecognized or malformed package file
		SetPackageErrorCode(EOpenPackageResult::MalformedTag);
		return false;
	}

	// Don't read packages that are too old
	if( PackageFileSummary.GetFileVersionUE4() < VER_UE4_OLDEST_LOADABLE_PACKAGE )
	{
		SetPackageErrorCode(EOpenPackageResult::VersionTooOld);
		return false;
	}

	// Don't read packages that were saved with an package version newer than the current one.
	if( (PackageFileSummary.GetFileVersionUE4() > GPackageFileUE4Version) || (PackageFileSummary.GetFileVersionLicenseeUE4() > GPackageFileLicenseeUE4Version) )
	{
		SetPackageErrorCode(EOpenPackageResult::VersionTooNew);
		return false;
	}

	// Check serialized custom versions against latest custom versions.
	const FCustomVersionContainer& LatestCustomVersions  = FCustomVersionContainer::GetRegistered();
	const FCustomVersionSet&  PackageCustomVersions = PackageFileSummary.GetCustomVersionContainer().GetAllVersions();
	for (const FCustomVersion& SerializedCustomVersion : PackageCustomVersions)
	{
		auto* LatestVersion = LatestCustomVersions.GetVersion(SerializedCustomVersion.Key);
		if (!LatestVersion)
		{
			SetPackageErrorCode(EOpenPackageResult::CustomVersionMissing);
			return false;
		}
		else if (SerializedCustomVersion.Version > LatestVersion->Version)
		{
			SetPackageErrorCode(EOpenPackageResult::VersionTooNew);
			return false;
		}
	}

	//make sure the filereader gets the correct version number (it defaults to latest version)
	SetUE4Ver(PackageFileSummary.GetFileVersionUE4());
	SetLicenseeUE4Ver(PackageFileSummary.GetFileVersionLicenseeUE4());
	SetEngineVer(PackageFileSummary.SavedByEngineVersion);
	Loader->SetUE4Ver(PackageFileSummary.GetFileVersionUE4());
	Loader->SetLicenseeUE4Ver(PackageFileSummary.GetFileVersionLicenseeUE4());
	Loader->SetEngineVer(PackageFileSummary.SavedByEngineVersion);

	const FCustomVersionContainer& PackageFileSummaryVersions = PackageFileSummary.GetCustomVersionContainer();
	SetCustomVersions(PackageFileSummaryVersions);
	Loader->SetCustomVersions(PackageFileSummaryVersions);

	PackageFileSize = Loader->TotalSize();

	SetPackageErrorCode(EOpenPackageResult::Success);
	return true;
}

bool FPackageReader::ReadAssetRegistryData (TArray<FAssetData*>& AssetDataList)
{
	check(Loader);

	// Does the package contain asset registry tags
	if( PackageFileSummary.AssetRegistryDataOffset == 0 )
	{
		// No Tag Table!
		return false;
	}

	// Seek the the part of the file where the asset registry tags live
	Seek( PackageFileSummary.AssetRegistryDataOffset );

	// Determine the package name and path
	FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	const bool bIsMapPackage = (PackageFileSummary.PackageFlags & PKG_ContainsMap) != 0;

	// Load the object count
	int32 ObjectCount = 0;
	*this << ObjectCount;

	// Worlds that were saved before they were marked public do not have asset data so we will synthesize it here to make sure we see all legacy umaps
	// We will also do this for maps saved after they were marked public but no asset data was saved for some reason. A bug caused this to happen for some maps.
	if (bIsMapPackage)
	{
		const bool bLegacyPackage = PackageFileSummary.GetFileVersionUE4() < VER_UE4_PUBLIC_WORLDS;
		const bool bNoMapAsset = (ObjectCount == 0);
		if (bLegacyPackage || bNoMapAsset)
		{
			FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
			AssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), FName(*AssetName), FName(TEXT("World")), FAssetDataTagMap(), PackageFileSummary.ChunkIDs, PackageFileSummary.PackageFlags));
		}
	}

	// UAsset files usually only have one asset, maps and redirectors have multiple
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectCount; ++ObjectIdx)
	{
		FString ObjectPath;
		FString ObjectClassName;
		int32 TagCount = 0;
		*this << ObjectPath;
		*this << ObjectClassName;
		*this << TagCount;

		FAssetDataTagMap TagsAndValues;
		TagsAndValues.Reserve(TagCount);

		for(int32 TagIdx = 0; TagIdx < TagCount; ++TagIdx)
		{
			FString Key;
			FString Value;
			*this << Key;
			*this << Value;

			if (!Key.IsEmpty() && !Value.IsEmpty())
			{
				TagsAndValues.Add(FName(*Key), Value);
			}
		}

		if (ObjectPath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
		{
			// This should never happen, it means that package A has an export with an outer of package B
			UE_ASSET_LOG(LogAssetRegistry, Warning, *PackageName, TEXT("Package has invalid export %s, resave source package!"), *ObjectPath);
			continue;
		}

		if (!ensureMsgf(!ObjectPath.Contains(TEXT("."), ESearchCase::CaseSensitive), TEXT("Cannot make FAssetData for sub object %s!"), *ObjectPath))
		{
			continue;
		}

		FString AssetName = ObjectPath;

		// Before world were RF_Public, other non-public assets were added to the asset data table in map packages.
		// Here we simply skip over them
		if ( bIsMapPackage && PackageFileSummary.GetFileVersionUE4() < VER_UE4_PUBLIC_WORLDS )
		{
			if ( AssetName != FPackageName::GetLongPackageAssetName(PackageName) )
			{
				continue;
			}
		}

		// Create a new FAssetData for this asset and update it with the gathered data
		AssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), FName(*AssetName), FName(*ObjectClassName), MoveTemp(TagsAndValues), PackageFileSummary.ChunkIDs, PackageFileSummary.PackageFlags));
	}

	return true;
}

bool FPackageReader::ReadAssetDataFromThumbnailCache(TArray<FAssetData*>& AssetDataList)
{
	check(Loader);

	// Does the package contain a thumbnail table?
	if( PackageFileSummary.ThumbnailTableOffset == 0 )
	{
		return false;
	}

	// Seek the the part of the file where the thumbnail table lives
	Seek( PackageFileSummary.ThumbnailTableOffset );

	// Determine the package name and path
	FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	// Load the thumbnail count
	int32 ObjectCount = 0;
	*this << ObjectCount;

	// Iterate over every thumbnail entry and harvest the objects classnames
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectCount; ++ObjectIdx)
	{
		// Serialize the classname
		FString AssetClassName;
		*this << AssetClassName;

		// Serialize the object path.
		FString ObjectPathWithoutPackageName;
		*this << ObjectPathWithoutPackageName;

		// Serialize the rest of the data to get at the next object
		int32 FileOffset = 0;
		*this << FileOffset;

		FString GroupNames;
		FString AssetName;

		if (!ensureMsgf(!ObjectPathWithoutPackageName.Contains(TEXT("."), ESearchCase::CaseSensitive), TEXT("Cannot make FAssetData for sub object %s!"), *ObjectPathWithoutPackageName))
		{
			continue;
		}

		// Create a new FAssetData for this asset and update it with the gathered data
		AssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectPathWithoutPackageName), FName(*AssetClassName), FAssetDataTagMap(), PackageFileSummary.ChunkIDs, PackageFileSummary.PackageFlags));
	}

	return true;
}

bool FPackageReader::ReadAssetRegistryDataIfCookedPackage(TArray<FAssetData*>& AssetDataList, TArray<FString>& CookedPackageNamesWithoutAssetData)
{
	if (!!(GetPackageFlags() & PKG_FilterEditorOnly))
	{
		const FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);
		
		bool bFoundAtLeastOneAsset = false;

		// If the packaged is saved with the right version we have the information
		// which of the objects in the export map as the asset.
		// Otherwise we need to store a temp minimal data and then force load the asset
		// to re-generate its registry data
		if (UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
		{
			const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

			TArray<FObjectImport> ImportMap;
			TArray<FObjectExport> ExportMap;
			SerializeNameMap();
			SerializeImportMap(ImportMap);
			SerializeExportMap(ExportMap);
			for (FObjectExport& Export : ExportMap)
			{
				if (Export.bIsAsset)
				{
					// We need to get the class name from the import/export maps
					FName ObjectClassName;
					if (Export.ClassIndex.IsNull())
					{
						ObjectClassName = UClass::StaticClass()->GetFName();
					}
					else if (Export.ClassIndex.IsExport())
					{
						const FObjectExport& ClassExport = ExportMap[Export.ClassIndex.ToExport()];
						ObjectClassName = ClassExport.ObjectName;
					}
					else if (Export.ClassIndex.IsImport())
					{
						const FObjectImport& ClassImport = ImportMap[Export.ClassIndex.ToImport()];
						ObjectClassName = ClassImport.ObjectName;
					}

					AssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), Export.ObjectName, ObjectClassName, FAssetDataTagMap(), TArray<int32>(), GetPackageFlags()));
					bFoundAtLeastOneAsset = true;
				}
			}
		}
		if (!bFoundAtLeastOneAsset)
		{
			CookedPackageNamesWithoutAssetData.Add(PackageName);
		}
		return true;
	}

	return false;
}

bool FPackageReader::ReadDependencyData(FPackageDependencyData& OutDependencyData)
{
	OutDependencyData.PackageName = FName(*FPackageName::FilenameToLongPackageName(PackageFilename));
	OutDependencyData.PackageData.DiskSize = PackageFileSize;
	OutDependencyData.PackageData.PackageGuid = PackageFileSummary.Guid;

	SerializeNameMap();
	SerializeImportMap(OutDependencyData.ImportMap);
	SerializeSoftPackageReferenceList(OutDependencyData.SoftPackageReferenceList);
	SerializeSearchableNamesMap(OutDependencyData);

	return true;
}

void FPackageReader::SerializeNameMap()
{
	if( PackageFileSummary.NameCount > 0 )
	{
		Seek( PackageFileSummary.NameOffset );

		for ( int32 NameMapIdx = 0; NameMapIdx < PackageFileSummary.NameCount; ++NameMapIdx )
		{
			// Read the name entry from the file.
			FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
			*this << NameEntry;

			NameMap.Add(FName(NameEntry));
		}
	}
}

void FPackageReader::SerializeImportMap(TArray<FObjectImport>& OutImportMap)
{
	if( PackageFileSummary.ImportCount > 0 )
	{
		Seek( PackageFileSummary.ImportOffset );
		for ( int32 ImportMapIdx = 0; ImportMapIdx < PackageFileSummary.ImportCount; ++ImportMapIdx )
		{
			FObjectImport* Import = new(OutImportMap)FObjectImport;
			*this << *Import;
		}
	}
}

void FPackageReader::SerializeExportMap(TArray<FObjectExport>& OutExportMap)
{
	if (PackageFileSummary.ExportCount > 0)
	{
		Seek(PackageFileSummary.ExportOffset);
		for (int32 ExportMapIdx = 0; ExportMapIdx < PackageFileSummary.ExportCount; ++ExportMapIdx)
		{
			FObjectExport* Export = new(OutExportMap)FObjectExport;
			*this << *Export;
		}
	}
}

void FPackageReader::SerializeSoftPackageReferenceList(TArray<FName>& OutSoftPackageReferenceList)
{
	if (UE4Ver() >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP && PackageFileSummary.SoftPackageReferencesOffset > 0 && PackageFileSummary.SoftPackageReferencesCount > 0)
	{
		Seek(PackageFileSummary.SoftPackageReferencesOffset);

		if (UE4Ver() < VER_UE4_ADDED_SOFT_OBJECT_PATH)
		{
			for (int32 ReferenceIdx = 0; ReferenceIdx < PackageFileSummary.SoftPackageReferencesCount; ++ReferenceIdx)
			{
				FString PackageName;
				*this << PackageName;

				if (UE4Ver() < VER_UE4_KEEP_ONLY_PACKAGE_NAMES_IN_STRING_ASSET_REFERENCES_MAP)
				{
					PackageName = FPackageName::GetNormalizedObjectPath(PackageName);
					if (!PackageName.IsEmpty())
					{
						PackageName = FPackageName::ObjectPathToPackageName(PackageName);
					}
				}

				OutSoftPackageReferenceList.Add(FName(*PackageName));
			}
		}
		else
		{
			for (int32 ReferenceIdx = 0; ReferenceIdx < PackageFileSummary.SoftPackageReferencesCount; ++ReferenceIdx)
			{
				FName PackageName;
				*this << PackageName;
				OutSoftPackageReferenceList.Add(PackageName);
			}
		}
	}
}

void FPackageReader::SerializeSearchableNamesMap(FPackageDependencyData& OutDependencyData)
{
	if (UE4Ver() >= VER_UE4_ADDED_SEARCHABLE_NAMES && PackageFileSummary.SearchableNamesOffset > 0)
	{
		Seek(PackageFileSummary.SearchableNamesOffset);

		OutDependencyData.SerializeSearchableNamesMap(*this);
	}
}

void FPackageReader::Serialize( void* V, int64 Length )
{
	check(Loader);
	Loader->Serialize( V, Length );
}

bool FPackageReader::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	check(Loader);
	return Loader->Precache( PrecacheOffset, PrecacheSize );
}

void FPackageReader::Seek( int64 InPos )
{
	check(Loader);
	Loader->Seek( InPos );
}

int64 FPackageReader::Tell()
{
	check(Loader);
	return Loader->Tell();
}

int64 FPackageReader::TotalSize()
{
	check(Loader);
	return Loader->TotalSize();
}

uint32 FPackageReader::GetPackageFlags() const
{
	return PackageFileSummary.PackageFlags;
}

FArchive& FPackageReader::operator<<( FName& Name )
{
	check(Loader);

	NAME_INDEX NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogAssetRegistry, Fatal, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
	}

	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap[NameIndex] == NAME_None)
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name and the serialized instance number
		Name = FName(NameMap[NameIndex], Number);
	}

	return *this;
}
