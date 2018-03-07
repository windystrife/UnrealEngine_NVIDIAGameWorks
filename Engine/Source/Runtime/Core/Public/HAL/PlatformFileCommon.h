// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadSafeCounter.h"
#include "ScopeLock.h"

#include "ThreadSafeCounter.h"
#include "ScopeLock.h"
#include "GenericPlatformFile.h"
#include "PlatformProcess.h"


#define MANAGE_FILE_HANDLES (#) // this is not longer used, this will error on any attempt to use it

#define SPEW_DISK_UTLIZATION (0)

#if SPEW_DISK_UTLIZATION

static const float DiskUtilizationTrackerPrintFrequency = 0.1f;

struct FDiskUtilizationTracker
{
	FCriticalSection Crit;
	int32 NumRequests;
	double LastTime;
	double LastPrint;
	double WorkTime;
	double IdleTime;
	uint64 AmountRead;
	uint64 NumSeeks;
	uint64 TotalSeekDistance;
	uint64 NumReads;

	FDiskUtilizationTracker()
		: NumRequests(0)
		, LastTime(-1.0)
		, LastPrint(-1.0)
		, WorkTime(0.0)
		, IdleTime(0.0)
		, AmountRead(0)
		, NumSeeks(0)
		, TotalSeekDistance(0)
		, NumReads(0)
	{
	}

	void Start(uint64 Size, uint64 SeekDistance)
	{
		FScopeLock Lock(&Crit);
		double Now = FPlatformTime::Seconds();
		if (NumRequests++ == 0)
		{
			// was idle, no longer is
			if (LastTime != -1.0)
			{
				IdleTime += (Now - LastTime);
			}
			LastTime = Now;
			MaybePrint();
		}
		AmountRead += Size;
		NumReads++;
		if (SeekDistance > 0)
		{
			NumSeeks++;
			TotalSeekDistance += SeekDistance;
		}
	}
	void Stop()
	{
		FScopeLock Lock(&Crit);
		double Now = FPlatformTime::Seconds();
		if (--NumRequests == 0)
		{
			// was working, no longer is
			check(LastTime > 0);
			WorkTime += (Now - LastTime);
			LastTime = Now;
			MaybePrint();
		}
	}
	void MaybePrint()
	{
		if (LastPrint < 0.0)
		{
			LastPrint = LastTime;
			return;
		}
		float TimeInterval = float(LastTime - LastPrint);
		if (TimeInterval > DiskUtilizationTrackerPrintFrequency && IdleTime + WorkTime > 0.0)
		{
			LastPrint = LastTime;
			float Result = float(100.0 * WorkTime / (IdleTime + WorkTime));
			double MBS = ((double)AmountRead / TimeInterval) / (1024 * 1024);
			double KBPerSeek = 0.0;
			if (NumSeeks)
			{
				KBPerSeek = ((double)AmountRead) / (1024 * NumSeeks);
			}
			double ActualMBS = MBS * Result / 100.0;
			double AvgSeek = NumSeeks ? ((double)TotalSeekDistance / (double)NumSeeks) : 0;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Disk: %5.2f%% utilization over %6.2fs\t%.2f MB/s\t%.2f Actual MB/s\t(%d Reads, %d Seeks, %.2f kbytes / seek, %.2f ave seek)\r\n"), Result, TimeInterval, MBS, ActualMBS, NumReads, NumSeeks, KBPerSeek, AvgSeek);
			WorkTime = 0.0;
			IdleTime = 0.0;
			AmountRead = 0;
			NumSeeks = 0;
			TotalSeekDistance = 0;
			NumReads = 0;
		}
	}
} GDiskUtilizationTracker;


struct FScopedDiskUtilizationTracker
{
	FScopedDiskUtilizationTracker(uint64 Size, uint64 SeekDistance)
	{
		GDiskUtilizationTracker.Start(Size, SeekDistance);
	}
	~FScopedDiskUtilizationTracker()
	{
		GDiskUtilizationTracker.Stop();
	}

};



#else
struct FScopedDiskUtilizationTracker
{
	FScopedDiskUtilizationTracker(uint64 Size, uint64 SeekDistance)
	{
	}
};
#endif


class FRegisteredFileHandle : public IFileHandle
{
public:
	FRegisteredFileHandle()
		: NextLink(nullptr)
		, PreviousLink(nullptr)
		, bIsOpenAndAvailableForClosing(false)
	{
	}

private:
	friend class FFileHandleRegistry;

	FRegisteredFileHandle* NextLink;
	FRegisteredFileHandle* PreviousLink;
	bool bIsOpenAndAvailableForClosing;
};

PRAGMA_DISABLE_OPTIMIZATION

class FFileHandleRegistry
{
public:
	FFileHandleRegistry(int32 InMaxOpenHandles)
		: MaxOpenHandles(InMaxOpenHandles)
		, OpenAndAvailableForClosingHead(nullptr)
		, OpenAndAvailableForClosingTail(nullptr)
	{
	}

	FRegisteredFileHandle* InitialOpenFile(const TCHAR* Filename)
	{
		if (HandlesCurrentlyInUse.Increment() > MaxOpenHandles)
		{
			FreeHandles();
		}

		FRegisteredFileHandle* Handle = PlatformInitialOpenFile(Filename);
		if (Handle != nullptr)
		{
			FScopeLock Lock(&LockSection);
			LinkToTail(Handle);
		}
		else
		{
			HandlesCurrentlyInUse.Decrement();
		}

		return Handle;
	}

