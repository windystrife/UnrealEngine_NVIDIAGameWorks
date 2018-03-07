// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5File.cpp: HTML5 platform implementations of File functions

	NOTE: this file should closely match [ LinuxPlatformFile.cpp ]
		the only difference is "local" storage has special handler (which is
		going to change in the near future when wasm starts using the ASMFS)

=============================================================================*/

#include "CoreTypes.h"
#include "HAL/PlatformMath.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Function.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <utime.h>

#ifndef O_BINARY
	#define O_BINARY 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHTML5PlatformFile, Log, All);

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime HTML5Epoch(1970, 1, 1);

namespace
{
	FFileStatData HTML5StatToUEFileData(struct stat& FileInfo)
	{
		const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = FileInfo.st_size;
		}

		return FFileStatData(
			HTML5Epoch + FTimespan::FromSeconds(FileInfo.st_ctime),
			HTML5Epoch + FTimespan::FromSeconds(FileInfo.st_atime),
			HTML5Epoch + FTimespan::FromSeconds(FileInfo.st_mtime),
			FileSize,
			bIsDirectory,
			!!(FileInfo.st_mode & S_IWUSR)
			);
	}
}

/**
 * HTML5 file handle implementation
**/
class CORE_API FFileHandleHTML5 : public IFileHandle
{
	enum {READWRITE_SIZE = 1024 * 1024};
	int32 FileHandle;

	FORCEINLINE bool IsValid()
	{
		return FileHandle != -1;
	}

public:
	FFileHandleHTML5(int32 InFileHandle, const TCHAR* InFilename)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
	{
		check(FileHandle > -1);
		check(Filename.Len() > 0);

		bUseLocalStorage = Filename.Contains(FString(FApp::GetProjectName()) + TEXT("/Saved/"));
		if ( bUseLocalStorage )
		{
			// currently, need to setup IndexDB mount point -- but this is going to change in the near future to ASMFS
			// currently, SaveGame only really needs this for now -- but, that stuff is implimented in HTML5SaveGameSystem.cpp
			// OTOH, Saved/Config/*.ini could also use this persistent feature
		}
	}

	virtual ~FFileHandleHTML5()
	{
		close(FileHandle);
		FileHandle = -1;
	}

	virtual int64 Tell() override
	{
		check(IsValid());
		return lseek(FileHandle, 0, SEEK_CUR);
	}

	virtual bool Seek(int64 NewPosition) override
	{
		check(IsValid());
		check(NewPosition >= 0);
		return lseek(FileHandle, NewPosition, SEEK_SET) != -1;
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(IsValid());
		check(NewPositionRelativeToEnd <= 0);
		return lseek(FileHandle, NewPositionRelativeToEnd, SEEK_END) != -1;
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		check(IsValid());

		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FPlatformMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			if (read(FileHandle, Destination, ThisSize) != ThisSize)
			{
				return false;
			}
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}
		return true;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(IsValid());
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FPlatformMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			if (write(FileHandle, Source, ThisSize) != ThisSize)
			{
				return false;
			}
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		return true;
	}

private:
	FString Filename;
	bool bUseLocalStorage;
};

/**
 * HTML5 File I/O implementation
**/
class CORE_API FHTML5PlatformFile : public IPhysicalPlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename)
	{
		FString Result(Filename);
		FPaths::NormalizeFilename(Result);
		return FPaths::ConvertRelativePathToFull(Result);
	}

	virtual FString NormalizeDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		FPaths::NormalizeDirectoryName(Result);
		return FPaths::ConvertRelativePathToFull(Result);
	}

