// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OutputDeviceMemory.cpp: Ring buffer (memory only) output device
=============================================================================*/

#include "Misc/OutputDeviceMemory.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceHelper.h"

#define DUMP_LOG_ON_EXIT (!NO_LOGGING && PLATFORM_DESKTOP && (!UE_BUILD_SHIPPING || USE_LOGGING_IN_SHIPPING))

FOutputDeviceMemory::FOutputDeviceMemory(int32 InPreserveSize /*= 256 * 1024*/, int32 InBufferSize /*= 2048 * 1024*/)
: ArchiveProxy(*this)
, BufferStartPos(0)
, BufferLength(0)
, PreserveSize(InPreserveSize)
{
#if DUMP_LOG_ON_EXIT
	const FString LogFileName = FPlatformOutputDevices::GetAbsoluteLogFilename();
	FOutputDeviceFile::CreateBackupCopy(*LogFileName);
	IFileManager::Get().Delete(*LogFileName);
#endif // DUMP_LOG_ON_EXIT

	checkf(InBufferSize >= InPreserveSize * 2, TEXT("FOutputDeviceMemory buffer size should be >= 2x PreserveSize"));

	Buffer.AddUninitialized(InBufferSize);
	Logf(TEXT("Log file open, %s"), FPlatformTime::StrTimestamp());
}

void FOutputDeviceMemory::TearDown() 
{
	Logf(TEXT("Log file closed, %s"), FPlatformTime::StrTimestamp());

	// Dump on exit
#if DUMP_LOG_ON_EXIT
	const FString LogFileName = FPlatformOutputDevices::GetAbsoluteLogFilename();
	FArchive* LogFile = IFileManager::Get().CreateFileWriter(*LogFileName, FILEWRITE_AllowRead);
	if (LogFile)
	{
		Dump(*LogFile);
		LogFile->Flush();
		delete LogFile;
	}
#endif // DUMP_LOG_ON_EXIT
}

void FOutputDeviceMemory::Flush()
{
	// Do nothing, buffer is always flushed
}

void FOutputDeviceMemory::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time)
{
#if !NO_LOGGING
	FOutputDeviceHelper::FormatCastAndSerializeLine(ArchiveProxy, Data, Verbosity, Category, Time, bSuppressEventTag, bAutoEmitLineTerminator);
#endif
}

void FOutputDeviceMemory::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	Serialize(Data, Verbosity, Category, -1.0);
}

void FOutputDeviceMemory::SerializeToBuffer(ANSICHAR* Data, int32 Length)
{	
	const int32 BufferCapacity = Buffer.Num(); // Never changes

	// Given the size of the buffer (usually 1MB) this should never happen
	ensure(Length <= BufferCapacity);

	while (Length)
	{
		int32 WritePos = 0;
		int32 WriteLength = 0;

		{
			// We only need to lock long enough to update all state variables. Copy doesn't need a lock, at least for large buffers.
			FScopeLock WriteLock(&BufferPosCritical);

			WritePos = BufferStartPos;

			// If this will take us past the buffer, wrap but preserve the specified amount of data at the head
			if (BufferStartPos + Length > BufferCapacity)
			{
				WriteLength = BufferCapacity - BufferStartPos;
				BufferStartPos = PreserveSize;
			}
			else
			{
				WriteLength = Length;
				BufferStartPos += WriteLength;
			}

			BufferLength = FMath::Min(BufferLength + WriteLength, BufferCapacity);
		}

		FMemory::Memcpy(Buffer.GetData() + WritePos, Data, WriteLength * sizeof(ANSICHAR));
		Length -= WriteLength;
	}
}

void FOutputDeviceMemory::Dump(FArchive& Ar)
{
	const int32 BufferCapacity = Buffer.Num(); // Never changes

	// Dump the startup logs
	Ar.Serialize(Buffer.GetData(), PreserveSize * sizeof(ANSICHAR));

	// If the log has wrapped, dump the earliest portion chronologically from the end
	if (BufferLength == BufferCapacity)
	{
		ANSICHAR* BufferStartPosPtr = Buffer.GetData() + BufferStartPos * sizeof(ANSICHAR);
		const int32 PreWrapLength = BufferCapacity - BufferStartPos;
		Ar.Serialize(BufferStartPosPtr, PreWrapLength * sizeof(ANSICHAR));
	}

	// Dump the logs from preserved section up to current, if we've made it that far
	if (BufferLength > PreserveSize)
	{
		ANSICHAR* PreservePosPtr = Buffer.GetData() + PreserveSize * sizeof(ANSICHAR);
		const int32 AfterPreserveLength = BufferStartPos - PreserveSize;
		Ar.Serialize(PreservePosPtr, AfterPreserveLength * sizeof(ANSICHAR));
	}
}
