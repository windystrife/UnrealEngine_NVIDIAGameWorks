// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformFile.h"
#include "CoreTypes.h"
#include "Misc/DateTime.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Misc/Paths.h"
#include "CoreGlobals.h"
#include "Windows/WindowsHWrapper.h"
#include <sys/utime.h>
#include "LockFreeList.h"
#include "AsyncFileHandle.h"
#include "Async/AsyncWork.h"
#include "ScopeLock.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#include "Private/Windows/WindowsAsyncIO.h"

TLockFreePointerListUnordered<void, PLATFORM_CACHE_LINE_SIZE> WindowsAsyncIOEventPool;



	namespace FileConstants
	{
		uint32 WIN_INVALID_SET_FILE_POINTER = INVALID_SET_FILE_POINTER;
	}
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	FORCEINLINE int32 UEDayOfWeekToWindowsSystemTimeDayOfWeek(const EDayOfWeek InDayOfWeek)
	{
		switch (InDayOfWeek)
		{
			case EDayOfWeek::Monday:
				return 1;
			case EDayOfWeek::Tuesday:
				return 2;
			case EDayOfWeek::Wednesday:
				return 3;
			case EDayOfWeek::Thursday:
				return 4;
			case EDayOfWeek::Friday:
				return 5;
			case EDayOfWeek::Saturday:
				return 6;
			case EDayOfWeek::Sunday:
				return 0;
			default:
				break;
		}

		return 0;
	}

	FORCEINLINE FDateTime WindowsFileTimeToUEDateTime(const FILETIME& InFileTime)
	{
		// This roundabout conversion clamps the precision of the returned time value to match that of time_t (1 second precision)
		// This avoids issues when sending files over the network via cook-on-the-fly
		SYSTEMTIME SysTime;
		if (FileTimeToSystemTime(&InFileTime, &SysTime))
		{
			return FDateTime(SysTime.wYear, SysTime.wMonth, SysTime.wDay, SysTime.wHour, SysTime.wMinute, SysTime.wSecond);
		}

		// Failed to convert
		return FDateTime::MinValue();
	}

	FORCEINLINE FILETIME UEDateTimeToWindowsFileTime(const FDateTime& InDateTime)
	{
		// This roundabout conversion clamps the precision of the returned time value to match that of time_t (1 second precision)
		// This avoids issues when sending files over the network via cook-on-the-fly
		SYSTEMTIME SysTime;
		SysTime.wYear = InDateTime.GetYear();
		SysTime.wMonth = InDateTime.GetMonth();
		SysTime.wDay = InDateTime.GetDay();
		SysTime.wDayOfWeek = UEDayOfWeekToWindowsSystemTimeDayOfWeek(InDateTime.GetDayOfWeek());
		SysTime.wHour = InDateTime.GetHour();
		SysTime.wMinute = InDateTime.GetMinute();
		SysTime.wSecond = InDateTime.GetSecond();
		SysTime.wMilliseconds = 0;

		FILETIME FileTime;
		SystemTimeToFileTime(&SysTime, &FileTime);

		return FileTime;
	}
}

/**
 * This file reader uses overlapped i/o and double buffering to asynchronously read from files
 */
class CORE_API FAsyncBufferedFileReaderWindows : public IFileHandle
{
protected:
	enum {DEFAULT_BUFFER_SIZE = 64 * 1024};
	/**
	 * The file handle to operate on
	 */
	HANDLE Handle;
	/**
	 * The size of the file that is being read
	 */
	int64 FileSize;
	/**
	 * Overall position in the file and buffers combined
	 */
	int64 FilePos;
	/**
	 * Overall position in the file as the OverlappedIO struct understands it
	 */
	uint64 OverlappedFilePos;
	/**
	 * These are the two buffers used for reading the file asynchronously
	 */
	int8* Buffers[2];
	/**
	 * The size of the buffers in bytes
	 */
	const int32 BufferSize;
	/**
	 * The current index of the buffer that we are serializing from
	 */
	int32 SerializeBuffer;
	/**
	 * The current index of the streaming buffer for async reading into
	 */
	int32 StreamBuffer;
	/**
	 * Where we are in the serialize buffer
	 */
	int32 SerializePos;
	/**
	 * Tracks which buffer has the async read outstanding (0 = first read after create/seek, 1 = streaming buffer)
	 */
	int32 CurrentAsyncReadBuffer;
	/**
	 * The overlapped IO struct to use for determining async state
	 */
	OVERLAPPED OverlappedIO;
	/**
	 * Used to track whether the last read reached the end of the file or not. Reset when a Seek happens
	 */
	bool bIsAtEOF;
	/**
	 * Whether there's a read outstanding or not
	 */
	bool bHasReadOutstanding;

