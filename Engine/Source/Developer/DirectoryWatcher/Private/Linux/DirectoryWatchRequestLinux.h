// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDirectoryWatcher.h"
#include <sys/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

class FDirectoryWatchRequestLinux
{
public:

	/**
	 * Template class to support FString to TValueType maps with a case sensitive key
	 */
	template<typename TValueType>
	struct FCaseSensitiveLookupKeyFuncs : BaseKeyFuncs<TValueType, FString>
	{
		static FORCEINLINE const FString& GetSetKey(const TPair<FString, TValueType>& Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
		static FORCEINLINE uint32 GetKeyHash(const FString& Key)
		{
			return FCrc::StrCrc32<TCHAR>(*Key);
		}
	};


	FDirectoryWatchRequestLinux();
	virtual ~FDirectoryWatchRequestLinux();

	/** Sets up the directory handle and request information */
	bool Init(const FString& InDirectory, uint32 Flags);

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

	/** Adds watches for all files (and subdirectories) in a directory. */
	void WatchDirectoryTree(const FString & RootAbsolutePath);

	/** Removes all watches from a */
	void UnwatchDirectoryTree(const FString & RootAbsolutePath);

	FString Directory;

	bool bRunning;
	bool bEndWatchRequestInvoked;

	/** Whether to report directory creation/deletion changes. */
	bool bIncludeDirectoryChanges;

	/** Whether or not watch subtree. */
	bool bWatchSubtree;

	int FileDescriptor;

	/** Mapping from watch descriptors to their path names */
	TMap<int32, FString> WatchDescriptorsToPaths;

	/** Mapping from paths to watch descriptors */
	TMap<FString, int32, FDefaultSetAllocator, FCaseSensitiveLookupKeyFuncs<int32>> PathsToWatchDescriptors;

	int NotifyFilter;

	TArray<IDirectoryWatcher::FDirectoryChanged> Delegates;
	TArray<FFileChangeData> FileChanges;

	void Shutdown();
	void ProcessChanges();
};
