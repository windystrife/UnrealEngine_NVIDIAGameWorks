// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceFile.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Serialization/Archive.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformOutputDevices.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceHelper.h"
#include "Math/Color.h"

/** Used by tools which include only core to disable log file creation. */
#ifndef ALLOW_LOG_FILE
	#define ALLOW_LOG_FILE 1
#endif

typedef uint8 UTF8BOMType[3];
static UTF8BOMType UTF8BOM = { 0xEF, 0xBB, 0xBF };

/**
* Thread heartbeat check class.
* Used by crash handling code to check for hangs. 
* [] tags identify which thread owns a variable or function
*/
class CORE_API FAsyncWriter : public FRunnable, public FArchive
{
	enum EConstants
	{
		InitialBufferSize = 128 * 1024
	};

	/** Thread to run the worker FRunnable on. Serializes the ring buffer to disk. */
	volatile FRunnableThread* Thread;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;

	/** Writer archive */
	FArchive& Ar;
	/** Data ring buffer */
	TArray<uint8> Buffer;
	/** [WRITER THREAD] Position where the unserialized data starts in the buffer */
	volatile int32 BufferStartPos;
	/** [CLIENT THREAD] Position where the unserialized data ends in the buffer (such as if (BufferEndPos > BufferStartPos) Length = BufferEndPos - BufferStartPos; */
	volatile int32 BufferEndPos;
	/** [CLIENT THREAD] Sync object for the buffer pos */
	FCriticalSection BufferPosCritical;
	/** [CLIENT/WRITER THREAD] Outstanding serialize request counter. This is to make sure we flush all requests. */
	FThreadSafeCounter SerializeRequestCounter;
	/** [CLIENT/WRITER THREAD] Tells the writer thread, the client requested flush. */
	FThreadSafeCounter WantsArchiveFlush;

	/** [WRITER THREAD] Last time the archive was flushed. used in threaded situations to flush the underlying archive at a certain maximum rate. */
	double LastArchiveFlushTime;

	/** [WRITER THREAD] Archive flush interval. */
	double ArchiveFlushIntervalSec;

	/** [WRITER THREAD] Flushes the archive and reset the flush timer. */
	void FlushArchiveAndResetTimer()
	{
		// This should be the one and only place where we flush because we want the flush to happen only on the 
		// async writer thread (if threading is enabled)
		Ar.Flush();
		LastArchiveFlushTime = FPlatformTime::Seconds();
	}

	/** [WRITER THREAD] Serialize the contents of the ring buffer to disk */
	void SerializeBufferToArchive()
	{
		SCOPED_NAMED_EVENT(FAsyncWriter_SerializeBufferToArchive, FColor::Cyan);
		while (SerializeRequestCounter.GetValue() > 0)
		{
			// Grab a local copy of the end pos. It's ok if it changes on the client thread later on.
			// We won't be modifying it anyway and will later serialize new data in the next iteration.
			// Here we only serialize what we know exists at the beginning of this function.
			int32 ThisThreadEndPos = BufferEndPos;

			if (ThisThreadEndPos >= BufferStartPos)
			{
				Ar.Serialize(Buffer.GetData() + BufferStartPos, ThisThreadEndPos - BufferStartPos);
			}
			else
			{
				// Data is wrapped around the ring buffer
				Ar.Serialize(Buffer.GetData() + BufferStartPos, Buffer.Num() - BufferStartPos);
				Ar.Serialize(Buffer.GetData(), BufferEndPos);
			}
			// Modify the start pos. Only the worker thread modifies this value so it's ok to not guard it with a critical section.
			FPlatformAtomics::InterlockedExchange(&BufferStartPos, ThisThreadEndPos);

			// Decrement the request counter, we now know we serialized at least one request.
			// We might have serialized more requests but it's irrelevant, the counter will go down to 0 eventually
			SerializeRequestCounter.Decrement();

			// Flush the archive periodically if running on a separate thread
			if (Thread)
			{
				if ((FPlatformTime::Seconds() - LastArchiveFlushTime) > ArchiveFlushIntervalSec)
				{
					FlushArchiveAndResetTimer();
				}
			}
			// If no threading is available or when we explicitly requested flush (see FlushBuffer), flush immediately after writing.
			// In some rare cases we may flush twice (see above) but that's ok. We need a clear division between flushing because of the timer
			// and force flush on demand.
			if (WantsArchiveFlush.GetValue() > 0)
			{
				FlushArchiveAndResetTimer();
				int32 FlushCount = WantsArchiveFlush.Decrement();
				check(FlushCount >= 0);
			}
		}
	}

