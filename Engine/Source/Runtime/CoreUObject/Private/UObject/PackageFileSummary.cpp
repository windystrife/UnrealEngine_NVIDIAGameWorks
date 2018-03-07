// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/PackageFileSummary.h"
#include "UObject/Linker.h"

FPackageFileSummary::FPackageFileSummary()
{
	FMemory::Memzero( this, sizeof(*this) );
}

/** Converts file version to custom version system version */
static ECustomVersionSerializationFormat::Type GetCustomVersionFormatForArchive(int32 LegacyFileVersion)
{
	ECustomVersionSerializationFormat::Type CustomVersionFormat = ECustomVersionSerializationFormat::Unknown;
	if (LegacyFileVersion == -2)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Enums;
	}
	else if (LegacyFileVersion < -2 && LegacyFileVersion >= -5)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Guids;
	}
	else if (LegacyFileVersion < -5)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Optimized;
	}
	check(CustomVersionFormat != ECustomVersionSerializationFormat::Unknown);
	return CustomVersionFormat;
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	bool bCanStartSerializing = true;
	int64 ArchiveSize = 0;
	if (Ar.IsLoading())
	{
		// Sanity checks before we even start serializing the archive
		ArchiveSize = Ar.TotalSize();
		const int64 MinimumPackageSize = 32; // That should get us safely to Sum.TotalHeaderSize
		bCanStartSerializing = ArchiveSize >= MinimumPackageSize;
		UE_CLOG(!bCanStartSerializing, LogLinker, Warning,
			TEXT("Failed to read package file summary, the file \"%s\" is too small (%lld bytes, expected at least %lld bytes)"),
			*Ar.GetArchiveName(), ArchiveSize, MinimumPackageSize);
	}
	if (bCanStartSerializing)
	{
		Ar << Sum.Tag;
	}
	// only keep loading if we match the magic
	if( Sum.Tag == PACKAGE_FILE_TAG || Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
	{
		// The package has been stored in a separate endianness than the linker expected so we need to force
		// endian conversion. Latent handling allows the PC version to retrieve information about cooked packages.
		if( Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
		{
			// Set proper tag.
			Sum.Tag = PACKAGE_FILE_TAG;
			// Toggle forced byte swapping.
			if( Ar.ForceByteSwapping() )
			{
				Ar.SetByteSwapping( false );
			}
			else
			{
				Ar.SetByteSwapping( true );
			}
		}
		/**
		 * The package file version number when this package was saved.
		 *
		 * Lower 16 bits stores the UE3 engine version
		 * Upper 16 bits stores the UE4/licensee version
		 * For newer packages this is -7
		 *		-2 indicates presence of enum-based custom versions
		 *		-3 indicates guid-based custom versions
		 *		-4 indicates removal of the UE3 version. Packages saved with this ID cannot be loaded in older engine versions 
		 *		-5 indicates the replacement of writing out the "UE3 version" so older versions of engine can gracefully fail to open newer packages
		 *		-6 indicates optimizations to how custom versions are being serialized
		 *		-7 indicates the texture allocation info has been removed from the summary
		 */
		const int32 CurrentLegacyFileVersion = -7;
		int32 LegacyFileVersion = CurrentLegacyFileVersion;
		Ar << LegacyFileVersion;

		if (Ar.IsLoading())
		{
			if (LegacyFileVersion < 0) // means we have modern version numbers
			{
				if (LegacyFileVersion < CurrentLegacyFileVersion)
				{
					// we can't safely load more than this because the legacy version code differs in ways we can not predict.
					// Make sure that the linker will fail to load with it.
					Sum.FileVersionUE4 = 0;
					Sum.FileVersionLicenseeUE4 = 0;
					return Ar;
				}

				if (LegacyFileVersion != -4)
				{
					int32 LegacyUE3Version = 0;
					Ar << LegacyUE3Version;
				}
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;

				if (LegacyFileVersion <= -2)
				{
					Sum.CustomVersionContainer.Serialize(Ar, GetCustomVersionFormatForArchive(LegacyFileVersion));
				}

				if (!Sum.FileVersionUE4 && !Sum.FileVersionLicenseeUE4)
				{
#if WITH_EDITOR
					// the editor cannot safely load unversioned content
					UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" is unversioned and we cannot safely load unversioned files in the editor."), *Ar.GetArchiveName());
					return Ar;
#else
					// this file is unversioned, remember that, then use current versions
					Sum.bUnversioned = true;
					Sum.FileVersionUE4 = GPackageFileUE4Version;
					Sum.FileVersionLicenseeUE4 = GPackageFileLicenseeUE4Version;
					Sum.CustomVersionContainer = FCustomVersionContainer::GetRegistered();
#endif
				}
			}
			else
			{
				// This is probably an old UE3 file, make sure that the linker will fail to load with it.
				Sum.FileVersionUE4 = 0;
				Sum.FileVersionLicenseeUE4 = 0;
			}
		}
		else
		{
			if (Sum.bUnversioned)
			{
				int32 Zero = 0;
				Ar << Zero; // LegacyUE3version
				Ar << Zero; // VersionUE4
				Ar << Zero; // VersionLicenseeUE4

				FCustomVersionContainer NoCustomVersions;
				NoCustomVersions.Serialize(Ar);
			}
			else
			{
				// Must write out the last UE3 engine version, so that older versions identify it as new
				int32 LegacyUE3Version = 864;
				Ar << LegacyUE3Version;
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;

				// Serialise custom version map.
				Sum.CustomVersionContainer.Serialize(Ar);
			}
		}
		Ar << Sum.TotalHeaderSize;
		Ar << Sum.FolderName;
		Ar << Sum.PackageFlags;

#if WITH_EDITOR
		if (Ar.IsLoading())
		{
			// This flag should never be saved and its reused, so we need to make sure it hasn't been loaded.
			Sum.PackageFlags &= ~PKG_NewlyCreated;
		}
#endif // WITH_EDITOR

		if( Sum.PackageFlags & PKG_FilterEditorOnly )
		{
			Ar.SetFilterEditorOnly(true);
		}
		Ar << Sum.NameCount					<< Sum.NameOffset;
		if (Sum.FileVersionUE4 >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		{
			Ar << Sum.GatherableTextDataCount	<< Sum.GatherableTextDataOffset;
		}
		Ar << Sum.ExportCount				<< Sum.ExportOffset;
		Ar << Sum.ImportCount				<< Sum.ImportOffset;
		Ar << Sum.DependsOffset;

		if (Ar.IsLoading() && (Sum.FileVersionUE4 < VER_UE4_OLDEST_LOADABLE_PACKAGE || Sum.FileVersionUE4 > GPackageFileUE4Version))
		{
			return Ar; // we can't safely load more than this because the below was different in older files.
		}

		if (Ar.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		{
			Ar << Sum.SoftPackageReferencesCount << Sum.SoftPackageReferencesOffset;
		}

		if (Ar.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADDED_SEARCHABLE_NAMES)
		{
			Ar << Sum.SearchableNamesOffset;
		}

		Ar << Sum.ThumbnailTableOffset;

		int32 GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		if( Ar.IsLoading() && GenerationCount > 0 )
		{
			Sum.Generations.Empty( 1 );
			Sum.Generations.AddUninitialized( GenerationCount );
		}
		for( int32 i=0; i<GenerationCount; i++ )
		{
			Sum.Generations[i].Serialize(Ar, Sum);
		}

		if( Sum.GetFileVersionUE4() >= VER_UE4_ENGINE_VERSION_OBJECT )
		{
			if(Ar.IsCooking() || (Ar.IsSaving() && !FEngineVersion::Current().HasChangelist()))
			{
				FEngineVersion EmptyEngineVersion;
				Ar << EmptyEngineVersion;
			}
			else
			{
				Ar << Sum.SavedByEngineVersion;
			}
		}
		else
		{
			int32 EngineChangelist = 0;
			Ar << EngineChangelist;

			if(Ar.IsLoading() && EngineChangelist != 0)
			{
				Sum.SavedByEngineVersion.Set(4, 0, 0, EngineChangelist, TEXT(""));
			}
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION )
		{
			if(Ar.IsCooking() || (Ar.IsSaving() && !FEngineVersion::Current().HasChangelist()))
			{
				FEngineVersion EmptyEngineVersion;
				Ar << EmptyEngineVersion;
			}
			else
			{
				Ar << Sum.CompatibleWithEngineVersion;
			}
		}
		else
		{
			if (Ar.IsLoading())
			{
				Sum.CompatibleWithEngineVersion = Sum.SavedByEngineVersion;
			}
		}

		Ar << Sum.CompressionFlags;
		if (!FCompression::VerifyCompressionFlagsValid(Sum.CompressionFlags))
		{
			UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" has invalid compression flags (%d)."), *Ar.GetArchiveName(), Sum.CompressionFlags);
			Sum.FileVersionUE4 = VER_UE4_OLDEST_LOADABLE_PACKAGE - 1;
			return Ar;
		}

		TArray<FCompressedChunk> CompressedChunks;
		Ar << CompressedChunks;

		if (CompressedChunks.Num())
		{
			// this file has package level compression, we won't load it.
			UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" is has package level compression (and is probably cooked). These old files cannot be loaded in the editor."), *Ar.GetArchiveName());
			Sum.FileVersionUE4 = VER_UE4_OLDEST_LOADABLE_PACKAGE - 1;
			return Ar; // we can't safely load more than this because we just changed the version to something it is not.
		}

		Ar << Sum.PackageSource;

		// No longer used: List of additional packages that are needed to be cooked for this package (ie streaming levels)
		// Keeping the serialization code for backwards compatibility without bumping the package version
		TArray<FString>	AdditionalPackagesToCook;
		Ar << AdditionalPackagesToCook;

		if (LegacyFileVersion > -7)
		{
			int32 NumTextureAllocations = 0;
			Ar << NumTextureAllocations;
			// We haven't used texture allocation info for ages and it's no longer supported anyway
			check(NumTextureAllocations == 0);
		}

		Ar << Sum.AssetRegistryDataOffset;
		Ar << Sum.BulkDataStartOffset;
		
		if (Sum.GetFileVersionUE4() >= VER_UE4_WORLD_LEVEL_INFO)
		{
			Ar << Sum.WorldTileInfoDataOffset;
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS)
		{
			Ar << Sum.ChunkIDs;
		}
		else if (Sum.GetFileVersionUE4() >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE)
		{
			// handle conversion of single ChunkID to an array of ChunkIDs
			if (Ar.IsLoading())
			{
				int ChunkID = -1;
				Ar << ChunkID;

				// don't load <0 entries since an empty array represents the same thing now
				if (ChunkID >= 0)
				{
					Sum.ChunkIDs.Add( ChunkID );
				}
			}
		}
		if (Ar.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
		{
			Ar << Sum.PreloadDependencyCount << Sum.PreloadDependencyOffset;
		}
		else
		{
			Sum.PreloadDependencyCount = -1;
			Sum.PreloadDependencyOffset = 0;
		}
	}

	return Ar;
}