	/**
	 * Closes the file handle
	 */
	bool Close(void)
	{
		if (Handle != nullptr)
		{
			// Close the file handle
			CloseHandle(Handle);
			Handle = nullptr;
		}
		return true;
	}

	/**
	 * This toggles the buffers we read into & serialize out of between buffer indices 0 & 1
	 */
	FORCEINLINE void SwapBuffers()
	{
		StreamBuffer ^= 1;
		SerializeBuffer ^= 1;
		// We are now at the beginning of the serialize buffer
		SerializePos = 0;
	}

	FORCEINLINE void CopyOverlappedPosition()
	{
		ULARGE_INTEGER LI;
		LI.QuadPart = OverlappedFilePos;
		OverlappedIO.Offset = LI.LowPart;
		OverlappedIO.OffsetHigh = LI.HighPart;
	}

	FORCEINLINE void UpdateFileOffsetAfterRead(uint32 AmountRead)
	{
		bHasReadOutstanding = false;
		OverlappedFilePos += AmountRead;
		// Update the overlapped structure since it uses this for where to read from
		CopyOverlappedPosition();
		if (OverlappedFilePos >= uint64(FileSize))
		{
			bIsAtEOF = true;
		}
	}

	bool WaitForAsyncRead()
	{
		// Check for already being at EOF because we won't issue a read
		if (bIsAtEOF || !bHasReadOutstanding)
		{
			return true;
		}
		uint32 NumRead = 0;
		if (GetOverlappedResult(Handle, &OverlappedIO, (::DWORD*)&NumRead, true) != false)
		{
			UpdateFileOffsetAfterRead(NumRead);
			return true;
		}
		else if (GetLastError() == ERROR_HANDLE_EOF)
		{
			bIsAtEOF = true;
			return true;
		}
		return false;
	}

	void StartAsyncRead(int32 BufferToReadInto)
	{
		if (!bIsAtEOF)
		{
			bHasReadOutstanding = true;
			CurrentAsyncReadBuffer = BufferToReadInto;
			uint32 NumRead = 0;
			// Now kick off an async read
			if (!ReadFile(Handle, Buffers[BufferToReadInto], BufferSize, (::DWORD*)&NumRead, &OverlappedIO))
			{
				uint32 ErrorCode = GetLastError();
				if (ErrorCode != ERROR_IO_PENDING)
				{
					bIsAtEOF = true;
					bHasReadOutstanding = false;
				}
			}
			else
			{
				// Read completed immediately
				UpdateFileOffsetAfterRead(NumRead);
			}
		}
	}

	FORCEINLINE void StartStreamBufferRead()
	{
		StartAsyncRead(StreamBuffer);
	}

	FORCEINLINE void StartSerializeBufferRead()
	{
		StartAsyncRead(SerializeBuffer);
	}

	FORCEINLINE bool IsValid()
	{
		return Handle != nullptr && Handle != INVALID_HANDLE_VALUE;
	}

public:
	FAsyncBufferedFileReaderWindows(HANDLE InHandle, int32 InBufferSize = DEFAULT_BUFFER_SIZE) :
		Handle(InHandle),
		FilePos(0),
		OverlappedFilePos(0),
		BufferSize(InBufferSize),
		SerializeBuffer(0),
		StreamBuffer(1),
		SerializePos(0),
		CurrentAsyncReadBuffer(0),
		bIsAtEOF(false),
		bHasReadOutstanding(false)
	{
		LARGE_INTEGER LI;
		GetFileSizeEx(Handle, &LI);
		FileSize = LI.QuadPart;
		// Allocate our two buffers
		Buffers[0] = (int8*)FMemory::Malloc(BufferSize);
		Buffers[1] = (int8*)FMemory::Malloc(BufferSize);

		// Zero the overlapped structure
		FMemory::Memzero(&OverlappedIO, sizeof(OVERLAPPED));

		// Kick off the first async read
		StartSerializeBufferRead();
	}