	/** [CLIENT THREAD] Flush the memory buffer (doesn't force the archive to flush). Can only be used from inside of BufferPosCritical lock. */
	void FlushBuffer()
	{
		SerializeRequestCounter.Increment();
		if (!Thread)
		{
			SerializeBufferToArchive();
		}
		while (SerializeRequestCounter.GetValue() != 0)
		{
			FPlatformProcess::SleepNoStats(0);
		}
		// Make sure there's been no unexpected concurrency
		check(SerializeRequestCounter.GetValue() == 0);
	}

public:

	FAsyncWriter(FArchive& InAr)
		: Thread(nullptr)
		, Ar(InAr)
		, BufferStartPos(0)
		, BufferEndPos(0)
		, LastArchiveFlushTime(0.0)
		, ArchiveFlushIntervalSec(0.2)
	{
		Buffer.AddUninitialized(InitialBufferSize);

		float CommandLineInterval = 0.0;
		if (FParse::Value(FCommandLine::Get(), TEXT("LOGFLUSHINTERVAL="), CommandLineInterval))
		{
			ArchiveFlushIntervalSec = CommandLineInterval;
		}

		if (FPlatformProcess::SupportsMultithreading())
		{
			FString WriterName = FString::Printf(TEXT("FAsyncWriter_%s"), *FPaths::GetBaseFilename(Ar.GetArchiveName()));
			FPlatformAtomics::InterlockedExchangePtr((void**)&Thread, FRunnableThread::Create(this, *WriterName, 0, TPri_BelowNormal));
		}
	}

	virtual ~FAsyncWriter()
	{
		Flush();
		delete Thread;
		Thread = nullptr;
	}

	/** [CLIENT THREAD] Serialize data to buffer that will later be saved to disk by the async thread */
	virtual void Serialize(void* InData, int64 Length) override
	{
		if (!InData || Length <= 0)
		{
			return;
		}

		const uint8* Data = (uint8*)InData;

		FScopeLock WriteLock(&BufferPosCritical);

		// Store the local copy of the current buffer start pos. It may get moved by the worker thread but we don't
		// care about it too much because we only modify BufferEndPos. Copy should be atomic enough. We only use it
		// for checking the remaining space in the buffer so underestimating is ok.
		{
			const int32 ThisThreadStartPos = BufferStartPos;
			// Calculate the remaining size in the ring buffer
			const int32 BufferFreeSize = ThisThreadStartPos <= BufferEndPos ? (Buffer.Num() - BufferEndPos + ThisThreadStartPos) : (ThisThreadStartPos - BufferEndPos);
			// Make sure the buffer is BIGGER than we require otherwise we may calculate the wrong (0) buffer EndPos for StartPos = 0 and Length = Buffer.Num()
			if (BufferFreeSize <= Length)
			{
				// Force the async thread to call SerializeBufferToArchive even if it's currently empty
				FlushBuffer();

				// Resize the buffer if needed
				if (Length >= Buffer.Num())
				{
					// Keep the buffer bigger than we require so that % Buffer.Num() does not return 0 for Lengths = Buffer.Num()
					Buffer.SetNumUninitialized(Length + 1);
				}
			}
		}

		// We now know there's enough space in the buffer to copy data
		const int32 WritePos = BufferEndPos;
		if ((WritePos + Length) <= Buffer.Num())
		{
			// Copy straight into the ring buffer
			FMemory::Memcpy(Buffer.GetData() + WritePos, Data, Length);
		}
		else
		{
			// Wrap around the ring buffer
			int32 BufferSizeToEnd = Buffer.Num() - WritePos;
			FMemory::Memcpy(Buffer.GetData() + WritePos, Data, BufferSizeToEnd);
			FMemory::Memcpy(Buffer.GetData(), Data + BufferSizeToEnd, Length - BufferSizeToEnd);
		}

		// Update the end position and let the async thread know we need to write to disk
		FPlatformAtomics::InterlockedExchange(&BufferEndPos, (BufferEndPos + Length) % Buffer.Num());
		SerializeRequestCounter.Increment();

		// No async thread? Serialize now.
		if (!Thread)
		{
			SerializeBufferToArchive();
		}
	}

