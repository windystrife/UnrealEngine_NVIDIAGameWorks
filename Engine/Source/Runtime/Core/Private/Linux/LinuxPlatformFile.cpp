// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformFile.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include <sys/file.h>

#include "PlatformFileCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogLinuxPlatformFile, Log, All);

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime UnixEpoch(1970, 1, 1);

namespace
{
	FFileStatData UnixStatToUEFileData(struct stat& FileInfo)
	{
		const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = FileInfo.st_size;
		}

		return FFileStatData(
			UnixEpoch + FTimespan::FromSeconds(FileInfo.st_ctime), 
			UnixEpoch + FTimespan::FromSeconds(FileInfo.st_atime), 
			UnixEpoch + FTimespan::FromSeconds(FileInfo.st_mtime), 
			FileSize,
			bIsDirectory,
			!!(FileInfo.st_mode & S_IWUSR)
		);
	}
}

/**
 * Linux file handle implementation which limits number of open files per thread. This
 * is to prevent running out of system file handles. Should not be neccessary when
 * using pak file (e.g., SHIPPING?) so not particularly optimized. Only manages
 * files which are opened READ_ONLY.
 */

/**
 * Linux version of the file handle registry
 */
class FLinuxFileRegistry : public FFileHandleRegistry
{
public:
	FLinuxFileRegistry()
		: FFileHandleRegistry(200)
	{
	}

protected:
	virtual FRegisteredFileHandle* PlatformInitialOpenFile(const TCHAR* Filename) override;
	virtual bool PlatformReopenFile(FRegisteredFileHandle* Handle) override;
	virtual void PlatformCloseFile(FRegisteredFileHandle* Handle) override;
};
static FLinuxFileRegistry GFileRegistry;

/** 
 * Linux file handle implementation
 */
class CORE_API FFileHandleLinux : public FRegisteredFileHandle
{
	enum {READWRITE_SIZE = SSIZE_MAX};

	FORCEINLINE bool IsValid()
	{
		return FileHandle != -1;
	}

public:
	FFileHandleLinux(int32 InFileHandle, const TCHAR* InFilename, bool InFileOpenAsWrite)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
		, FileOffset(0)
		, FileSize(0)
		, FileOpenAsWrite(InFileOpenAsWrite)
	{
		check(FileHandle > -1);
		check(Filename.Len() > 0);

		// Only files opened for read will be managed
		if (!FileOpenAsWrite)
		{
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			FileSize = FileInfo.st_size;
		}
	}

	virtual ~FFileHandleLinux()
	{
		if (FileOpenAsWrite)
		{
			close(FileHandle);
		}
		else
		{
			// only track registry for read files
			GFileRegistry.UnTrackAndCloseFile(this);
		}
		FileHandle = -1;
	}

	virtual int64 Tell() override
	{
		if (!FileOpenAsWrite)
		{
			return FileOffset;
		}
		else
		{
			check(IsValid());
			return lseek(FileHandle, 0, SEEK_CUR);
		}
	}

	virtual bool Seek(int64 NewPosition) override
	{
		check(NewPosition >= 0);

		if (!FileOpenAsWrite)
		{
			FileOffset = NewPosition >= FileSize ? FileSize - 1 : NewPosition;
			return true;
		}
		else
		{
			check(IsValid());
			return lseek(FileHandle, NewPosition, SEEK_SET) != -1;
		}
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(NewPositionRelativeToEnd <= 0);

		if (!FileOpenAsWrite)
		{
			FileOffset = (NewPositionRelativeToEnd >= FileSize) ? 0 : (FileSize + NewPositionRelativeToEnd - 1);
			return true;
		}
		else
		{
			check(IsValid());
			return lseek(FileHandle, NewPositionRelativeToEnd, SEEK_END) != -1;
		}
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		int64 BytesRead = 0;
		// handle virtual file handles
		GFileRegistry.TrackStartRead(this);

		{
			FScopedDiskUtilizationTracker Tracker(BytesToRead, FileOffset);
			// seek to the offset on seek? this matches console behavior more closely
			check(IsValid());
			if (lseek(FileHandle, FileOffset, SEEK_SET) == -1)
			{
				return false;
			}
			BytesRead += ReadInternal(Destination, BytesToRead);
		}

		// handle virtual file handles
		GFileRegistry.TrackEndRead(this);
		FileOffset += BytesRead;
		return BytesRead == BytesToRead;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(IsValid());
		check(FileOpenAsWrite);
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
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

	virtual int64 Size() override
	{
		if (!FileOpenAsWrite)
		{
			return FileSize;
		}
		else
		{
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			return FileInfo.st_size;
		}
	}

	
private:

	/** This function exists because we cannot read more than SSIZE_MAX at once */
	int64 ReadInternal(uint8* Destination, int64 BytesToRead)
	{
		check(IsValid());
		int64 BytesRead = 0;
		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			int64 ThisRead = read(FileHandle, Destination, ThisSize);
			BytesRead += ThisRead;
			if (ThisRead != ThisSize)
			{
				return BytesRead;
			}
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}
		return BytesRead;
	}

