// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/DirectoryWatcherLinux.h"
#include "DirectoryWatcherPrivate.h"

FDirectoryWatcherLinux::FDirectoryWatcherLinux()
{
	NumRequests = 0;
}

FDirectoryWatcherLinux::~FDirectoryWatcherLinux()
{
	if (RequestMap.Num() != 0)
	{
		// Delete any remaining requests here. These requests are likely from modules which are still loaded at the time that this module unloads.
		for (TMap<FString, FDirectoryWatchRequestLinux*>::TConstIterator RequestIt(RequestMap); RequestIt; ++RequestIt)
		{
			if (ensure(RequestIt.Value()))
			{
				delete RequestIt.Value();
				NumRequests--;
			}
		}

		RequestMap.Empty();
	}

	if (RequestsPendingDelete.Num() != 0)
	{
		for (int32 RequestIdx = 0; RequestIdx < RequestsPendingDelete.Num(); ++RequestIdx)
		{
			delete RequestsPendingDelete[RequestIdx];
			NumRequests--;
		}
	}

	// Make sure every request that was created is destroyed
	ensure(NumRequests == 0);
}

bool FDirectoryWatcherLinux::RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, uint32 Flags)
{
	FDirectoryWatchRequestLinux** RequestPtr = RequestMap.Find(Directory);
	FDirectoryWatchRequestLinux* Request = NULL;

	if (RequestPtr)
	{
		// There should be no NULL entries in the map
		check (*RequestPtr);

		Request = *RequestPtr;
	}
	else
	{
		Request = new FDirectoryWatchRequestLinux();
		NumRequests++;

		// Begin reading directory changes
		if (!Request->Init(Directory, Flags))
		{
			UE_LOG(LogDirectoryWatcher, Warning, TEXT("Failed to begin reading directory changes for %s."), *Directory);
			delete Request;
			NumRequests--;
			return false;
		}

		RequestMap.Add(Directory, Request);
	}

	OutHandle = Request->AddDelegate(InDelegate);

	return true;
}


bool FDirectoryWatcherLinux::UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle)
{
	FDirectoryWatchRequestLinux** RequestPtr = RequestMap.Find(Directory);

	if (RequestPtr)
	{
		// There should be no NULL entries in the map
		check (*RequestPtr);

		FDirectoryWatchRequestLinux* Request = *RequestPtr;

		if (Request->RemoveDelegate(InHandle))
		{
			if (!Request->HasDelegates())
			{
				// Remove from the active map and add to the pending delete list
				RequestMap.Remove(Directory);
				RequestsPendingDelete.AddUnique(Request);

				// Signal to end the watch which will mark this request for deletion
				Request->EndWatchRequest();
			}

			return true;
		}

	}

	return false;
}

void FDirectoryWatcherLinux::Tick(float DeltaSeconds)
{
	// Delete unregistered requests
	for (int32 RequestIdx = RequestsPendingDelete.Num() - 1; RequestIdx >= 0; --RequestIdx)
	{
		FDirectoryWatchRequestLinux* Request = RequestsPendingDelete[RequestIdx];
		delete Request;
		NumRequests--;
		RequestsPendingDelete.RemoveAt(RequestIdx);
	}

	// Trigger any file change notification delegates
	for (TMap<FString, FDirectoryWatchRequestLinux*>::TConstIterator RequestIt(RequestMap); RequestIt; ++RequestIt)
	{
		RequestIt.Value()->ProcessPendingNotifications();
	}
}