	virtual ~FAsyncBufferedFileReaderWindows(void)
	{
		// Can't free the buffers or close the file if a read is outstanding
		WaitForAsyncRead();
		Close();
		FMemory::Free(Buffers[0]);
		FMemory::Free(Buffers[1]);
	}

	virtual bool Seek(int64 InPos) override
	{
		check(IsValid());
		check(InPos >= 0);
		check(InPos <= FileSize);

		// Determine the change in locations
		int64 PosDelta = InPos - FilePos;
		if (PosDelta == 0)
		{
			// Same place so no work to do
			return true;
		}

		// No matter what, we need to wait for the current async read to finish since we most likely need to issue a new one
		if (!WaitForAsyncRead())
		{
			return false;
		}

		FilePos = InPos;

		// If the requested location is not within our current serialize buffer, we need to start the whole read process over
		bool bWithinSerializeBuffer = (PosDelta < 0 && (SerializePos - FMath::Abs(PosDelta) >= 0)) ||
			(PosDelta > 0 && ((PosDelta + SerializePos) < BufferSize));
		if (bWithinSerializeBuffer)
		{
			// Still within the serialize buffer so just update the position
			SerializePos += PosDelta;
		}
		else
		{
			// Reset our EOF tracking and let the read handle setting it if need be
			bIsAtEOF = false;
			// Not within our buffer so start a new async read on the serialize buffer
			OverlappedFilePos = InPos;
			CopyOverlappedPosition();
			CurrentAsyncReadBuffer = SerializeBuffer;
			SerializePos = 0;
			StartSerializeBufferRead();
		}
		return true;
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(IsValid());
		check(NewPositionRelativeToEnd <= 0);

		// Position is negative so this is actually subtracting
		return Seek(FileSize + NewPositionRelativeToEnd);
	}

	virtual int64 Tell(void) override
	{
		check(IsValid());
		return FilePos;
	}

	virtual int64 Size() override
	{
		check(IsValid());
		return FileSize;
	}

	virtual bool Read(uint8* Dest, int64 BytesToRead) override
	{
		check(IsValid());
		// If zero were requested, quit (some calls like to do zero sized reads)
		if (BytesToRead <= 0)
		{
			return false;
		}

		if (CurrentAsyncReadBuffer == SerializeBuffer)
		{
			// First async read after either construction or a seek
			if (!WaitForAsyncRead())
			{
				return false;
			}
			StartStreamBufferRead();
		}

		check(Dest != nullptr)
		// While there is data to copy
		while (BytesToRead > 0)
		{
			// Figure out how many bytes we can read from the serialize buffer
			int64 NumToCopy = FMath::Min<int64>(BytesToRead, BufferSize - SerializePos);
			if (FilePos + NumToCopy > FileSize)
			{
				// Tried to read past the end of the file, so fail
				return false;
			}
			// See if we are at the end of the serialize buffer or not
			if (NumToCopy > 0)
			{
				FMemory::Memcpy(Dest, &Buffers[SerializeBuffer][SerializePos], NumToCopy);

				// Update the internal positions
				SerializePos += NumToCopy;
				check(SerializePos <= BufferSize);
				FilePos += NumToCopy;
				check(FilePos <= FileSize);

				// Decrement the number of bytes we copied
				BytesToRead -= NumToCopy;

				// Now offset the dest pointer with the chunk we copied
				Dest = (uint8*)Dest + NumToCopy;
			}
			else
			{
				// We've crossed the buffer boundary and now need to make sure the stream buffer read is done
				if (!WaitForAsyncRead())
				{
					return false;
				}
				SwapBuffers();
				StartStreamBufferRead();
			}
		}
		return true;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(0 && "This is an async reader only and doesn't support writing");
		return false;
	}
};

/** 
 * Windows file handle implementation
**/

class CORE_API FFileHandleWindows : public IFileHandle
{
	enum { READWRITE_SIZE = 1024 * 1024 };
	HANDLE FileHandle;
	/** The overlapped IO struct to use for determining async state	*/
	OVERLAPPED OverlappedIO;
	/** Manages the location of our file position for setting on the overlapped struct for reads/writes */
	int64 FilePos;
	/** Need the file size for seek from end */
	int64 FileSize;