	// Holds the internal file handle.
	int32 FileHandle;

	// Holds the name of the file that this handle represents. Kept around for possible reopen of file.
	FString Filename;

	// Most recent valid slot index for this handle; >=0 for handles which are managed.
	int32 HandleSlot;

	// Current file offset; valid if a managed handle.
	int64 FileOffset;

	// Cached file size; valid if a managed handle.
	int64 FileSize;

	// track if file is open for write
	bool FileOpenAsWrite;

	friend class FReadaheadCache;
	friend class FLinuxFileRegistry;
};

/**
 * A class to handle case insensitive file opening. This is a band-aid, non-performant approach,
 * without any caching.
 */
class FLinuxFileMapper
{

public:

	FLinuxFileMapper()
	{
	}

	FString GetPathComponent(const FString & Filename, int NumPathComponent)
	{
		// skip over empty part
		int StartPosition = (Filename[0] == TEXT('/')) ? 1 : 0;
		
		for (int ComponentIdx = 0; ComponentIdx < NumPathComponent; ++ComponentIdx)
		{
			int FoundAtIndex = Filename.Find(TEXT("/"), ESearchCase::CaseSensitive,
											 ESearchDir::FromStart, StartPosition);
			
			if (FoundAtIndex == INDEX_NONE)
			{
				checkf(false, TEXT("Asked to get %d-th path component, but filename '%s' doesn't have that many!"), 
					   NumPathComponent, *Filename);
				break;
			}
			
			StartPosition = FoundAtIndex + 1;	// skip the '/' itself
		}

		// now return the 
		int NextSlash = Filename.Find(TEXT("/"), ESearchCase::CaseSensitive,
									  ESearchDir::FromStart, StartPosition);
		if (NextSlash == INDEX_NONE)
		{
			// just return the rest of the string
			return Filename.RightChop(StartPosition);
		}
		else if (NextSlash == StartPosition)
		{
			return TEXT("");	// encountered an invalid path like /foo/bar//baz
		}
		
		return Filename.Mid(StartPosition, NextSlash - StartPosition);
	}

	int32 CountPathComponents(const FString & Filename)
	{
		if (Filename.Len() == 0)
		{
			return 0;
		}

		// if the first character is not a separator, it's part of a distinct component
		int NumComponents = (Filename[0] != TEXT('/')) ? 1 : 0;
		for (const auto & Char : Filename)
		{
			if (Char == TEXT('/'))
			{
				++NumComponents;
			}
		}

		// cannot be 0 components if path is non-empty
		return FMath::Max(NumComponents, 1);
	}

