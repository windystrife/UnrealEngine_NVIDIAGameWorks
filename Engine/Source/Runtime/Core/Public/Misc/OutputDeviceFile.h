// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

/** string added to the filename of timestamped backup log files */
#define BACKUP_LOG_FILENAME_POSTFIX TEXT("-backup-")

class FAsyncWriter;

/** Used by FOutputDeviceFile to write to a file on a separate thread */
class FAsyncWriter;

enum class EByteOrderMark : int8
{
	UTF8,
	Unspecified,
};

/**
* File output device (Note: Only works if ALLOW_LOG_FILE && !NO_LOGGING is true, otherwise Serialize does nothing).
*/
class CORE_API FOutputDeviceFile : public FOutputDevice
{
public:
	/**
	* Constructor, initializing member variables.
	*
	* @param InFilename	Filename to use, can be nullptr
	* @param bDisableBackup If true, existing files will not be backed up
	*/
	FOutputDeviceFile(const TCHAR* InFilename = nullptr, bool bDisableBackup = false);

	/**
	* Destructor to perform teardown
	*
	*/
	~FOutputDeviceFile();

	/** Sets the filename that the output device writes to.  If the output device was already writing to a file, closes that file. */
	void SetFilename(const TCHAR* InFilename);

	//~ Begin FOutputDevice Interface.
	/**
	* Closes output device and cleans up. This can't happen in the destructor
	* as we have to call "delete" which cannot be done for static/ global
	* objects.
	*/
	void TearDown() override;

	/**
	* Flush the write cache so the file isn't truncated in case we crash right
	* after calling this function.
	*/
	void Flush() override;

	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time) override;
	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}
	//~ End FOutputDevice Interface.

	/** Creates a backup copy of a log file if it already exists */
	static void CreateBackupCopy(const TCHAR* Filename);

	/** Checks if the filename represents a backup copy of a log file */
	static bool IsBackupCopy(const TCHAR* Filename);

private:

	/** Writes to a file on a separate thread */
	FAsyncWriter* AsyncWriter;
	/** Archive used by the async writer */
	FArchive* WriterArchive;

	TCHAR Filename[1024];
	bool Opened;
	bool Dead;

	/** If true, existing files will not be backed up */
	bool		bDisableBackup;

	void WriteRaw(const TCHAR* C);

	/** Creates the async writer and its archive. Returns true if successful.  */
	bool CreateWriter(uint32 MaxAttempts = 32);

	void WriteByteOrderMarkToArchive(EByteOrderMark ByteOrderMark);
};