	/** Flush all buffers to disk */
	void Flush()
	{
		FScopeLock WriteLock(&BufferPosCritical);
		WantsArchiveFlush.Increment();
		FlushBuffer();
	}

	//~ Begin FRunnable Interface.
	virtual bool Init() 
	{
		return true;
	}
	virtual uint32 Run()
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (SerializeRequestCounter.GetValue() > 0)
			{
				SerializeBufferToArchive();
			}
			else if ((FPlatformTime::Seconds() - LastArchiveFlushTime) > ArchiveFlushIntervalSec)
			{
				FlushArchiveAndResetTimer();
			}
			else
			{
				FPlatformProcess::SleepNoStats(0.01f);
			}
		}
		return 0;
	}
	virtual void Stop()
	{
		StopTaskCounter.Increment();
	}
	//~ End FRunnable Interface
};

/** 
 * Constructor, initializing member variables.
 *
 * @param InFilename		Filename to use, can be NULL
 * @param bInDisableBackup	If true, existing files will not be backed up
 */
FOutputDeviceFile::FOutputDeviceFile( const TCHAR* InFilename, bool bInDisableBackup  )
: AsyncWriter(nullptr)
, WriterArchive(nullptr)
, Opened(false)
, Dead(false)
, bDisableBackup(bInDisableBackup)
{
	if( InFilename )
	{
		FCString::Strncpy( Filename, InFilename, ARRAY_COUNT(Filename) );
	}
	else
	{
		Filename[0]	= 0;
	}
}

/**
* Destructor to perform teardown
*
*/
FOutputDeviceFile::~FOutputDeviceFile()
{
	TearDown();
}

