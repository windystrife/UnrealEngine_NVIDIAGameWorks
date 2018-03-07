// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchDataEnumeration.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Generation/ChunkDatabaseWriter.h"
#include "Common/FileSystem.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDataEnumeration, Log, All);
DEFINE_LOG_CATEGORY(LogDataEnumeration);

namespace EnumerationHelpers
{
	template< typename DataType >
	FString ToHexString(const DataType& DataVal)
	{
		const void* AsBuffer = &DataVal;
		return BytesToHex(static_cast<const uint8*>(AsBuffer), sizeof(DataType));
	}

	bool IsChunkDbData(FArchive& Archive)
	{
		using namespace BuildPatchServices;
		const int64 ArPos = Archive.Tell();
		FChunkDatabaseHeader ChunkDbHeader;
		Archive << ChunkDbHeader;
		bool bIsChunkDb = ChunkDbHeader.Version > 0;
		Archive.Seek(ArPos);
		return bIsChunkDb;
	}

	bool EnumerateManifestData(FArchive& Archive, FString& Output, bool bIncludeSizes)
	{
		using namespace BuildPatchServices;
		bool bSuccess = false;
		TArray<FGuid> DataList;
		TArray<uint8> FileData;
		Archive.Seek(0);
		FileData.AddUninitialized(Archive.TotalSize());
		Archive.Serialize(FileData.GetData(), Archive.TotalSize());
		FBuildPatchAppManifestRef AppManifest = MakeShareable(new FBuildPatchAppManifest());
		if (AppManifest->DeserializeFromData(FileData))
		{
			AppManifest->GetDataList(DataList);
			UE_LOG(LogDataEnumeration, Verbose, TEXT("Data file list:-"));
			for (FGuid& DataGuid : DataList)
			{
				FString OutputLine = FBuildPatchUtils::GetDataFilename(AppManifest, FString(), DataGuid);
				if (bIncludeSizes)
				{
					uint64 FileSize = AppManifest->GetDataSize(DataGuid);
					OutputLine += FString::Printf(TEXT("\t%u"), FileSize);
				}
				UE_LOG(LogDataEnumeration, Verbose, TEXT("%s"), *OutputLine);
				Output += OutputLine + TEXT("\r\n");
			}
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogDataEnumeration, Error, TEXT("Failed to deserialize manifest"));
		}
		return bSuccess;
	}

	bool EnumerateChunkDbData(FArchive& Archive, FString& Output, bool bIncludeSizes)
	{
		using namespace BuildPatchServices;
		bool bSuccess = false;
		FChunkDatabaseHeader ChunkDbHeader;
		Archive << ChunkDbHeader;
		if (!Archive.IsError())
		{
			bSuccess = true;
			UE_LOG(LogDataEnumeration, Verbose, TEXT("Data file list:-"));
			for (const FChunkLocation& Location : ChunkDbHeader.Contents)
			{
				FChunkHeader ChunkHeader;
				Archive.Seek(Location.ByteStart);
				Archive << ChunkHeader;
				FString OutputLine = FString::Printf(TEXT("%s\t%s\t%s"), *Location.ChunkId.ToString(), *EnumerationHelpers::ToHexString(ChunkHeader.RollingHash), *ChunkHeader.SHAHash.ToString());
				if (bIncludeSizes)
				{
					OutputLine += FString::Printf(TEXT("\t%u"), Location.ByteSize);
				}
				UE_LOG(LogDataEnumeration, Verbose, TEXT("%s"), *OutputLine);
				Output += OutputLine + TEXT("\r\n");
				// If the header did not give valid info, mark as failed but continue.
				if (!ChunkHeader.Guid.IsValid())
				{
					UE_LOG(LogDataEnumeration, Error, TEXT("Invalid chunk header for %s at %u"), *Location.ChunkId.ToString(), Location.ByteStart);
					bSuccess = false;
				}
				// We treat a serialization error as critical, and stop reading.
				if (Archive.IsError())
				{
					UE_LOG(LogDataEnumeration, Error, TEXT("Serialization error when reading at byte %u. Aborting."), Location.ByteStart);
					bSuccess = false;
					break;
				}
			}
		}
		return bSuccess;
	}
}

bool FBuildDataEnumeration::EnumeratePatchData(const FString& InputFile, const FString& OutputFile, bool bIncludeSizes)
{
	using namespace BuildPatchServices;
	bool bSuccess = false;
	TUniquePtr<IFileSystem> FileSystem(FFileSystemFactory::Create());
	TUniquePtr<FArchive> File = FileSystem->CreateFileReader(*InputFile);
	if (File.IsValid())
	{
		FString FullList;
		bool bEnumerationOk = false;
		if (EnumerationHelpers::IsChunkDbData(*File))
		{
			bEnumerationOk = EnumerationHelpers::EnumerateChunkDbData(*File, FullList, bIncludeSizes);
		}
		else
		{
			bEnumerationOk = EnumerationHelpers::EnumerateManifestData(*File, FullList, bIncludeSizes);
		}
		if (bEnumerationOk)
		{
			if (FFileHelper::SaveStringToFile(FullList, *OutputFile))
			{
				UE_LOG(LogDataEnumeration, Log, TEXT("Saved out to %s"), *OutputFile);
				bSuccess = true;
			}
			else
			{
				UE_LOG(LogDataEnumeration, Error, TEXT("Failed to save output %s"), *OutputFile);
			}
		}
	}
	return bSuccess;
}