	/**
	 * Tries to recursively find (using case-insensitive comparison) and open the file.
	 * The first file found will be opened.
	 * 
	 * @param Filename Original file path as requested (absolute)
	 * @param PathComponentToLookFor Part of path we are currently trying to find.
	 * @param MaxPathComponents Maximum number of path components (directories), i.e. how deep the path is.
	 * @param ConstructedPath The real (absolute) path that we have found so far
	 * 
	 * @return a handle opened with open()
	 */
	bool MapFileRecursively(const FString & Filename, int PathComponentToLookFor, int MaxPathComponents, FString & ConstructedPath)
	{
		// get the directory without the last path component
		FString BaseDir = ConstructedPath;

		// get the path component to compare
		FString PathComponent = GetPathComponent(Filename, PathComponentToLookFor);
		FString PathComponentLower = PathComponent.ToLower();

		bool bFound = false;

		// see if we can open this (we should)
		DIR* DirHandle = opendir(TCHAR_TO_UTF8(*BaseDir));
		if (DirHandle)
		{
			struct dirent *Entry;
			while ((Entry = readdir(DirHandle)) != nullptr)
			{
				FString DirEntry = UTF8_TO_TCHAR(Entry->d_name);
				if (DirEntry.ToLower() == PathComponentLower)
				{
					if (PathComponentToLookFor < MaxPathComponents - 1)
					{
						// make sure this is a directory
						bool bIsDirectory = Entry->d_type == DT_DIR;
						if(Entry->d_type == DT_UNKNOWN || Entry->d_type == DT_LNK)
						{
							struct stat StatInfo;
							if(stat(TCHAR_TO_UTF8(*(BaseDir / Entry->d_name)), &StatInfo) == 0)
							{
								bIsDirectory = S_ISDIR(StatInfo.st_mode);
							}
						}

						if (bIsDirectory)
						{
							// recurse with the new filename
							FString NewConstructedPath = ConstructedPath;
							NewConstructedPath /= DirEntry;

							bFound = MapFileRecursively(Filename, PathComponentToLookFor + 1, MaxPathComponents, NewConstructedPath);
							if (bFound)
							{
								ConstructedPath = NewConstructedPath;
								break;
							}
						}
					}
					else
					{
						// last level, try opening directly
						FString ConstructedFilename = ConstructedPath;
						ConstructedFilename /= DirEntry;

						struct stat StatInfo;
						bFound = (stat(TCHAR_TO_UTF8(*ConstructedFilename), &StatInfo) == 0);
						if (bFound)
						{
							ConstructedPath = ConstructedFilename;
							break;
						}
					}
				}
			}
			closedir(DirHandle);
		}
		
		return bFound;
	}

	/**
	 * Tries to map a filename (one with a possibly wrong case) to one that exists.
	 * 
	 * @param PossiblyWrongFilename absolute filename (that has possibly a wrong case)
	 * @param ExistingFilename filename that exists (only valid to use if the function returned success).
	 */
	bool MapCaseInsensitiveFile(const FString & PossiblyWrongFilename, FString & ExistingFilename)
	{
		// Cannot log anything here, as this may result in infinite recursion when this function is called on log file itself

		// We can get some "absolute" filenames like "D:/Blah/" here (e.g. non-Linux paths to source files embedded in assets).
		// In that case, fail silently.
		if (PossiblyWrongFilename.IsEmpty() || PossiblyWrongFilename[0] != TEXT('/'))
		{
			return false;
		}

		// try the filename as given first
		struct stat StatInfo;
		bool bFound = stat(TCHAR_TO_UTF8(*PossiblyWrongFilename), &StatInfo) == 0;

		if (bFound)
		{
			ExistingFilename = PossiblyWrongFilename;
		}
		else
		{
			// perform a case-insensitive search from /

			int MaxPathComponents = CountPathComponents(PossiblyWrongFilename);
			if (MaxPathComponents > 0)
			{
				FString FoundFilename(TEXT("/"));	// start with root
				bFound = MapFileRecursively(PossiblyWrongFilename, 0, MaxPathComponents, FoundFilename);
				if (bFound)
				{
					ExistingFilename = FoundFilename;
				}
			}
		}

		return bFound;
	}