	FORCEINLINE bool IsValid()
	{
		return FileHandle != NULL && FileHandle != INVALID_HANDLE_VALUE;
	}

	FORCEINLINE void UpdateOverlappedPos()
	{
		ULARGE_INTEGER LI;
		LI.QuadPart = FilePos;
		OverlappedIO.Offset = LI.LowPart;
		OverlappedIO.OffsetHigh = LI.HighPart;
	}

	FORCEINLINE void UpdateFileSize()
	{
		LARGE_INTEGER LI;
		GetFileSizeEx(FileHandle, &LI);
		FileSize = LI.QuadPart;
	}

public:
	FFileHandleWindows(HANDLE InFileHandle = NULL)
		: FileHandle(InFileHandle)
		, FilePos(0)
		, FileSize(0)
	{
		if (IsValid())
		{
			UpdateFileSize();
		}
		FMemory::Memzero(&OverlappedIO, sizeof(OVERLAPPED));
	}
	virtual ~FFileHandleWindows()
	{
		CloseHandle(FileHandle);
		FileHandle = NULL;
	}
	virtual int64 Tell(void) override
	{
		check(IsValid());
		return FilePos;
	}
	virtual int64 Size() override
	{
		check(IsValid());
		return FileSize;
	}
	virtual bool Seek(int64 NewPosition) override
	{
		check(IsValid());
		check(NewPosition >= 0);
		check(NewPosition <= FileSize);

		FilePos = NewPosition;
		UpdateOverlappedPos();
		return true;
	}
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(IsValid());
		check(NewPositionRelativeToEnd <= 0);

		// Position is negative so this is actually subtracting
		return Seek(FileSize + NewPositionRelativeToEnd);
	}
	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		check(IsValid());
		uint32 NumRead = 0;
		// Now kick off an async read
		if (!ReadFile(FileHandle, Destination, BytesToRead, (::DWORD*)&NumRead, &OverlappedIO))
		{
			uint32 ErrorCode = GetLastError();
			if (ErrorCode != ERROR_IO_PENDING)
			{
				// Read failed
				return false;
			}
			// Wait for the read to complete
			NumRead = 0;
			if (!GetOverlappedResult(FileHandle, &OverlappedIO, (::DWORD*)&NumRead, true))
			{
				// Read failed
				return false;
			}
		}
		// Update where we are in the file
		FilePos += NumRead;
		UpdateOverlappedPos();
		return true;
	}
	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(IsValid());
		uint32 NumWritten = 0;
		// Now kick off an async write
		if (!WriteFile(FileHandle, Source, BytesToWrite, (::DWORD*)&NumWritten, &OverlappedIO))
		{
			uint32 ErrorCode = GetLastError();
			if (ErrorCode != ERROR_IO_PENDING)
			{
				// Write failed
				return false;
			}
			// Wait for the write to complete
			NumWritten = 0;
			if (!GetOverlappedResult(FileHandle, &OverlappedIO, (::DWORD*)&NumWritten, true))
			{
				// Write failed
				return false;
			}
		}
		// Update where we are in the file
		FilePos += NumWritten;
		UpdateOverlappedPos();
		UpdateFileSize();
		return true;
	}
};

/**
 * Windows File I/O implementation
**/
class CORE_API FWindowsPlatformFile : public IPhysicalPlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename)
	{
		FString Result(Filename);
		FPaths::NormalizeFilename(Result);
		if (Result.StartsWith(TEXT("//")))
		{
			Result = FString(TEXT("\\\\")) + Result.RightChop(2);
		}
		return FPaths::ConvertRelativePathToFull(Result);
	}
	virtual FString NormalizeDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		FPaths::NormalizeDirectoryName(Result);
		if (Result.StartsWith(TEXT("//")))
		{
			Result = FString(TEXT("\\\\")) + Result.RightChop(2);
		}
		return FPaths::ConvertRelativePathToFull(Result);
	}
