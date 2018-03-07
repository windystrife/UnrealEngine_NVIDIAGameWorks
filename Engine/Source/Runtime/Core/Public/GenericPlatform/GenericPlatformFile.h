// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformFile.h: Generic platform file interfaces
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/EnumClassFlags.h"

class IAsyncReadFileHandle;

/**
* Enum for async IO priorities.
*/
enum EAsyncIOPriority
{
	AIOP_MIN = 0,
	AIOP_Precache = AIOP_MIN, 
	AIOP_Low,
	AIOP_BelowNormal,
	AIOP_Normal,
	AIOP_High,
	AIOP_CriticalPath,
	AIOP_MAX = AIOP_CriticalPath,
	AIOP_NUM
};

/**
 * Enum for platform file read flags
 */
enum class EPlatformFileRead : uint8
{
	None = 0x0,
	AllowWrite = 0x01	// attempts to open for read while allowing others to write
};

ENUM_CLASS_FLAGS(EPlatformFileRead);

/**
 * Enum for platform file write flags
 */
enum class EPlatformFileWrite : uint8
{
	None = 0x0,
	AllowRead = 0x01	// attempts to open for write while allowing others to read
};

ENUM_CLASS_FLAGS(EPlatformFileWrite);

/** 
 * File handle interface. 
**/
class CORE_API IFileHandle
{
public:
	/** Destructor, also the only way to close the file handle **/
	virtual ~IFileHandle()
	{
	}

	/** Return the current write or read position. **/
	virtual int64		Tell() = 0;
	/** 
	 * Change the current write or read position. 
	 * @param NewPosition	new write or read position
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Seek(int64 NewPosition) = 0;

	/** 
	 * Change the current write or read position, relative to the end of the file.
	 * @param NewPositionRelativeToEnd	new write or read position, relative to the end of the file should be <=0!
	 * @return							true if the operation completed successfully.
	**/
	virtual bool		SeekFromEnd(int64 NewPositionRelativeToEnd = 0) = 0;

	/** 
	 * Read bytes from the file.
	 * @param Destination	Buffer to holds the results, should be at least BytesToRead in size.
	 * @param BytesToRead	Number of bytes to read into the destination.
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Read(uint8* Destination, int64 BytesToRead) = 0;

	/** 
	 * Write bytes to the file.
	 * @param Source		Buffer to write, should be at least BytesToWrite in size.
	 * @param BytesToWrite	Number of bytes to write.
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Write(const uint8* Source, int64 BytesToWrite) = 0;

	/**
	 * Flushes file.
	**/
	virtual void		Flush() { };

public:
	/////////// Utility Functions. These have a default implementation that uses the pure virtual operations.

	/** Return the total size of the file **/
	virtual int64		Size();
};


/**
 * Contains the information that's returned from stat'ing a file or directory 
 */
struct FFileStatData
{
	FFileStatData()
		: CreationTime(FDateTime::MinValue())
		, AccessTime(FDateTime::MinValue())
		, ModificationTime(FDateTime::MinValue())
		, FileSize(-1)
		, bIsDirectory(false)
		, bIsReadOnly(false)
		, bIsValid(false)
	{
	}

	FFileStatData(FDateTime InCreationTime, FDateTime InAccessTime,	FDateTime InModificationTime, const int64 InFileSize, const bool InIsDirectory, const bool InIsReadOnly)
		: CreationTime(InCreationTime)
		, AccessTime(InAccessTime)
		, ModificationTime(InModificationTime)
		, FileSize(InFileSize)
		, bIsDirectory(InIsDirectory)
		, bIsReadOnly(InIsReadOnly)
		, bIsValid(true)
	{
	}

	/** The time that the file or directory was originally created, or FDateTime::MinValue if the creation time is unknown */
	FDateTime CreationTime;

	/** The time that the file or directory was last accessed, or FDateTime::MinValue if the access time is unknown */
	FDateTime AccessTime;