	/**
	 * Opens a file for reading, disregarding the case.
	 * 
	 * @param Filename absolute filename
	 * @param MappedToFilename absolute filename that we mapped the Filename to (always filled out on success, even if the same as Filename)
	 */
	int32 OpenCaseInsensitiveRead(const FString & Filename, FString & MappedToFilename)
	{
		// We can get some "absolute" filenames like "D:/Blah/" here (e.g. non-Linux paths to source files embedded in assets).
		// In that case, fail silently.
		if (Filename.IsEmpty() || Filename[0] != TEXT('/'))
		{
			return -1;
		}

		// try opening right away
		int32 Handle = open(TCHAR_TO_UTF8(*Filename), O_RDONLY | O_CLOEXEC);
		if (Handle != -1)
		{
			MappedToFilename = Filename;
		}
		else
		{
			// log non-standard errors only
			if (ENOENT != errno)
			{
				int ErrNo = errno;
				UE_LOG(LogLinuxPlatformFile, Warning, TEXT( "open('%s', O_RDONLY | O_CLOEXEC) failed: errno=%d (%s)" ), *Filename, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
			}
			else
			{
				// perform a case-insensitive search
				// make sure we get the absolute filename
				checkf(Filename[0] == TEXT('/'), TEXT("Filename '%s' given to OpenCaseInsensitiveRead is not absolute!"), *Filename);
				
				int MaxPathComponents = CountPathComponents(Filename);
				if (MaxPathComponents > 0)
				{
					FString FoundFilename(TEXT("/"));	// start with root
					if (MapFileRecursively(Filename, 0, MaxPathComponents, FoundFilename))
					{
						Handle = open(TCHAR_TO_UTF8(*FoundFilename), O_RDONLY | O_CLOEXEC);
						if (Handle != -1)
						{
							MappedToFilename = FoundFilename;
							if (Filename != MappedToFilename)
							{
								UE_LOG(LogLinuxPlatformFile, Log, TEXT("Mapped '%s' to '%s'"), *Filename, *MappedToFilename);
							}
						}
					}
				}
			}
		}
		return Handle;
	}
};

FLinuxFileMapper GCaseInsensMapper;

/**
 * Linux File I/O implementation
**/
FString FLinuxPlatformFile::NormalizeFilename(const TCHAR* Filename)
{
	FString Result(Filename);
	FPaths::NormalizeFilename(Result);
	return FPaths::ConvertRelativePathToFull(Result);
}

FString FLinuxPlatformFile::NormalizeDirectory(const TCHAR* Directory)
{
	FString Result(Directory);
	FPaths::NormalizeDirectoryName(Result);
	return FPaths::ConvertRelativePathToFull(Result);
}

bool FLinuxPlatformFile::FileExists(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		return S_ISREG(FileInfo.st_mode);
	}
	return false;
}

int64 FLinuxPlatformFile::FileSize(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return -1;
	}

	struct stat FileInfo;
	FileInfo.st_size = -1;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		// make sure to return -1 for directories
		if (S_ISDIR(FileInfo.st_mode))
		{
			FileInfo.st_size = -1;
		}
	}
	return FileInfo.st_size;
}

bool FLinuxPlatformFile::DeleteFile(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	FString IntendedFilename(NormalizeFilename(Filename));
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(IntendedFilename, CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	// removing mapped file is too dangerous
	if (IntendedFilename != CaseSensitiveFilename)
	{
		UE_LOG(LogLinuxPlatformFile, Warning, TEXT("Could not find file '%s', deleting file '%s' instead (for consistency with the rest of file ops)"), *IntendedFilename, *CaseSensitiveFilename);
	}
	return unlink(TCHAR_TO_UTF8(*CaseSensitiveFilename)) == 0;
}

bool FLinuxPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	// skipping checking F_OK since this is already taken care of by case mapper

	if (access(TCHAR_TO_UTF8(*CaseSensitiveFilename), W_OK) == -1)
	{
		return errno == EACCES;
	}
	return false;
}

bool FLinuxPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(From), CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	int32 Result = rename(TCHAR_TO_UTF8(*CaseSensitiveFilename), TCHAR_TO_UTF8(*NormalizeFilename(To)));
	if (Result == -1 && errno == EXDEV)
	{
		// Copy the file if rename failed because To and From are on different file systems
		if (CopyFile(To, *CaseSensitiveFilename))
		{
			DeleteFile(*CaseSensitiveFilename);
			Result = 0;
		}
	}
	return Result != -1;
}

bool FLinuxPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		if (bNewReadOnlyValue)
		{
			FileInfo.st_mode &= ~S_IWUSR;
		}
		else
		{
			FileInfo.st_mode |= S_IWUSR;
		}
		return chmod(TCHAR_TO_UTF8(*CaseSensitiveFilename), FileInfo.st_mode) == 0;
	}
	return false;
}

