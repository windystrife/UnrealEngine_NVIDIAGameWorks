// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/SecureHash.h"
#include "Common/StatsCollector.h"
#include "HAL/FileManager.h"

namespace BuildPatchServices
{
	struct FFileSpan
	{
		FFileSpan(const FString& InFilename, uint64 InSize, uint64 InStartIdx, bool InIsUnixExecutable, const FString& InSymlinkTarget)
			: Filename(InFilename)
			, Size(InSize)
			, StartIdx(InStartIdx)
			, IsUnixExecutable(InIsUnixExecutable)
			, SymlinkTarget(InSymlinkTarget)
		{
		}

		FFileSpan(const FFileSpan& CopyFrom)
			: Filename(CopyFrom.Filename)
			, Size(CopyFrom.Size)
			, StartIdx(CopyFrom.StartIdx)
			, SHAHash(CopyFrom.SHAHash)
			, IsUnixExecutable(CopyFrom.IsUnixExecutable)
			, SymlinkTarget(CopyFrom.SymlinkTarget)
		{
		}

		FFileSpan(FFileSpan&& MoveFrom)
			: Filename(MoveTemp(MoveFrom.Filename))
			, Size(MoveTemp(MoveFrom.Size))
			, StartIdx(MoveTemp(MoveFrom.StartIdx))
			, SHAHash(MoveTemp(MoveFrom.SHAHash))
			, IsUnixExecutable(MoveTemp(MoveFrom.IsUnixExecutable))
			, SymlinkTarget(MoveTemp(MoveFrom.SymlinkTarget))
		{
		}

		FFileSpan()
			: Size(0)
			, StartIdx(0)
			, IsUnixExecutable(false)
		{
		}

		FORCEINLINE FFileSpan& operator=(const FFileSpan& CopyFrom)
		{
			Filename = CopyFrom.Filename;
			Size = CopyFrom.Size;
			StartIdx = CopyFrom.StartIdx;
			SHAHash = CopyFrom.SHAHash;
			IsUnixExecutable = CopyFrom.IsUnixExecutable;
			SymlinkTarget = CopyFrom.SymlinkTarget;
			return *this;
		}

		FORCEINLINE FFileSpan& operator=(FFileSpan&& MoveFrom)
		{
			Filename = MoveTemp(MoveFrom.Filename);
			Size = MoveTemp(MoveFrom.Size);
			StartIdx = MoveTemp(MoveFrom.StartIdx);
			SHAHash = MoveTemp(MoveFrom.SHAHash);
			IsUnixExecutable = MoveTemp(MoveFrom.IsUnixExecutable);
			SymlinkTarget = MoveTemp(MoveFrom.SymlinkTarget);
			return *this;
		}

		FString Filename;
		uint64 Size;
		uint64 StartIdx;
		FSHAHash SHAHash;
		bool IsUnixExecutable;
		FString SymlinkTarget;
	};

	class FBuildStreamer
	{
	public:

		/**
		 * Fetches some data from the buffer, also removing it.
		 * @param   IN  Buffer          Pointer to buffer to receive the data.
		 * @param   IN  ReqSize         The amount of data to attempt to retrieve.
		 * @param   IN  WaitForData     Optional: Default true. Whether to wait until there is enough data in the buffer.
		 * @return the amount of data retrieved.
		 */
		virtual uint32 DequeueData(uint8* Buffer, uint32 ReqSize, bool WaitForData = true) = 0;

		/**
		 * Retrieves the file details for a specific start index.
		 * @param   IN  StartingIdx     The data index into the build image.
		 * @param   OUT FileSpan        Receives a copy of the file span data.
		 * @return true if the data byte at StartingIdx is the start of a file, false indicates that FileSpan was not set.
		 */
		virtual bool GetFileSpan(uint64 StartingIdx, FFileSpan& FileSpan) const = 0;

		/**
		 * Gets a list of empty files that the build contains.
		 * @return array of empty files in the build.
		 */
		virtual TArray<FString> GetEmptyFiles() const = 0;

		/**
		 * Gets a list of all filenames that the build contains.
		 * Will block until the list of files is enumerated and ignored files have been stripped out.
		 * @return array of filenames in the build.
		 */
		virtual TArray<FString> GetAllFilenames() const = 0;

		/**
		 * Whether the read thread has finished reading the build image.
		 * @return true if there is no more data coming into the buffer.
		 */
		virtual bool IsEndOfBuild() const = 0;

		/**
		 * Whether there is any more data available to dequeue from the buffer.
		 * @return true if there is no more data coming in, and the internal buffer is also empty.
		 */
		virtual bool IsEndOfData() const = 0;

		/**
		 * Get the total build size that was streamed.
		 * MUST be called only after IsEndOfBuild returns true.
		 * @return the number of bytes in the streamed build.
		 */
		virtual uint64 GetBuildSize() const = 0;

		/**
		 * Get the list of file spans for each file in the build, including empty files.
		 * MUST be called only after IsEndOfBuild returns true.
		 * @return the list of files in the build and their details.
		 */
		virtual TArray<FFileSpan> GetAllFiles() const = 0;
	};

	typedef TSharedRef<FBuildStreamer, ESPMode::ThreadSafe> FBuildStreamerRef;
	typedef TSharedPtr<FBuildStreamer, ESPMode::ThreadSafe> FBuildStreamerPtr;

	class FBuildStreamerFactory
	{
	public:
		static FBuildStreamerRef Create(const FString& BuildRoot, const FString& IgnoreListFile, const FStatsCollectorRef& StatsCollector, IFileManager* FileManager = &IFileManager::Get());
	};
}