	/** The time the the file or directory was last modified, or FDateTime::MinValue if the modification time is unknown */
	FDateTime ModificationTime;

	/** Size of the file (in bytes), or -1 if the file size is unknown */
	int64 FileSize;

	/** True if this data is for a directory, false if it's for a file */
	bool bIsDirectory : 1;

	/** True if this file is read-only */
	bool bIsReadOnly : 1;

	/** True if file or directory was found, false otherwise. Note that this value being true does not ensure that the other members are filled in with meaningful data, as not all file systems have access to all of this data */
	bool bIsValid : 1;
};


/**
* File I/O Interface
**/
class CORE_API IPlatformFile
{
public:
	/** Physical file system of the _platform_, never wrapped. **/
	static IPlatformFile& GetPlatformPhysical();
	/** Returns the name of the physical platform file type. */
	static const TCHAR* GetPhysicalTypeName();
	/** Destructor. */
	virtual ~IPlatformFile() {}

	/**
	 *	Set whether the sandbox is enabled or not
	 *
	 *	@param	bInEnabled		true to enable the sandbox, false to disable it
	 */
	virtual void SetSandboxEnabled(bool bInEnabled)
	{
	}

	/**
	 *	Returns whether the sandbox is enabled or not
	 *
	 *	@return	bool			true if enabled, false if not
	 */
	virtual bool IsSandboxEnabled() const
	{
		return false;
	}

	/**
	 * Checks if this platform file should be used even though it was not asked to be.
	 * i.e. pak files exist on disk so we should use a pak file
	 */
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
	{
		return false;
	}