FDateTime FLinuxPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return FDateTime::MinValue();
	}
	
	// get file times
	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		if (errno == EOVERFLOW)
		{
			// hacky workaround for files mounted on Samba (see https://bugzilla.samba.org/show_bug.cgi?id=7707)
			return FDateTime::Now();
		}
		else
		{
			return FDateTime::MinValue();
		}
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
	return UnixEpoch + TimeSinceEpoch;
}

void FLinuxPlatformFile::SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return;
	}

	// get file times
	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		return;
	}

	// change the modification time only
	struct utimbuf Times;
	Times.actime = FileInfo.st_atime;
	Times.modtime = (DateTime - UnixEpoch).GetTotalSeconds();
	utime(TCHAR_TO_UTF8(*CaseSensitiveFilename), &Times);
}

FDateTime FLinuxPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		// could not find the file
		return FDateTime::MinValue();
	}

	// get file times
	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		return FDateTime::MinValue();
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
	return UnixEpoch + TimeSinceEpoch;
}

FString FLinuxPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	return Filename;
/*
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Filename), CaseSensitiveFilename))
	{
		return Filename;
	}

	return CaseSensitiveFilename;
*/
}

IFileHandle* FLinuxPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	// let the file registry manage read files
	return GFileRegistry.InitialOpenFile(*NormalizeFilename(Filename));
}

IFileHandle* FLinuxPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	int Flags = O_CREAT | O_CLOEXEC;	// prevent children from inheriting this

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

	// Caveat: cannot specify O_TRUNC in flags, as this will corrupt the file which may be "locked" by other process. We will ftruncate() it once we "lock" it
	int32 Handle = open(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), Flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (Handle != -1)
	{
		// mimic Windows "exclusive write" behavior (we don't use FILE_SHARE_WRITE) by locking the file.
		// note that the (non-mandatory) "lock" will be removed by itself when the last file descriptor is close()d
		if (flock(Handle, LOCK_EX | LOCK_NB) == -1)
		{
			// if locked, consider operation a failure
			if (EAGAIN == errno || EWOULDBLOCK == errno)
			{
				close(Handle);
				return nullptr;
			}
			// all the other locking errors are ignored.
		}

		// truncate the file now that we locked it
		if (!bAppend)
		{
			if (ftruncate(Handle, 0) != 0)
			{
				int ErrNo = errno;
				UE_LOG(LogLinuxPlatformFile, Warning, TEXT( "ftruncate() failed for '%s': errno=%d (%s)" ),
															Filename, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
				close(Handle);
				return nullptr;
			}
		}

		FFileHandleLinux* FileHandleLinux = new FFileHandleLinux(Handle, *NormalizeDirectory(Filename), true);

		if (bAppend)
		{
			FileHandleLinux->SeekFromEnd(0);
		}
		return FileHandleLinux;
	}

	int ErrNo = errno;
	UE_LOG(LogLinuxPlatformFile, Warning, TEXT( "open('%s', Flags=0x%08X) failed: errno=%d (%s)" ), *NormalizeFilename(Filename), Flags, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
	return nullptr;
}

bool FLinuxPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(Directory), CaseSensitiveFilename))
	{
		// could not find the file
		return false;
	}

	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) != -1)
	{
		return S_ISDIR(FileInfo.st_mode);
	}
	return false;
}

bool FLinuxPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	return mkdir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)), 0755) == 0;
}

bool FLinuxPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	FString CaseSensitiveFilename;
	FString IntendedFilename(NormalizeFilename(Directory));
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(IntendedFilename, CaseSensitiveFilename))
	{
		// could not find the directory
		return false;
	}

	// removing mapped directory is too dangerous
	if (IntendedFilename != CaseSensitiveFilename)
	{
		UE_LOG(LogLinuxPlatformFile, Warning, TEXT("Could not find directory '%s', deleting '%s' instead (for consistency with the rest of file ops)"), *IntendedFilename, *CaseSensitiveFilename);
	}
	return rmdir(TCHAR_TO_UTF8(*CaseSensitiveFilename)) == 0;
}

