// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformProcess.mm: Mac implementations of Process functions
=============================================================================*/

#include "Mac/MacCriticalSection.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include <mach-o/dyld.h>
#include <libproc.h>

FMacSystemWideCriticalSection::FMacSystemWideCriticalSection(const FString& InName, FTimespan InTimeout)
{
	check(InName.Len() > 0)
	check(InTimeout >= FTimespan::Zero())
	check(InTimeout.GetTotalSeconds() < (double)FLT_MAX)	// we'll need this to fit in a single-precision float later so check it here

	FDateTime ExpireTime = FDateTime::UtcNow() + InTimeout;

	// This lock implementation is using files so correct any backslashes in the name
	const FString LockPath = FString(FMacPlatformProcess::ApplicationSettingsDir()) / InName;
	FString NormalizedFilepath(LockPath);
	NormalizedFilepath.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Attempt to open a file with O_EXLOCK (equivalent of atomic open() + flock())
	FileHandle = open(TCHAR_TO_UTF8(*NormalizedFilepath), O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	if (FileHandle == -1 && InTimeout != FTimespan::Zero())
	{
		// Failed to get a valid file handle so the file is probably open and locked by another owner - retry until we timeout

		for (FTimespan TimeoutRemaining = ExpireTime - FDateTime::UtcNow();
			 FileHandle == -1 && TimeoutRemaining > FTimespan::Zero();
			 TimeoutRemaining = ExpireTime - FDateTime::UtcNow())
		{
			// retry until timeout
			float RetrySeconds = FMath::Min((float)TimeoutRemaining.GetTotalSeconds(), 0.25f);
			FMacPlatformProcess::Sleep(RetrySeconds);
			FileHandle = open(TCHAR_TO_UTF8(*NormalizedFilepath), O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		}
	}
}

FMacSystemWideCriticalSection::~FMacSystemWideCriticalSection()
{
	Release();
}

bool FMacSystemWideCriticalSection::IsValid() const
{
	return FileHandle != -1;
}

void FMacSystemWideCriticalSection::Release()
{
	if (IsValid())
	{
		close(FileHandle);
		FileHandle = -1;
	}
}