	void UnTrackAndCloseFile(FRegisteredFileHandle* Handle)
	{
		bool bWasOpen = false;
		{
			FScopeLock Lock(&LockSection);
			if (Handle->bIsOpenAndAvailableForClosing)
			{
				UnLink(Handle);
				bWasOpen = true;
			}
		}
		if (bWasOpen)
		{
			PlatformCloseFile(Handle);
			HandlesCurrentlyInUse.Decrement();
		}
	}

	void TrackStartRead(FRegisteredFileHandle* Handle)
	{
		{
			FScopeLock Lock(&LockSection);

			if (Handle->bIsOpenAndAvailableForClosing)
			{
				UnLink(Handle);
				return;
			}
		}

		if (HandlesCurrentlyInUse.Increment() > MaxOpenHandles)
		{
			FreeHandles();
		}
		// can do this out of the lock, in case it's slow
		PlatformReopenFile(Handle);
	}

	void TrackEndRead(FRegisteredFileHandle* Handle)
	{
		{
			FScopeLock Lock(&LockSection);
			LinkToTail(Handle);
		}
	}

protected:
	virtual FRegisteredFileHandle* PlatformInitialOpenFile(const TCHAR* Filename) = 0;
	virtual bool PlatformReopenFile(FRegisteredFileHandle*) = 0;
	virtual void PlatformCloseFile(FRegisteredFileHandle*) = 0;

private:

	void FreeHandles()
	{
		// do we need to make room for a file handle? this assumes that the critical section is already locked
		while (HandlesCurrentlyInUse.GetValue() > MaxOpenHandles)
		{
			FRegisteredFileHandle* ToBeClosed = nullptr;
			{
				FScopeLock Lock(&LockSection);
				ToBeClosed = PopFromHead();
			}
			if (ToBeClosed)
			{
				// close it, freeing up space for new file to open
				PlatformCloseFile(ToBeClosed);
				HandlesCurrentlyInUse.Decrement();
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("Spinning because we are actively reading from more file handles than we have possible handles.\r\n"));
				FPlatformProcess::SleepNoStats(.1f);
			}
		}
	}


	void LinkToTail(FRegisteredFileHandle* Handle)
	{
		check(!Handle->PreviousLink && !Handle->NextLink && !Handle->bIsOpenAndAvailableForClosing);
		Handle->bIsOpenAndAvailableForClosing = true;
		if (OpenAndAvailableForClosingTail)
		{
			Handle->PreviousLink = OpenAndAvailableForClosingTail;
			check(!OpenAndAvailableForClosingTail->NextLink);
			OpenAndAvailableForClosingTail->NextLink = Handle;
		}
		else
		{
			check(!OpenAndAvailableForClosingHead);
			OpenAndAvailableForClosingHead = Handle;
		}
		OpenAndAvailableForClosingTail = Handle;
	}

	void UnLink(FRegisteredFileHandle* Handle)
	{
		if (OpenAndAvailableForClosingHead == Handle)
		{
			verify(PopFromHead() == Handle);
			return;
		}
		check(Handle->bIsOpenAndAvailableForClosing);
		Handle->bIsOpenAndAvailableForClosing = false;
		if (OpenAndAvailableForClosingTail == Handle)
		{
			check(OpenAndAvailableForClosingHead && OpenAndAvailableForClosingHead != Handle && Handle->PreviousLink);
			OpenAndAvailableForClosingTail = Handle->PreviousLink;
			OpenAndAvailableForClosingTail->NextLink = nullptr;
			Handle->NextLink = nullptr;
			Handle->PreviousLink = nullptr;
			return;
		}
		check(Handle->NextLink && Handle->PreviousLink);
		Handle->NextLink->PreviousLink = Handle->PreviousLink;
		Handle->PreviousLink->NextLink = Handle->NextLink;
		Handle->NextLink = nullptr;
		Handle->PreviousLink = nullptr;

	}

	FRegisteredFileHandle* PopFromHead()
	{
		FRegisteredFileHandle* Result = OpenAndAvailableForClosingHead;
		if (Result)
		{
			check(!Result->PreviousLink);
			check(Result->bIsOpenAndAvailableForClosing);
			Result->bIsOpenAndAvailableForClosing = false;
			OpenAndAvailableForClosingHead = Result->NextLink;
			if (!OpenAndAvailableForClosingHead)
			{
				check(OpenAndAvailableForClosingTail == Result);
				OpenAndAvailableForClosingTail = nullptr;
			}
			else
			{
				check(OpenAndAvailableForClosingHead->PreviousLink == Result);
				OpenAndAvailableForClosingHead->PreviousLink = nullptr;
			}
			Result->NextLink = nullptr;
			Result->PreviousLink = nullptr;
		}
		return Result;
	}

	// critical section to protect the below arrays
	FCriticalSection LockSection;

	int32 MaxOpenHandles;

	FRegisteredFileHandle* OpenAndAvailableForClosingHead;
	FRegisteredFileHandle* OpenAndAvailableForClosingTail;

	FThreadSafeCounter HandlesCurrentlyInUse;
};

PRAGMA_ENABLE_OPTIMIZATION