public:
	virtual bool FileExists(const TCHAR* Filename) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) != -1)
		{
			return S_ISREG(FileInfo.st_mode);
		}
		return false;
	}

	virtual int64 FileSize(const TCHAR* Filename) override
	{
		struct stat FileInfo;
		FileInfo.st_size = -1;
		stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo);
		// make sure to return -1 for directories
		if (S_ISDIR(FileInfo.st_mode))
		{
			FileInfo.st_size = -1;
		}
		return FileInfo.st_size;
	}

	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		return unlink(TCHAR_TO_UTF8(*NormalizeFilename(Filename))) == 0;
	}

	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), F_OK) == -1)
		{
			return false; // file doesn't exist
		}
		if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), W_OK) == -1)
		{
			return errno == EACCES;
		}
		return false;
	}

	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return rename(TCHAR_TO_UTF8(*NormalizeFilename(From)), TCHAR_TO_UTF8(*NormalizeFilename(To))) != -1;
	}

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) != -1)
		{
			if (bNewReadOnlyValue)
			{
				FileInfo.st_mode &= ~S_IWUSR;
			}
			else
			{
				FileInfo.st_mode |= S_IWUSR;
			}
			return chmod(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), FileInfo.st_mode) == 0;
		}
		return false;
	}

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
		return HTML5Epoch + TimeSinceEpoch;
	}

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return;
		}

		// change the modification time only
		struct utimbuf Times;
		Times.actime = FileInfo.st_atime;
		Times.modtime = (DateTime - HTML5Epoch).GetTotalSeconds();
		utime(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &Times);
	}

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
		return HTML5Epoch + TimeSinceEpoch;
	}

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override
	{
		return Filename;
	}

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite /*= false*/) override
	{
		FString fn = NormalizeFilename(Filename);
		int32 Handle = open(TCHAR_TO_UTF8(*fn), O_RDONLY | O_BINARY);
		if (Handle != -1)
		{
			return new FFileHandleHTML5(Handle, Filename);
		}
		return NULL;
	}

	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		int Flags = O_CREAT;
		if (bAppend)
		{
			Flags |= O_APPEND;
		}
		else
		{
			Flags |= O_TRUNC;
		}
		if (bAllowRead)
		{
			Flags |= O_RDWR;
		}
		else
		{
			Flags |= O_WRONLY;
		}

		// create directories if needed.
		if (!CreateDirectoriesFromPath(Filename))
		{
			return NULL;
		}

		int32 Handle = open(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), Flags, S_IRUSR | S_IWUSR);
		if (Handle != -1)
		{
			FFileHandleHTML5* FileHandleHTML5 = new FFileHandleHTML5(Handle, Filename);
			if (bAppend)
			{
				FileHandleHTML5->SeekFromEnd(0);
			}
			return FileHandleHTML5;
		}
		return NULL;
	}

	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Directory)), &FileInfo) != -1)
		{
			return S_ISDIR(FileInfo.st_mode);
		}
		return false;
	}
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
		return mkdir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)), 0755) == 0;
	}

	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
		return rmdir(TCHAR_TO_UTF8(*NormalizeFilename(Directory))) == 0;
	}

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(FilenameOrDirectory)), &FileInfo) == -1)
		{
			return FFileStatData();
		}

		return HTML5StatToUEFileData(FileInfo);
	}

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
	{
		const FString DirectoryStr = Directory;
		return IterateDirectoryCommon(Directory, [&](struct dirent* InEntry) -> bool
		{
			const bool bIsDirectory = InEntry->d_type == DT_DIR;
			return Visitor.Visit(*(DirectoryStr / UTF8_TO_TCHAR(InEntry->d_name)), bIsDirectory);
		});
	}

	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override
	{
		const FString DirectoryStr = Directory;
		const FString NormalizedDirectoryStr = NormalizeFilename(Directory);
		return IterateDirectoryCommon(Directory, [&](struct dirent* InEntry) -> bool
		{
			const FString UnicodeEntryName = UTF8_TO_TCHAR(InEntry->d_name);

			struct stat FileInfo;
			const FString AbsoluteUnicodeName = NormalizedDirectoryStr / UnicodeEntryName;
			if (stat(TCHAR_TO_UTF8(*AbsoluteUnicodeName), &FileInfo) != -1)
			{
				return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), HTML5StatToUEFileData(FileInfo));
			}
			return true;
		});
	}

	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(struct dirent*)>& Visitor)
	{
		bool Result = false;
		// If Directory is an empty string, assume that we want to iterate Binaries/Mac (current dir), but because we're an app bundle, iterate bundle's Contents/Frameworks instead
		DIR* Handle = opendir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)));
		if (Handle)
		{
			Result = true;
			struct dirent *Entry;
			while ((Entry = readdir(Handle)) != NULL)
			{
				if (FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT("..")))
				{
					Result = Visitor(Entry);
				}
			}
			closedir(Handle);
		}
		return Result;
	}

	bool CreateDirectoriesFromPath(const TCHAR* Path)
	{
		// if the file already exists, then directories exist.
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Path)), &FileInfo) != -1)
		{
			return true;
		}

		// convert path to native char string.
		int32 Len = strlen(TCHAR_TO_UTF8(*NormalizeFilename(Path)));
		char *DirPath = reinterpret_cast<char *>(FMemory::Malloc((Len+2) * sizeof(char)));
		char *SubPath = reinterpret_cast<char *>(FMemory::Malloc((Len+2) * sizeof(char)));
		strcpy(DirPath, TCHAR_TO_UTF8(*NormalizeFilename(Path)));

		for (int32 i=0; i<Len; ++i)
		{
			SubPath[i] = DirPath[i];

			if (SubPath[i] == '/')
			{
				SubPath[i+1] = 0;

				// directory exists?
				struct stat SubPathFileInfo;
				if (stat(SubPath, &SubPathFileInfo) == -1)
				{
					// nope. create it.
					if (mkdir(SubPath, 0755) == -1)
					{
						int ErrNo = errno;
						UE_LOG(LogHTML5PlatformFile, Warning, TEXT( "create dir('%s') failed: errno=%d (%s)" ),
																	DirPath, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
						FMemory::Free(DirPath);
						FMemory::Free(SubPath);
						return false;
					}
				}
			}
		}

		FMemory::Free(DirPath);
		FMemory::Free(SubPath);
		return true;
	}
};

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FHTML5PlatformFile Singleton;
	return Singleton;
}
