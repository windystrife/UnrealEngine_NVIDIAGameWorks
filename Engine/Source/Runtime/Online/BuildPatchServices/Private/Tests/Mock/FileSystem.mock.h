// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/FileSystem.h"
#include "Tests/TestHelpers.h"
#include "Common/StatsCollector.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockFileSystem
		: public IFileSystem
	{
	public:
		typedef TTuple<double, FArchive*, FString, EFileRead> FCreateFileReader;
		typedef TTuple<double, FArchive*, FString, EFileWrite> FCreateFileWriter;
		typedef TTuple<double, FString, int64> FGetFileSize;
		typedef TTuple<double, FString, EFileAttributes> FGetFileAttributes;
		typedef TTuple<double, FString, bool> FSetReadOnly;
		typedef TTuple<double, FString, bool> FSetCompressed;
		typedef TTuple<double, FString, bool> FSetExecutable;

	public:
		virtual TUniquePtr<FArchive> CreateFileReader(const TCHAR* Filename, EFileRead ReadFlags = EFileRead::None) const override
		{
			FScopeLock ScopeLock(&ThreadLock);
			TUniquePtr<FArchive> Reader(new FMemoryReader(ReadFile));
			RxCreateFileReader.Emplace(FStatsCollector::GetSeconds(), Reader.Get(), Filename, ReadFlags);
			return Reader;
		}

		virtual TUniquePtr<FArchive> CreateFileWriter(const TCHAR* Filename, EFileWrite WriteFlags = EFileWrite::None) const override
		{
			FScopeLock ScopeLock(&ThreadLock);
			TUniquePtr<FArchive> Writer(new FMemoryWriter(WriteFile));
			RxCreateFileWriter.Emplace(FStatsCollector::GetSeconds(), Writer.Get(), Filename, WriteFlags);
			return Writer;
		}

		bool DeleteFile(const TCHAR* Filename) const
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockFileSystem::DeleteFile");
			return false;
		}

		bool MoveFile(const TCHAR* FileDest, const TCHAR* FileSource) const
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockFileSystem::MoveFile");
			return false;
		}

		virtual bool GetFileSize(const TCHAR* Filename, int64& OutFileSize) const override
		{
			OutFileSize = INDEX_NONE;
			if (FileSizes.Contains(Filename))
			{
				OutFileSize = FileSizes[Filename];
			}
			RxGetFileSize.Emplace(FStatsCollector::GetSeconds(), Filename, OutFileSize);
			return true;
		}

		virtual bool GetFileAttributes(const TCHAR* Filename, EFileAttributes& OutFileAttributes) const override
		{
			OutFileAttributes = EFileAttributes::None;
			if (FileAttributes.Contains(Filename))
			{
				OutFileAttributes = FileAttributes[Filename];
			}
			RxGetFileAttributes.Emplace(FStatsCollector::GetSeconds(), Filename, OutFileAttributes);
			return true;
		}

		virtual bool SetReadOnly(const TCHAR* Filename, bool bIsReadOnly) const override
		{
			RxSetReadOnly.Emplace(FStatsCollector::GetSeconds(), Filename, bIsReadOnly);
			return true;
		}

		virtual bool SetCompressed(const TCHAR* Filename, bool bIsCompressed) const override
		{
			RxSetCompressed.Emplace(FStatsCollector::GetSeconds(), Filename, bIsCompressed);
			return true;
		}

		virtual bool SetExecutable(const TCHAR* Filename, bool bIsExecutable) const override
		{
			RxSetExecutable.Emplace(FStatsCollector::GetSeconds(), Filename, bIsExecutable);
			return true;
		}

	public:
		mutable FCriticalSection ThreadLock;
		mutable TArray<FCreateFileReader> RxCreateFileReader;
		mutable TArray<FCreateFileWriter> RxCreateFileWriter;
		mutable TArray<FGetFileAttributes> RxGetFileAttributes;
		mutable TArray<FGetFileSize> RxGetFileSize;
		mutable TArray<FSetReadOnly> RxSetReadOnly;
		mutable TArray<FSetCompressed> RxSetCompressed;
		mutable TArray<FSetExecutable> RxSetExecutable;
		mutable TArray<uint8> ReadFile;
		mutable TArray<uint8> WriteFile;
		mutable TMap<FString, int64> FileSizes;
		mutable TMap<FString, EFileAttributes> FileAttributes;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
