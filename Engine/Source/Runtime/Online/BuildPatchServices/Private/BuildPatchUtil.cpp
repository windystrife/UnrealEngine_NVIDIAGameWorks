// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchUtil.cpp: Implements miscellaneous utility functions.
=============================================================================*/

#include "BuildPatchUtil.h"
#include "Misc/Paths.h"
#include "BuildPatchServicesModule.h"
#include "Data/ChunkData.h"
#include "BuildPatchHash.h"
#include "Common/FileSystem.h"

using namespace BuildPatchServices;

/* FBuildPatchUtils implementation
*****************************************************************************/
FString FBuildPatchUtils::GetChunkNewFilename( const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& ChunkGUID, const uint64& ChunkHash )
{
	check( ChunkGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%016llX_%s.chunk"), *EBuildPatchAppManifestVersion::GetChunkSubdir( ManifestVersion ),FCrc::MemCrc32( &ChunkGUID, sizeof( FGuid ) ) % 100, ChunkHash, *ChunkGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileNewFilename(const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& FileGUID, const FSHAHashData& FileHash)
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%s_%s.file"), *EBuildPatchAppManifestVersion::GetFileSubdir( ManifestVersion ), FCrc::MemCrc32( &FileGUID, sizeof( FGuid ) ) % 100, *FileHash.ToString(), *FileGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileNewFilename(const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& FileGUID, const uint64& FileHash)
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%016llX_%s.file"), *EBuildPatchAppManifestVersion::GetFileSubdir( ManifestVersion ), FCrc::MemCrc32( &FileGUID, sizeof( FGuid ) ) % 100, FileHash, *FileGUID.ToString() ) );
}

void FBuildPatchUtils::GetChunkDetailFromNewFilename( const FString& ChunkNewFilename, FGuid& ChunkGUID, uint64& ChunkHash )
{
	const FString ChunkFilename = FPaths::GetBaseFilename( ChunkNewFilename );
	FString GuidString;
	FString HashString;
	ChunkFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 16 );
	ChunkHash = FCString::Strtoui64( *HashString, NULL, 16 );
	FGuid::Parse( GuidString, ChunkGUID );
}

void FBuildPatchUtils::GetFileDetailFromNewFilename(const FString& FileNewFilename, FGuid& FileGUID, FSHAHashData& FileHash)
{
	const FString FileFilename = FPaths::GetBaseFilename( FileNewFilename );
	FString GuidString;
	FString HashString;
	FileFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 40 );
	HexToBytes( HashString, FileHash.Hash );
	FGuid::Parse( GuidString, FileGUID );
}

FString FBuildPatchUtils::GetChunkOldFilename( const FString& RootDirectory, const FGuid& ChunkGUID )
{
	check( ChunkGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("Chunks/%02d/%s.chunk"), FCrc::MemCrc_DEPRECATED( &ChunkGUID, sizeof( FGuid ) ) % 100, *ChunkGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileOldFilename( const FString& RootDirectory, const FGuid& FileGUID )
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("Files/%02d/%s.file"), FCrc::MemCrc_DEPRECATED( &FileGUID, sizeof( FGuid ) ) % 100, *FileGUID.ToString() ) );
}

FString FBuildPatchUtils::GetDataTypeOldFilename( EBuildPatchDataType DataType, const FString& RootDirectory, const FGuid& Guid )
{
	check( Guid.IsValid() );

	switch ( DataType )
	{
	case EBuildPatchDataType::ChunkData:
		return GetChunkOldFilename( RootDirectory, Guid );
	case EBuildPatchDataType::FileData:
		return GetFileOldFilename( RootDirectory, Guid );
	}

	// Error, didn't case type
	check( false );
	return TEXT( "" );
}

FString FBuildPatchUtils::GetDataFilename(const FBuildPatchAppManifestRef& Manifest, const FString& RootDirectory, const FGuid& DataGUID)
{
	const EBuildPatchDataType DataType = Manifest->IsFileDataManifest() ? EBuildPatchDataType::FileData : EBuildPatchDataType::ChunkData;
	if (Manifest->GetManifestVersion() < EBuildPatchAppManifestVersion::DataFileRenames)
	{
		return FBuildPatchUtils::GetDataTypeOldFilename(DataType, RootDirectory, DataGUID);
	}
	else if (!Manifest->IsFileDataManifest())
	{
		uint64 ChunkHash;
		const bool bFound = Manifest->GetChunkHash(DataGUID, ChunkHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetChunkNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, ChunkHash);
	}
	else if (Manifest->GetManifestVersion() <= EBuildPatchAppManifestVersion::StoredAsCompressedUClass)
	{
		FSHAHashData FileHash;
		const bool bFound = Manifest->GetFileHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, FileHash);
	}
	else
	{
		uint64 FileHash;
		const bool bFound = Manifest->GetFilePartHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, FileHash);
	}
	return TEXT("");
}



bool FBuildPatchUtils::GetGUIDFromFilename( const FString& DataFilename, FGuid& DataGUID )
{
	const FString DataBaseFilename = FPaths::GetBaseFilename( DataFilename );
	FString GuidString;
	if( DataBaseFilename.Contains( TEXT( "_" ) ) )
	{
		DataBaseFilename.Split( TEXT( "_" ), NULL, &GuidString );
	}
	else
	{
		GuidString = DataBaseFilename;
	}
	if(GuidString.Len() == 32)
	{
		return FGuid::Parse(GuidString, DataGUID);
	}
	return false;
}

