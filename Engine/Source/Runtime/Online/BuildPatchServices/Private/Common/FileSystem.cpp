// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Common/FileSystem.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Containers/StringConv.h"

#if PLATFORM_WINDOWS
// Start of region that uses windows types.
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <wtypes.h>
#include <winbase.h>
#include <winioctl.h>
namespace FileSystemHelpers
{
	bool PlatformGetAttributes(const TCHAR* Filename, BuildPatchServices::EFileAttributes& OutFileAttributes)
	{
		OutFileAttributes = BuildPatchServices::EFileAttributes::None;
		DWORD FileAttributes = ::GetFileAttributes(Filename);
		DWORD Error = ::GetLastError();
		if (FileAttributes != INVALID_FILE_ATTRIBUTES)
		{
			OutFileAttributes |= BuildPatchServices::EFileAttributes::Exists;
			if ((FileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
			{
				OutFileAttributes |= BuildPatchServices::EFileAttributes::ReadOnly;
			}
			if ((FileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0)
			{
				OutFileAttributes |= BuildPatchServices::EFileAttributes::Compressed;
			}
			return true;
		}
		else if (Error == ERROR_PATH_NOT_FOUND || Error == ERROR_FILE_NOT_FOUND)
		{
			return true;
		}
		return false;
	}

	bool PlatformSetCompressed(const TCHAR* Filename, bool bIsCompressed)
	{
		// Get the file handle.
		HANDLE FileHandle = ::CreateFile(
			Filename,                               // Path to file.
			GENERIC_READ | GENERIC_WRITE,           // Read and write access.
			FILE_SHARE_READ | FILE_SHARE_WRITE,     // Share access for DeviceIoControl.
			NULL,                                   // Default security.
			OPEN_EXISTING,                          // Open existing file.
			FILE_ATTRIBUTE_NORMAL,                  // No specific attributes.
			NULL                                    // No template file handle.
		);
		uint32 Error = ::GetLastError();
		if (FileHandle == NULL || FileHandle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		// Send the compression control code to the device.
		uint16 Message = bIsCompressed ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
		uint16* MessagePtr = &Message;
		DWORD Dummy = 0;
		LPDWORD DummyPtr = &Dummy;
		bool bSuccess = ::DeviceIoControl(
			FileHandle,                 // The file handle.
			FSCTL_SET_COMPRESSION,      // Control code.
			MessagePtr,                 // Our message.
			sizeof(uint16),             // Our message size.
			NULL,                       // Not used.
			0,                          // Not used.
			DummyPtr,                   // The value returned will be meaningless, but is required.
			NULL                        // No overlap structure, we a running this synchronously.
		) == TRUE;
		Error = ::GetLastError();

		// Close the open file handle.
		::CloseHandle(FileHandle);

		// We treat unsupported as not being a failure.
		const bool bFileSystemUnsupported = Error == ERROR_INVALID_FUNCTION;
		return bSuccess || bFileSystemUnsupported;
	}

	bool PlatformSetExecutable(const TCHAR* Filename, bool bIsExecutable)
	{
		// Not implemented.
		return true;
	}
}
// End of region that uses windows types.
#include "HideWindowsPlatformTypes.h"
// Stop windows header breaking our class's function name.
#undef GetFileAttributes
#elif PLATFORM_MAC
namespace FileSystemHelpers
{
	bool PlatformGetAttributes(const TCHAR* Filename, BuildPatchServices::EFileAttributes& OutFileAttributes)
	{
		struct stat FileInfo;
		const FTCHARToUTF8 FilenameUtf8(Filename);
		int32 Result = stat(FilenameUtf8.Get(), &FileInfo);
		int32 Error = errno;
		OutFileAttributes = BuildPatchServices::EFileAttributes::None;
		if (Result == 0)
		{
			OutFileAttributes |= BuildPatchServices::EFileAttributes::Exists;
			if ((FileInfo.st_mode & S_IWUSR) == 0)
			{
				OutFileAttributes |= BuildPatchServices::EFileAttributes::ReadOnly;
			}
			const mode_t ExeFlags = S_IXUSR | S_IXGRP | S_IXOTH;
			if ((FileInfo.st_mode & ExeFlags) == ExeFlags)
			{
				OutFileAttributes |= BuildPatchServices::EFileAttributes::Executable;
			}
			return true;
		}
		else if (Error == ENOTDIR || Error == ENOENT)
		{
			return true;
		}
		return false;
	}

	bool PlatformSetCompressed(const TCHAR* Filename, bool bIsCompressed)
	{
		// Not implemented
		return true;
	}

	bool PlatformSetExecutable(const TCHAR* Filename, bool bIsExecutable)
	{
		struct stat FileInfo;
		FTCHARToUTF8 FilenameUtf8(Filename);
		bool bSuccess = false;
		// Set executable permission bit
		if (stat(FilenameUtf8.Get(), &FileInfo) == 0)
		{
			mode_t ExeFlags = S_IXUSR | S_IXGRP | S_IXOTH;
			FileInfo.st_mode = bIsExecutable ? (FileInfo.st_mode | ExeFlags) : (FileInfo.st_mode & ~ExeFlags);
			bSuccess = chmod(FilenameUtf8.Get(), FileInfo.st_mode) == 0;
		}
		return bSuccess;
	}
}
#else
namespace FileSystemHelpers
{
	bool PlatformGetAttributes(const TCHAR* Filename, BuildPatchServices::EFileAttributes& OutFileAttributes)
	{
		// Not implemented.
		return true;
	}

	bool PlatformSetCompressed(const TCHAR* Filename, bool bIsCompressed)
	{
		// Not implemented.
		return true;
	}

	bool PlatformSetExecutable(const TCHAR* Filename, bool bIsExecutable)
	{
		// Not implemented.
		return true;
	}
}
#endif

// We are forwarding flags, so assert they are all equal.
static_assert((uint32)BuildPatchServices::EFileWrite::None == (uint32)::EFileWrite::FILEWRITE_None, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::None with ::EFileWrite::FILEWRITE_None");
static_assert((uint32)BuildPatchServices::EFileWrite::NoFail == (uint32)::EFileWrite::FILEWRITE_NoFail, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::NoFail with ::EFileWrite::FILEWRITE_NoFail");
static_assert((uint32)BuildPatchServices::EFileWrite::NoReplaceExisting == (uint32)::EFileWrite::FILEWRITE_NoReplaceExisting, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::NoReplaceExisting with ::EFileWrite::FILEWRITE_NoReplaceExisting");
static_assert((uint32)BuildPatchServices::EFileWrite::EvenIfReadOnly == (uint32)::EFileWrite::FILEWRITE_EvenIfReadOnly, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::EvenIfReadOnly with ::EFileWrite::FILEWRITE_EvenIfReadOnly");
static_assert((uint32)BuildPatchServices::EFileWrite::Append == (uint32)::EFileWrite::FILEWRITE_Append, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::Append with ::EFileWrite::FILEWRITE_Append");
static_assert((uint32)BuildPatchServices::EFileWrite::AllowRead == (uint32)::EFileWrite::FILEWRITE_AllowRead, "Please update FileSystem.h to match BuildPatchServices::EFileWrite::AllowRead with ::EFileWrite::FILEWRITE_AllowRead");
static_assert((uint32)BuildPatchServices::EFileRead::None == (uint32)::EFileRead::FILEREAD_None, "Please update FileSystem.h to match BuildPatchServices::EFileRead::None with ::EFileRead::FILEREAD_None");
static_assert((uint32)BuildPatchServices::EFileRead::NoFail == (uint32)::EFileRead::FILEREAD_NoFail, "Please update FileSystem.h to match BuildPatchServices::EFileRead::NoFail with ::EFileRead::FILEREAD_NoFail");
static_assert((uint32)BuildPatchServices::EFileRead::Silent == (uint32)::EFileRead::FILEREAD_Silent, "Please update FileSystem.h to match BuildPatchServices::EFileRead::Silent with ::EFileRead::FILEREAD_Silent");
static_assert((uint32)BuildPatchServices::EFileRead::AllowWrite == (uint32)::EFileRead::FILEREAD_AllowWrite, "Please update FileSystem.h to match BuildPatchServices::EFileRead::AllowWrite with ::EFileRead::FILEREAD_AllowWrite");

namespace BuildPatchServices
{
	class FFileSystem
		: public IFileSystem
	{
	public:
		FFileSystem();
		~FFileSystem();

		// IFileSystem interface begin.
		virtual bool GetFileSize(const TCHAR* Filename, int64& FileSize) const override;
		virtual bool GetFileAttributes(const TCHAR* Filename, EFileAttributes& FileAttributes) const override;
		virtual bool SetReadOnly(const TCHAR* Filename, bool bIsReadOnly) const override;
		virtual bool SetCompressed(const TCHAR* Filename, bool bIsCompressed) const override;
		virtual bool SetExecutable(const TCHAR* Filename, bool bIsExecutable) const override;
		virtual TUniquePtr<FArchive> CreateFileReader(const TCHAR* Filename, EFileRead ReadFlags = EFileRead::None) const override;
		virtual TUniquePtr<FArchive> CreateFileWriter(const TCHAR* Filename, EFileWrite WriteFlags = EFileWrite::None) const override;
		virtual bool DeleteFile(const TCHAR* Filename) const override;
		virtual bool MoveFile(const TCHAR* FileDest, const TCHAR* FileSource) const override;
		// IFileSystem interface end.

	private:
		IFileManager& FileManager;
		IPlatformFile& PlatformFile;
	};

	FFileSystem::FFileSystem()
		: FileManager(IFileManager::Get())
		, PlatformFile(IPlatformFile::GetPlatformPhysical())
	{
	}

	FFileSystem::~FFileSystem()
	{
	}

	bool FFileSystem::GetFileSize(const TCHAR* Filename, int64& FileSize) const
	{
		FileSize = PlatformFile.FileSize(Filename);
		return true;
	}

	bool FFileSystem::GetFileAttributes(const TCHAR* Filename, EFileAttributes& FileAttributes) const
	{
		return FileSystemHelpers::PlatformGetAttributes(Filename, FileAttributes);
	}

	bool FFileSystem::SetReadOnly(const TCHAR* Filename, bool bIsReadOnly) const
	{
		return PlatformFile.SetReadOnly(Filename, bIsReadOnly);
	}

	bool FFileSystem::SetCompressed(const TCHAR* Filename, bool bIsCompressed) const
	{
		return FileSystemHelpers::PlatformSetExecutable(Filename, bIsCompressed);
	}

	bool FFileSystem::SetExecutable(const TCHAR* Filename, bool bIsExecutable) const
	{
		return FileSystemHelpers::PlatformSetExecutable(Filename, bIsExecutable);
	}

	TUniquePtr<FArchive> FFileSystem::CreateFileReader(const TCHAR* Filename, EFileRead ReadFlags) const
	{
		return TUniquePtr<FArchive>(FileManager.CreateFileReader(Filename, static_cast<uint32>(ReadFlags)));
	}

	TUniquePtr<FArchive> FFileSystem::CreateFileWriter(const TCHAR* Filename, EFileWrite WriteFlags) const
	{
		return TUniquePtr<FArchive>(FileManager.CreateFileWriter(Filename, static_cast<uint32>(WriteFlags)));
	}

	bool FFileSystem::DeleteFile(const TCHAR* Filename) const
	{
		return FileManager.Delete(Filename, false, true, true);
	}

	bool FFileSystem::MoveFile(const TCHAR* FileDest, const TCHAR* FileSource) const
	{
		return FileManager.Move(FileDest, FileSource, true, true, true, false);
	}

	IFileSystem* FFileSystemFactory::Create()
	{
		return new FFileSystem();
	}
}