public:
	virtual bool FileExists(const TCHAR* Filename) override
	{
		uint32 Result = GetFileAttributesW(*NormalizeFilename(Filename));
		if (Result != 0xFFFFFFFF && !(Result & FILE_ATTRIBUTE_DIRECTORY))
		{
			return true;
		}
		return false;
	}
	virtual int64 FileSize(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (!!GetFileAttributesExW(*NormalizeFilename(Filename), GetFileExInfoStandard, &Info))
		{
			if ((Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				LARGE_INTEGER li;
				li.HighPart = Info.nFileSizeHigh;
				li.LowPart = Info.nFileSizeLow;
				return li.QuadPart;
			}
		}
		return -1;
	}
	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		const FString NormalizedFilename = NormalizeFilename(Filename);
		return !!DeleteFileW(*NormalizedFilename);
	}
	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		uint32 Result = GetFileAttributes(*NormalizeFilename(Filename));
		if (Result != 0xFFFFFFFF)
		{
			return !!(Result & FILE_ATTRIBUTE_READONLY);
		}
		return false;
	}
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return !!MoveFileW(*NormalizeFilename(From), *NormalizeFilename(To));
	}
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		return !!SetFileAttributesW(*NormalizeFilename(Filename), bNewReadOnlyValue ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
	}

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (GetFileAttributesExW(*NormalizeFilename(Filename), GetFileExInfoStandard, &Info))
		{
			return WindowsFileTimeToUEDateTime(Info.ftLastWriteTime);
		}

		return FDateTime::MinValue();
	}

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		HANDLE Handle = CreateFileW(*NormalizeFilename(Filename), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			const FILETIME ModificationFileTime = UEDateTimeToWindowsFileTime(DateTime);
			if (!SetFileTime(Handle, nullptr, nullptr, &ModificationFileTime))
			{
				UE_LOG(LogTemp, Warning, TEXT("SetTimeStamp: Failed to SetFileTime on %s"), Filename);
			}
			CloseHandle(Handle);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetTimeStamp: Failed to open file %s"), Filename);
		}
	}

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (GetFileAttributesExW(*NormalizeFilename(Filename), GetFileExInfoStandard, &Info))
		{
			return WindowsFileTimeToUEDateTime(Info.ftLastAccessTime);
		}

		return FDateTime::MinValue();
	}

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override
	{
		FString Result;
		WIN32_FIND_DATAW Data;
		FString NormalizedFilename = NormalizeFilename(Filename);
		while (NormalizedFilename.Len())
		{
			HANDLE Handle = FindFirstFileW(*NormalizedFilename, &Data);
			if (Handle != INVALID_HANDLE_VALUE)
			{
				if (Result.Len())
				{
					Result = FString(Data.cFileName) / Result;
				}
				else
				{
					Result = Data.cFileName;
				}				
				FindClose(Handle);
			}
			int32 SeparatorIndex = INDEX_NONE;
			if (NormalizedFilename.FindLastChar('/', SeparatorIndex))
			{
				NormalizedFilename = NormalizedFilename.Mid(0, SeparatorIndex);				
			}
			if (NormalizedFilename.Len() && (SeparatorIndex == INDEX_NONE || NormalizedFilename.EndsWith(TEXT(":"))))
			{
				Result = NormalizedFilename / Result;
				NormalizedFilename.Empty();
			}
		}
		return Result;
	}