uint8 FBuildPatchUtils::VerifyFile(IFileSystem* FileSystem, const FString& FileToVerify, const FSHAHashData& Hash1, const FSHAHashData& Hash2)
{
	FBuildPatchFloatDelegate NoProgressDelegate;
	FBuildPatchBoolRetDelegate NoPauseDelegate;
	FBuildPatchBoolRetDelegate NoAbortDelegate;
	return VerifyFile(FileSystem, FileToVerify, Hash1, Hash2, NoProgressDelegate, NoPauseDelegate, NoAbortDelegate);
}

uint8 FBuildPatchUtils::VerifyFile(IFileSystem* FileSystem, const FString& FileToVerify, const FSHAHashData& Hash1, const FSHAHashData& Hash2, FBuildPatchFloatDelegate ProgressDelegate, FBuildPatchBoolRetDelegate ShouldPauseDelegate, FBuildPatchBoolRetDelegate ShouldAbortDelegate)
{
	uint8 ReturnValue = 0;
	TUniquePtr<FArchive> FileReader = FileSystem->CreateFileReader(*FileToVerify);
	ProgressDelegate.ExecuteIfBound(0.0f);
	if (FileReader.IsValid())
	{
		FSHA1 HashState;
		FSHAHashData HashValue;
		const int64 FileSize = FileReader->TotalSize();
		uint8* FileReadBuffer = new uint8[FileBufferSize];
		while (!FileReader->AtEnd() && (!ShouldAbortDelegate.IsBound() || !ShouldAbortDelegate.Execute()))
		{
			// Pause if necessary
			while ((ShouldPauseDelegate.IsBound() && ShouldPauseDelegate.Execute())
			   && (!ShouldAbortDelegate.IsBound() || !ShouldAbortDelegate.Execute()))
			{
				FPlatformProcess::Sleep(0.1f);
			}
			// Read file and update hash state
			const int64 SizeLeft = FileSize - FileReader->Tell();
			const uint32 ReadLen = FMath::Min< int64 >(FileBufferSize, SizeLeft);
			FileReader->Serialize(FileReadBuffer, ReadLen);
			HashState.Update(FileReadBuffer, ReadLen);
			const double FileSizeTemp = FileSize;
			const float Progress = 1.0f - ((SizeLeft - ReadLen) / FileSizeTemp);
			ProgressDelegate.ExecuteIfBound(Progress);
		}
		delete[] FileReadBuffer;
		HashState.Final();
		HashState.GetHash(HashValue.Hash);
		ReturnValue = (HashValue == Hash1) ? 1 : (HashValue == Hash2) ? 2 : 0;
		if (ReturnValue == 0)
		{
			GLog->Logf(TEXT("BuildDataGenerator: Verify failed on %s"), *FileToVerify);
		}
		FileReader->Close();
	}
	else
	{
		GLog->Logf(TEXT("BuildDataGenerator: ERROR VerifyFile cannot open %s"), *FileToVerify);
	}
	ProgressDelegate.ExecuteIfBound(1.0f);
	return ReturnValue;
}

bool FBuildPatchUtils::UncompressChunkFile(TArray< uint8 >& ChunkFileArray)
{
	FMemoryReader ChunkArrayReader(ChunkFileArray);
	// Read the header
	FChunkHeader Header;
	ChunkArrayReader << Header;
	// Check header
	const bool bValidHeader = Header.Guid.IsValid();
	const bool bSupportedFormat = !(Header.StoredAs & EChunkStorageFlags::Encrypted);
	if (bValidHeader && bSupportedFormat)
	{
		bool bSuccess = true;
		// Uncompress if we need to
		if (Header.StoredAs == EChunkStorageFlags::Compressed)
		{
			// Load the compressed chunk data
			TArray< uint8 > CompressedData;
			TArray< uint8 > UncompressedData;
			CompressedData.Empty(Header.DataSize);
			CompressedData.AddUninitialized(Header.DataSize);
			UncompressedData.Empty(BuildPatchServices::ChunkDataSize);
			UncompressedData.AddUninitialized(BuildPatchServices::ChunkDataSize);
			ChunkArrayReader.Serialize(CompressedData.GetData(), Header.DataSize);
			ChunkArrayReader.Close();
			// Uncompress
			bSuccess = FCompression::UncompressMemory(
				static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				UncompressedData.GetData(),
				UncompressedData.Num(),
				CompressedData.GetData(),
				CompressedData.Num());
			// If successful, write back over the original array
			if (bSuccess)
			{
				ChunkFileArray.Empty();
				FMemoryWriter ChunkArrayWriter(ChunkFileArray);
				Header.StoredAs = EChunkStorageFlags::None;
				Header.DataSize = BuildPatchServices::ChunkDataSize;
				ChunkArrayWriter << Header;
				ChunkArrayWriter.Serialize(UncompressedData.GetData(), UncompressedData.Num());
				ChunkArrayWriter.Close();
			}
		}
		return bSuccess;
	}
	return false;
}