FFileStatData FLinuxPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	FString CaseSensitiveFilename;
	if (!GCaseInsensMapper.MapCaseInsensitiveFile(NormalizeFilename(FilenameOrDirectory), CaseSensitiveFilename))
	{
		// could not find the file
		return FFileStatData();
	}

	struct stat FileInfo;
	if(stat(TCHAR_TO_UTF8(*CaseSensitiveFilename), &FileInfo) == -1)
	{
		return FFileStatData();
	}

	return UnixStatToUEFileData(FileInfo);
}

bool FLinuxPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	const FString DirectoryStr = Directory;
	const FString NormalizedDirectoryStr = NormalizeFilename(Directory);

	return IterateDirectoryCommon(Directory, [&](struct dirent* InEntry) -> bool
	{
		const FString UnicodeEntryName = UTF8_TO_TCHAR(InEntry->d_name);
				
		bool bIsDirectory = false;
		if (InEntry->d_type != DT_UNKNOWN && InEntry->d_type != DT_LNK)
		{
			bIsDirectory = InEntry->d_type == DT_DIR;
		}
		else
		{
			// either filesystem does not support d_type (e.g. a network one or non-native) or we're dealing with a symbolic link, fallback to stat
			struct stat FileInfo;
			const FString AbsoluteUnicodeName = NormalizedDirectoryStr / UnicodeEntryName;
			if (stat(TCHAR_TO_UTF8(*AbsoluteUnicodeName), &FileInfo) != -1)
			{
				bIsDirectory = ((FileInfo.st_mode & S_IFMT) == S_IFDIR);
			}
			else
			{
				int ErrNo = errno;
				UE_LOG(LogLinuxPlatformFile, Warning, TEXT( "Cannot determine whether '%s' is a directory - d_type not supported and stat() failed with errno=%d (%s)"), *AbsoluteUnicodeName, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
			}
		}

		return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), bIsDirectory);
	});
}

bool FLinuxPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
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
			return Visitor.Visit(*(DirectoryStr / UnicodeEntryName), UnixStatToUEFileData(FileInfo));
		}

		return true;
	});
}

bool FLinuxPlatformFile::IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(struct dirent*)>& Visitor)
{
	bool Result = false;

	FString NormalizedDirectory = NormalizeFilename(Directory);
	DIR* Handle = opendir(TCHAR_TO_UTF8(*NormalizedDirectory));
	if (Handle)
	{
		Result = true;
		struct dirent* Entry;
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

bool FLinuxPlatformFile::CreateDirectoriesFromPath(const TCHAR* Path)
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
					UE_LOG(LogLinuxPlatformFile, Warning, TEXT( "create dir('%s') failed: errno=%d (%s)" ),
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

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FLinuxPlatformFile Singleton;
	return Singleton;
}

FRegisteredFileHandle* FLinuxFileRegistry::PlatformInitialOpenFile(const TCHAR* Filename)
{
	FString MappedToName;
	int32 Handle = GCaseInsensMapper.OpenCaseInsensitiveRead(Filename, MappedToName);
	if (Handle != -1)
	{
		return new FFileHandleLinux(Handle, *MappedToName, false);
	}
	return nullptr;
}

bool FLinuxFileRegistry::PlatformReopenFile(FRegisteredFileHandle* Handle)
{
	FFileHandleLinux* LinuxHandle = static_cast<FFileHandleLinux*>(Handle);

	bool bSuccess = true;
	LinuxHandle->FileHandle = open(TCHAR_TO_UTF8(*(LinuxHandle->Filename)), O_RDONLY | O_CLOEXEC);
	if(LinuxHandle->FileHandle != -1)
	{
		if (lseek(LinuxHandle->FileHandle, LinuxHandle->FileOffset, SEEK_SET) == -1)
		{
			UE_LOG(LogLinuxPlatformFile, Warning, TEXT("Could not seek to the previous position on handle for file '%s'"), *(LinuxHandle->Filename));
			bSuccess = false;
		}
	}
	else
	{
		UE_LOG(LogLinuxPlatformFile, Warning, TEXT("Could not reopen handle for file '%s'"), *(LinuxHandle->Filename));
		bSuccess = false;
	}

	return bSuccess;
}

void FLinuxFileRegistry::PlatformCloseFile(FRegisteredFileHandle* Handle)
{
	FFileHandleLinux* LinuxHandle = static_cast<FFileHandleLinux*>(Handle);
	close(LinuxHandle->FileHandle);
}

