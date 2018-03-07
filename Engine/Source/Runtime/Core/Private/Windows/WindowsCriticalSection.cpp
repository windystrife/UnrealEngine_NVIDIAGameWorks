// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsCriticalSection.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"

FWindowsSystemWideCriticalSection::FWindowsSystemWideCriticalSection(const FString& InName, FTimespan InTimeout)
{
	// Check for valid name length not exceeding the Window mutex name length limit
	check(InName.Len() > 0)
	check(InName.Len() < MAX_PATH)
	// Check for valid positive timeouts
	check(InTimeout >= FTimespan::Zero())
	check(InTimeout.GetTotalMilliseconds() < (double)0x7FFFFFFE)	// limit timespan to a number of millisec that will fit in a signed int32

	// Disallow backslashes as they aren't allowed in Windows mutex names
	FString NormalizedMutexName(InName);
	NormalizedMutexName.ReplaceInline(TEXT("\\"), TEXT("/"));

	TCHAR MutexName[MAX_PATH] = TEXT("");
	FCString::Strcpy(MutexName, *NormalizedMutexName);

	// Attempt to create and take ownership of a named mutex
	Mutex = CreateMutex(NULL, true, MutexName);

	if (Mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// CreateMutex returned a valid handle but we didn't get ownership because another process/thread has already created it
		bool bMutexOwned = false;

		if (InTimeout != FTimespan::Zero())
		{
			// We have a handle already so try waiting for it to be released by the current owner
			DWORD WaitResult = WaitForSingleObject(Mutex, FMath::TruncToInt((float)InTimeout.GetTotalMilliseconds()));

			// WAIT_OBJECT_0 = we got ownership when the previous owner released it
			// WAIT_ABANDONED = we got ownership when the previous owner exited WITHOUT releasing the mutex gracefully (we own it now but the state of any shared resource could be corrupted!)
			if (WaitResult == WAIT_ABANDONED || WaitResult == WAIT_OBJECT_0)
			{
				bMutexOwned = true;
			}
		}

		if (!bMutexOwned)
		{
			// We failed to gain ownership by waiting so close the handle to avoid leaking it.
			CloseHandle(Mutex);
			Mutex = NULL;
		}
	}
}

FWindowsSystemWideCriticalSection::~FWindowsSystemWideCriticalSection()
{
	Release();
}

bool FWindowsSystemWideCriticalSection::IsValid() const
{
	return Mutex != NULL;
}

void FWindowsSystemWideCriticalSection::Release()
{
	if (IsValid())
	{
		// Release ownership
		ReleaseMutex(Mutex);
		// Also release the handle so it isn't leaked
		CloseHandle(Mutex);
		Mutex = NULL;
	}
}

#include "Windows/HideWindowsPlatformTypes.h"