#define USE_WINDOWS_ASYNC_IMPL (!IS_PROGRAM && !WITH_EDITOR)
#if USE_WINDOWS_ASYNC_IMPL
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override
	{
		uint32  Access = GENERIC_READ;
		uint32  WinFlags = FILE_SHARE_READ;
		uint32  Create = OPEN_EXISTING;

		HANDLE Handle = CreateFileW(*NormalizeFilename(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
		// we can't really fail here because this is intended to be an async open
		return new FWindowsAsyncReadFileHandle(Handle);

	}
#endif

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		uint32  Access    = GENERIC_READ;
		uint32  WinFlags  = FILE_SHARE_READ | (bAllowWrite ? FILE_SHARE_WRITE : 0);
		uint32  Create    = OPEN_EXISTING;
#define USE_OVERLAPPED_IO 1

#if USE_OVERLAPPED_IO
		HANDLE Handle    = CreateFileW(*NormalizeFilename(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			return new FAsyncBufferedFileReaderWindows(Handle);
		}
#else
		HANDLE Handle = CreateFileW(*NormalizeFilename(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			return new FFileHandleWindows(Handle);
		}
#endif
		return NULL;
	}

	virtual IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		uint32  Access = GENERIC_READ;
		uint32  WinFlags = FILE_SHARE_READ | (bAllowWrite ? FILE_SHARE_WRITE : 0);
		uint32  Create = OPEN_EXISTING;
		HANDLE Handle = CreateFileW(*NormalizeFilename(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			return new FFileHandleWindows(Handle);
		}
		return NULL;
	}

	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		uint32  Access    = GENERIC_WRITE;
		uint32  WinFlags  = bAllowRead ? FILE_SHARE_READ : 0;
		uint32  Create    = bAppend ? OPEN_ALWAYS : CREATE_ALWAYS;
		HANDLE Handle    = CreateFileW(*NormalizeFilename(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL);
		if(Handle != INVALID_HANDLE_VALUE)
		{
			FFileHandleWindows *PlatformFileHandle = new FFileHandleWindows(Handle);
			if (bAppend)
			{
				PlatformFileHandle->SeekFromEnd(0);
			}
			return PlatformFileHandle;
		}
		return NULL;
	}

	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		// Empty Directory is the current directory so assume it always exists.
		bool bExists = !FCString::Strlen(Directory);
		if (!bExists) 
		{
			uint32 Result = GetFileAttributesW(*NormalizeDirectory(Directory));
			bExists = (Result != 0xFFFFFFFF && (Result & FILE_ATTRIBUTE_DIRECTORY));
		}
		return bExists;
	}
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
		return CreateDirectoryW(*NormalizeDirectory(Directory), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
	}
	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
		RemoveDirectoryW(*NormalizeDirectory(Directory));
		return !DirectoryExists(Directory);
	}
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (GetFileAttributesExW(*NormalizeFilename(FilenameOrDirectory), GetFileExInfoStandard, &Info))
		{
			const bool bIsDirectory = !!(Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

			int64 FileSize = -1;
			if (!bIsDirectory)
			{
				LARGE_INTEGER li;
				li.HighPart = Info.nFileSizeHigh;
				li.LowPart = Info.nFileSizeLow;
				FileSize = static_cast<int64>(li.QuadPart);
			}

			return FFileStatData(
				WindowsFileTimeToUEDateTime(Info.ftCreationTime),
				WindowsFileTimeToUEDateTime(Info.ftLastAccessTime),
				WindowsFileTimeToUEDateTime(Info.ftLastWriteTime),
				FileSize, 
				bIsDirectory,
				!!(Info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
				);
		}

		return FFileStatData();
	}
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
	{
		const FString DirectoryStr = Directory;
		return IterateDirectoryCommon(Directory, [&](const WIN32_FIND_DATAW& InData) -> bool
		{
			const bool bIsDirectory = !!(InData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			return Visitor.Visit(*(DirectoryStr / InData.cFileName), bIsDirectory);
		});
	}
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override
	{
		const FString DirectoryStr = Directory;
		return IterateDirectoryCommon(Directory, [&](const WIN32_FIND_DATAW& InData) -> bool
		{
			const bool bIsDirectory = !!(InData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

			int64 FileSize = -1;
			if (!bIsDirectory)
			{
				LARGE_INTEGER li;
				li.HighPart = InData.nFileSizeHigh;
				li.LowPart = InData.nFileSizeLow;
				FileSize = static_cast<int64>(li.QuadPart);
			}

			return Visitor.Visit(
				*(DirectoryStr / InData.cFileName), 
				FFileStatData(
					WindowsFileTimeToUEDateTime(InData.ftCreationTime),
					WindowsFileTimeToUEDateTime(InData.ftLastAccessTime),
					WindowsFileTimeToUEDateTime(InData.ftLastWriteTime),
					FileSize, 
					bIsDirectory,
					!!(InData.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
					)
				);
		});
	}
	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(const WIN32_FIND_DATAW&)>& Visitor)
	{
		bool Result = false;
		WIN32_FIND_DATAW Data;
		HANDLE Handle = FindFirstFileW(*(NormalizeDirectory(Directory) / TEXT("*.*")), &Data);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			Result = true;
			do
			{
				if (FCString::Strcmp(Data.cFileName, TEXT(".")) && FCString::Strcmp(Data.cFileName, TEXT("..")))
				{
					Result = Visitor(Data);
				}
			} while (Result && FindNextFileW(Handle, &Data));
			FindClose(Handle);
		}
		return Result;
	}
};

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FWindowsPlatformFile Singleton;
	return Singleton;
}