	/**
	 * Initializes platform file.
	 *
	 * @param Inner Platform file to wrap by this file.
	 * @param CmdLine Command line to parse.
	 * @return true if the initialization was successful, false otherise. */
	virtual bool		Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) = 0;

	/**
	 * Performs initialization of the platform file after it has become the active (FPlatformFileManager.GetPlatformFile() will return this
	 */
	virtual void		InitializeAfterSetActive() { }

	/**
	* Performs initialization of the platform file after the new async IO has been enabled
	*/
	virtual void		InitializeNewAsyncIO() { }

	/**
	 * Identifies any platform specific paths that are guaranteed to be local (i.e. cache, scratch space)
	 */
	virtual void		AddLocalDirectories(TArray<FString> &LocalDirectories)
	{
		if (GetLowerLevel())
		{
			GetLowerLevel()->AddLocalDirectories(LocalDirectories);
		}
	}

	virtual void		BypassSecurity(bool bInBypass)
	{
		if (GetLowerLevel() != nullptr)
		{
			GetLowerLevel()->BypassSecurity(bInBypass);
		}
	}

	/** Platform file can override this to get a regular tick from the engine */
	virtual void Tick() { }
	/** Gets the platform file wrapped by this file. */
	virtual IPlatformFile* GetLowerLevel() = 0;
	/** Sets the platform file wrapped by this file. */
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) = 0;
	/** Gets this platform file type name. */
	virtual const TCHAR* GetName() const = 0;
	/** Return true if the file exists. **/
	virtual bool		FileExists(const TCHAR* Filename) = 0;
	/** Return the size of the file, or -1 if it doesn't exist. **/
	virtual int64		FileSize(const TCHAR* Filename) = 0;
	/** Delete a file and return true if the file exists. Will not delete read only files. **/
	virtual bool		DeleteFile(const TCHAR* Filename) = 0;
	/** Return true if the file is read only. **/
	virtual bool		IsReadOnly(const TCHAR* Filename) = 0;
	/** Attempt to move a file. Return true if successful. Will not overwrite existing files. **/
	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) = 0;
	/** Attempt to change the read only status of a file. Return true if successful. **/
	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) = 0;
	/** Return the modification time of a file. Returns FDateTime::MinValue() on failure **/
	virtual FDateTime	GetTimeStamp(const TCHAR* Filename) = 0;
	/** Sets the modification time of a file **/
	virtual void		SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) = 0;
	/** Return the last access time of a file. Returns FDateTime::MinValue() on failure **/
	virtual FDateTime	GetAccessTimeStamp(const TCHAR* Filename) = 0;
	/** For case insensitive filesystems, returns the full path of the file with the same case as in the filesystem */
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) = 0;

	/** Attempt to open a file for reading.
	 *
	 * @param Filename file to be opened
	 * @param bAllowWrite (applies to certain platforms only) whether this file is allowed to be written to by other processes. This flag is needed to open files that are currently being written to as well.
	 *
	 * @return If successful will return a non-nullptr pointer. Close the file by delete'ing the handle.
	 */
	virtual IFileHandle*	OpenRead(const TCHAR* Filename, bool bAllowWrite = false) = 0;

	virtual IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false)
	{
		return OpenRead(Filename, bAllowWrite);
	}


	/** Attempt to open a file for writing. If successful will return a non-nullptr pointer. Close the file by delete'ing the handle. **/
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) = 0;

	/** Return true if the directory exists. **/
	virtual bool		DirectoryExists(const TCHAR* Directory) = 0;
	/** Create a directory and return true if the directory was created or already existed. **/
	virtual bool		CreateDirectory(const TCHAR* Directory) = 0;
	/** Delete a directory and return true if the directory was deleted or otherwise does not exist. **/
	virtual bool		DeleteDirectory(const TCHAR* Directory) = 0;

	/** Return the stat data for the given file or directory. Check the FFileStatData::bIsValid member before using the returned data */
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) = 0;

	/** Base class for file and directory visitors that take only the name. **/
	class FDirectoryVisitor
	{
	public:
		virtual ~FDirectoryVisitor() { }

		/** 
		 * Callback for a single file or a directory in a directory iteration.
		 * @param FilenameOrDirectory		If bIsDirectory is true, this is a directory (with no trailing path delimiter), otherwise it is a file name.
		 * @param bIsDirectory				true if FilenameOrDirectory is a directory.
		 * @return							true if the iteration should continue.
		**/
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) = 0;
	};

	/** Base class for file and directory visitors that take all the stat data. **/
	class FDirectoryStatVisitor
	{
	public:
		virtual ~FDirectoryStatVisitor() { }
		/** 
		 * Callback for a single file or a directory in a directory iteration.
		 * @param FilenameOrDirectory		If bIsDirectory is true, this is a directory (with no trailing path delimiter), otherwise it is a file name.
		 * @param StatData					The stat data for the file or directory.
		 * @return							true if the iteration should continue.
		**/
		virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData) = 0;
	};

	/** 
	 * Call the Visit function of the visitor once for each file or directory in a single directory. This function does not explore subdirectories.
	 * @param Directory		The directory to iterate the contents of.
	 * @param Visitor		Visitor to call for each element of the directory
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool		IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) = 0;

	/** 
	 * Call the Visit function of the visitor once for each file or directory in a single directory. This function does not explore subdirectories.
	 * @param Directory		The directory to iterate the contents of.
	 * @param Visitor		Visitor to call for each element of the directory
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool		IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) = 0;


	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////// Utility Functions. These have a default implementation that uses the pure virtual operations.
	/////////// Generally, these do not need to be implemented per platform.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////

	/** Open a file for async reading. This call does not hit the disk or block.
	*
	* @param Filename file to be opened
	* @return Close the file by delete'ing the handle. A non-null return value does not mean the file exists, since that may not be determined yet.
	*/
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename);

	virtual void GetTimeStampPair(const TCHAR* PathA, const TCHAR* PathB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB);

	/** Return the modification time of a file in the local time of the calling code (GetTimeStamp returns UTC). Returns FDateTime::MinValue() on failure **/
	virtual FDateTime	GetTimeStampLocal(const TCHAR* Filename);

	/** 
	 * Call the Visit function of the visitor once for each file or directory in a directory tree. This function explores subdirectories.
	 * @param Directory		The directory to iterate the contents of, recursively.
	 * @param Visitor		Visitor to call for each element of the directory and each element of all subdirectories.
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor);

	/** 
	 * Call the Visit function of the visitor once for each file or directory in a directory tree. This function explores subdirectories.
	 * @param Directory		The directory to iterate the contents of, recursively.
	 * @param Visitor		Visitor to call for each element of the directory and each element of all subdirectories.
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor);
		
	/**
	 * Finds all the files within the given directory, with optional file extension filter
	 * @param Directory			The directory to iterate the contents of
	 * @param FileExtension		If FileExtension is NULL, or an empty string "" then all files are found.
	 * 							Otherwise FileExtension can be of the form .EXT or just EXT and only files with that extension will be returned.
	 * @return FoundFiles		All the files that matched the optional FileExtension filter, or all files if none was specified.
	 */
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);

	/**
	 * Finds all the files within the directory tree, with optional file extension filter
	 * @param Directory			The starting directory to iterate the contents. This function explores subdirectories
	 * @param FileExtension		If FileExtension is NULL, or an empty string "" then all files are found.
	 * 							Otherwise FileExtension can be of the form .EXT or just EXT and only files with that extension will be returned.
	 * @return FoundFiles		All the files that matched the optional FileExtension filter, or all files if none was specified.
	 */
	virtual void FindFilesRecursively(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);

	/** 
	 * Delete all files and subdirectories in a directory, then delete the directory itself
	 * @param Directory		The directory to delete.
	 * @return				true if the directory was deleted or did not exist.
	**/
	virtual bool DeleteDirectoryRecursively(const TCHAR* Directory);

	/** Create a directory, including any parent directories and return true if the directory was created or already existed. **/
	virtual bool CreateDirectoryTree(const TCHAR* Directory);

	/** 
	 * Copy a file. This will fail if the destination file already exists.
	 * @param To				File to copy to.
	 * @param From				File to copy from.
	 * @param ReadFlags			Source file read options.
	 * @param WriteFlags		Destination file write options.
	 * @return			true if the file was copied sucessfully.
	**/
	virtual bool CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None);

	/** 
	 * Copy a file or a hierarchy of files (directory).
	 * @param DestinationDirectory			Target path (either absolute or relative) to copy to - always a directory! (e.g. "/home/dest/").
	 * @param Source						Source file (or directory) to copy (e.g. "/home/source/stuff").
	 * @param bOverwriteAllExisting			Whether to overwrite everything that exists at target
	 * @return								true if operation completed successfully.
	 */
	virtual bool CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting);

	/**
	 * Converts passed in filename to use an absolute path (for reading).
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename );

	/**
	 * Converts passed in filename to use an absolute path (for writing)
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename );

	/**
	 * Helper class to send/receive data to the file server function
	 */
	class IFileServerMessageHandler
	{
	public:
		virtual ~IFileServerMessageHandler() { }

		/** Subclass fills out an archive to send to the server */
		virtual void FillPayload(FArchive& Payload) = 0;

		/** Subclass pulls data response from the server */
		virtual void ProcessResponse(FArchive& Response) = 0;
	};

	/**
	 * Sends a message to the file server, and will block until it's complete. Will return 
	 * immediately if the file manager doesn't support talking to a server.
	 *
	 * @param Message	The string message to send to the server
	 *
	 * @return			true if the message was sent to server and it returned success, or false if there is no server, or the command failed
	 */
	virtual bool SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler)
	{
		// by default, IPlatformFile's can't talk to a server
		return false;
	}
};

/**
* Common base for physical platform File I/O Interface
**/
class CORE_API IPhysicalPlatformFile : public IPlatformFile
{
public:
	//~ Begin IPlatformFile Interface
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
		return true;
	}
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual IPlatformFile* GetLowerLevel() override
	{
		return nullptr;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		check(false); // can't override wrapped platform file for physical platform file
	}
	virtual const TCHAR* GetName() const override
	{
		return IPlatformFile::GetPhysicalTypeName();
	}
	//~ End IPlatformFile Interface
};