void FOutputDeviceFile::SetFilename(const TCHAR* InFilename)
{
	// Close any existing file.
	TearDown();

	FCString::Strncpy( Filename, InFilename, ARRAY_COUNT(Filename) );
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceFile::TearDown()
{
	if (AsyncWriter)
	{
		if (!bSuppressEventTag)
		{			
			Logf(TEXT("Log file closed, %s"), FPlatformTime::StrTimestamp());
		}
		delete AsyncWriter;
		AsyncWriter = nullptr;
	}
	delete WriterArchive;
	WriterArchive = nullptr;

	Filename[0] = 0;
}

/**
 * Flush the write cache so the file isn't truncated in case we crash right
 * after calling this function.
 */
void FOutputDeviceFile::Flush()
{
	if (AsyncWriter)
	{
		AsyncWriter->Flush();
	}
}

/** if the passed in file exists, makes a timestamped backup copy
 * @param Filename the name of the file to check
 */
void FOutputDeviceFile::CreateBackupCopy(const TCHAR* Filename)
{
	if (IFileManager::Get().FileSize(Filename) > 0)
	{
		FString Name, Extension;
		FString(Filename).Split(TEXT("."), &Name, &Extension, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		FString BackupFilename = FString::Printf(TEXT("%s%s%s.%s"), *Name, BACKUP_LOG_FILENAME_POSTFIX, *GSystemStartTime, *Extension);
		IFileManager::Get().Copy(*BackupFilename, Filename, false);
	}
}

bool FOutputDeviceFile::IsBackupCopy(const TCHAR* Filename)
{
	return Filename != nullptr && FCString::Stristr(const_cast<TCHAR*>(Filename), BACKUP_LOG_FILENAME_POSTFIX) != nullptr;
}

void FOutputDeviceFile::WriteByteOrderMarkToArchive(EByteOrderMark ByteOrderMark)
{
	switch (ByteOrderMark)
	{
	case EByteOrderMark::UTF8:
		AsyncWriter->Serialize(UTF8BOM, sizeof(UTF8BOM));
		break;

	case EByteOrderMark::Unspecified:
	default:
		check(false);
		break;
	}
}

bool FOutputDeviceFile::CreateWriter(uint32 MaxAttempts)
{
	uint32 WriteFlags = FILEWRITE_AllowRead | (Opened ? FILEWRITE_Append : 0);

	// Open log file.
	FArchive* Ar = IFileManager::Get().CreateFileWriter(Filename, WriteFlags);

	// If that failed, append an _2 and try again (unless we don't want extra copies). This 
	// happens in the case of running a server and client on same computer for example.
	if (!bDisableBackup && !Ar)
	{
		FString FilenamePart = FPaths::GetBaseFilename(Filename, false) + "_";
		FString ExtensionPart = FPaths::GetExtension(Filename, true);
		FString FinalFilename;
		uint32 FileIndex = 2;
		do
		{
			// Continue to increment indices until a valid filename is found
			FinalFilename = FilenamePart + FString::FromInt(FileIndex++) + ExtensionPart;
			if (!Opened)
			{
				CreateBackupCopy(*FinalFilename);
			}
			Ar = IFileManager::Get().CreateFileWriter(*FinalFilename, WriteFlags);
		} while (!Ar && FileIndex < MaxAttempts);
	}

	FAsyncWriter* Result = nullptr;
	if (Ar)
	{
		WriterArchive = Ar;
		AsyncWriter = new FAsyncWriter(*WriterArchive);
	}

	return !!AsyncWriter;
}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceFile::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
#if ALLOW_LOG_FILE && !NO_LOGGING
	static bool Entry = false;
	if( !GIsCriticalError || Entry )
	{
		if (!AsyncWriter && !Dead)
		{
			// Make log filename.
			if( !Filename[0] )
			{
				FCString::Strcpy(Filename, *FPlatformOutputDevices::GetAbsoluteLogFilename());
			}

			// if the file already exists, create a backup as we are going to overwrite it
			if (!bDisableBackup && !Opened)
			{
				CreateBackupCopy(Filename);
			}

			// Open log file and create the worker thread.
			if (CreateWriter())
			{
				Opened = true;

				WriteByteOrderMarkToArchive(EByteOrderMark::UTF8);

				if (!bSuppressEventTag)
				{
					Logf( TEXT("Log file open, %s"), FPlatformTime::StrTimestamp() );
				}
			}
			else 
			{
				Dead = true;
			}
		}

		if (AsyncWriter && Verbosity != ELogVerbosity::SetColor)
		{
			FOutputDeviceHelper::FormatCastAndSerializeLine(*AsyncWriter, Data, Verbosity, Category, Time, bSuppressEventTag, bAutoEmitLineTerminator);

			static bool GForceLogFlush = false;
			static bool GTestedCmdLine = false;
			if (!GTestedCmdLine)
			{
				GTestedCmdLine = true;
				// Force a log flush after each line
				GForceLogFlush = FParse::Param( FCommandLine::Get(), TEXT("FORCELOGFLUSH") );
			}
			if (GForceLogFlush)
			{
				AsyncWriter->Flush();
			}
		}
	}
	else
	{
		Entry = true;
		Serialize(Data, Verbosity, Category, Time);
		Entry = false;
	}
#endif
}

void FOutputDeviceFile::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize(Data, Verbosity, Category, -1.0);
}


void FOutputDeviceFile::WriteRaw( const TCHAR* C )
{
	AsyncWriter->Serialize((uint8*)const_cast<TCHAR*>(C), FCString::Strlen(C)*sizeof(TCHAR));
}
