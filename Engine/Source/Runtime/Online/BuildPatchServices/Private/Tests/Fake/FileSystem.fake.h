// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Tests/Mock/FileSystem.mock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FFakeFileSystem
		: public FMockFileSystem
	{
	public:
		virtual TUniquePtr<FArchive> CreateFileReader(const TCHAR* Filename, EFileRead ReadFlags = EFileRead::None) const override
		{
			FString NormalizedFilename = Filename;
			FPaths::NormalizeFilename(NormalizedFilename);
			NormalizedFilename = FPaths::ConvertRelativePathToFull(TEXT(""), MoveTemp(NormalizedFilename));
			FScopeLock ScopeLock(&ThreadLock);
			TUniquePtr<FArchive> Reader(new FMemoryReader(DiskData.FindOrAdd(NormalizedFilename)));
			RxCreateFileReader.Emplace(FStatsCollector::GetSeconds(), Reader.Get(), Filename, ReadFlags);
			return Reader;
		}

		virtual TUniquePtr<FArchive> CreateFileWriter(const TCHAR* Filename, EFileWrite WriteFlags = EFileWrite::None) const override
		{
			FString NormalizedFilename = Filename;
			FPaths::NormalizeFilename(NormalizedFilename);
			NormalizedFilename = FPaths::ConvertRelativePathToFull(TEXT(""), MoveTemp(NormalizedFilename));
			FScopeLock ScopeLock(&ThreadLock);
			TUniquePtr<FArchive> Writer(new FMemoryWriter(DiskData.FindOrAdd(NormalizedFilename)));
			RxCreateFileWriter.Emplace(FStatsCollector::GetSeconds(), Writer.Get(), Filename, WriteFlags);
			return Writer;
		}

		virtual bool GetFileSize(const TCHAR* Filename, int64& OutFileSize) const override
		{
			OutFileSize = INDEX_NONE;
			if (DiskData.Contains(Filename))
			{
				OutFileSize = DiskData[Filename].Num();
			}
			RxGetFileSize.Emplace(FStatsCollector::GetSeconds(), Filename, OutFileSize);
			return true;
		}

	public:
		mutable TMap<FString, TArray<uint8>> DiskData;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
