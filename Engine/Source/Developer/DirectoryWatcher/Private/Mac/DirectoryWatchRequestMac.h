// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "IDirectoryWatcher.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include <CoreServices/CoreServices.h>

class FDirectoryWatchRequestMac
{
public:
	FDirectoryWatchRequestMac(uint32 Flags);
	virtual ~FDirectoryWatchRequestMac();

	/** Sets up the directory handle and request information */
	bool Init(const FString& InDirectory);

	/** Adds a delegate to get fired when the directory changes */
	FDelegateHandle AddDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	bool RemoveDelegate( FDelegateHandle InHandle );
	/** Returns true if this request has any delegates listening to directory changes */
	bool HasDelegates() const;
	/** Prepares the request for deletion */
	void EndWatchRequest();
	/** Triggers all pending file change notifications */
	void ProcessPendingNotifications();

private:

	FSEventStreamRef	EventStream;
	bool				bRunning;
	bool				bEndWatchRequestInvoked;
	bool				bIncludeDirectoryEvents;
	bool				bIgnoreChangesInSubtree;

	TArray<IDirectoryWatcher::FDirectoryChanged> Delegates;
	TArray<FFileChangeData> FileChanges;

	friend void DirectoryWatchMacCallback( ConstFSEventStreamRef StreamRef, void* WatchRequestPtr, size_t EventCount, void* EventPaths, const FSEventStreamEventFlags EventFlags[], const FSEventStreamEventId EventIDs[] );

	void ProcessChanges( size_t EventCount, void* EventPaths, const FSEventStreamEventFlags EventFlags[] );
	void Shutdown( void );
};
