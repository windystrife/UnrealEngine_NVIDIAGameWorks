// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/DirectoryWatchRequestLinux.h"
#include "HAL/FileManager.h"
#include "DirectoryWatcherPrivate.h"

FDirectoryWatchRequestLinux::FDirectoryWatchRequestLinux()
:	bRunning(false)
,	bEndWatchRequestInvoked(false)
,	bIncludeDirectoryChanges(false)
,	bWatchSubtree(false)
{
	NotifyFilter = IN_CREATE | IN_MOVE | IN_MODIFY | IN_DELETE;
}

FDirectoryWatchRequestLinux::~FDirectoryWatchRequestLinux()
{
	Shutdown();
}

void FDirectoryWatchRequestLinux::Shutdown()
{
	close(FileDescriptor);

	WatchDescriptorsToPaths.Empty();
	PathsToWatchDescriptors.Empty();
}

bool FDirectoryWatchRequestLinux::Init(const FString& InDirectory, uint32 Flags)
{
	if (InDirectory.Len() == 0)
	{
		// Verify input
		return false;
	}

	Directory = InDirectory;
	bIncludeDirectoryChanges = (Flags & IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges) != 0;
	bWatchSubtree = (Flags & IDirectoryWatcher::WatchOptions::IgnoreChangesInSubtree) == 0;

	if (bRunning)
	{
		Shutdown();
	}

	bEndWatchRequestInvoked = false;

	// Make sure the path is absolute
	const FString FullPath = FPaths::ConvertRelativePathToFull(InDirectory);
	UE_LOG(LogDirectoryWatcher, Verbose, TEXT("Adding watch for directory tree '%s'"), *FullPath);

	FileDescriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (FileDescriptor == -1)
	{
		// Failed to init inotify
		UE_LOG(LogDirectoryWatcher, Error, TEXT("Failed to init inotify"));
		return false;
	}

	// find all subdirs
	WatchDirectoryTree(FullPath);

	bRunning = true;

	return true;
}

FDelegateHandle FDirectoryWatchRequestLinux::AddDelegate(const IDirectoryWatcher::FDirectoryChanged& InDelegate)
{
	Delegates.Add(InDelegate);
	return Delegates.Last().GetHandle();
}

bool FDirectoryWatchRequestLinux::RemoveDelegate(FDelegateHandle InHandle)
{
	return Delegates.RemoveAll([=](const IDirectoryWatcher::FDirectoryChanged& Delegate) {
		return Delegate.GetHandle() == InHandle;
	}) != 0;
}

bool FDirectoryWatchRequestLinux::HasDelegates() const
{
	return Delegates.Num() > 0;
}

void FDirectoryWatchRequestLinux::EndWatchRequest()
{
	bEndWatchRequestInvoked = true;
}


void FDirectoryWatchRequestLinux::ProcessPendingNotifications()
{
	ProcessChanges();

	// Trigger all listening delegates with the files that have changed
	if (FileChanges.Num() > 0)
	{
		for (int32 DelegateIdx = 0; DelegateIdx < Delegates.Num(); ++DelegateIdx)
		{
			Delegates[DelegateIdx].Execute(FileChanges);
		}

		FileChanges.Empty();
	}
}

void FDirectoryWatchRequestLinux::WatchDirectoryTree(const FString & RootAbsolutePath)
{
	UE_LOG(LogDirectoryWatcher, VeryVerbose, TEXT("Watching tree '%s'"), *RootAbsolutePath);
	TArray<FString> AllFiles;
	if (bWatchSubtree)
	{
		IFileManager::Get().FindFilesRecursive(AllFiles, *RootAbsolutePath, TEXT("*"), false, true);
	}
	// add the path as well
	AllFiles.Add(RootAbsolutePath);

	for (int32 FileIdx = 0, NumTotal = AllFiles.Num(); FileIdx < NumTotal; ++FileIdx)
	{
		const FString& FolderName = AllFiles[FileIdx];
		checkf(PathsToWatchDescriptors.Find(FolderName) == nullptr, TEXT("Adding a duplicate watch for directory '%s'"), *FolderName);

		int32 WatchDescriptor = inotify_add_watch(FileDescriptor, TCHAR_TO_UTF8(*FolderName), NotifyFilter);
		if (WatchDescriptor == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogDirectoryWatcher, Error, TEXT("inotify_add_watch cannot watch folder %s (errno = %d, %s)"), *FolderName,
				ErrNo,
				ANSI_TO_TCHAR(strerror(ErrNo))
				);
			// proceed further
		}

		UE_LOG(LogDirectoryWatcher, VeryVerbose, TEXT("+ Added a watch %d for '%s'"), WatchDescriptor, *FolderName);
		// update the mapping
		WatchDescriptorsToPaths.Add(WatchDescriptor, FolderName);
		PathsToWatchDescriptors.Add(FolderName, WatchDescriptor);
	}
}

void FDirectoryWatchRequestLinux::UnwatchDirectoryTree(const FString & RootAbsolutePath)
{
	UE_LOG(LogDirectoryWatcher, VeryVerbose, TEXT("Unwatching tree '%s'"), *RootAbsolutePath);
	// remove the watch for the folder and all subfolders
	// since it is expected that there will be a lot of them, just create a new TMap
	TMap<FString, int32, FDefaultSetAllocator, FCaseSensitiveLookupKeyFuncs<int32>> NewPathsToWatchDescriptors;
	for (auto MapIt = PathsToWatchDescriptors.CreateIterator(); MapIt; ++MapIt)
	{
		if (!MapIt->Key.StartsWith(RootAbsolutePath, ESearchCase::CaseSensitive))
		{
			NewPathsToWatchDescriptors.Add(MapIt->Key, MapIt->Value);
		}
		else
		{
			UE_LOG(LogDirectoryWatcher, VeryVerbose, TEXT("- Removing a watch %d for '%s'"), MapIt->Value, *MapIt->Key);

			// delete the descriptor
			int RetVal = inotify_rm_watch(FileDescriptor, MapIt->Value);

			// why check for RootAbsolutePath? Because this function may be called when root path has been deleted, and inotify_rm_watch() will fail
			// removing a watch on a deleted file... yay for API symmetry. Just "leak" the watch descriptor without the warning
			if (RetVal == -1 || MapIt->Key != RootAbsolutePath)
			{
				int ErrNo = errno;
				UE_LOG(LogDirectoryWatcher, Error, TEXT("inotify_rm_watch cannot remove descriptor %d for folder '%s' (errno = %d, %s)"),
					MapIt->Value,
					*MapIt->Key,
					ErrNo,
					ANSI_TO_TCHAR(strerror(ErrNo))
				);
			}
			WatchDescriptorsToPaths.Remove(MapIt->Value);
		}
	}

	PathsToWatchDescriptors = NewPathsToWatchDescriptors;
}

void FDirectoryWatchRequestLinux::ProcessChanges()
{
	uint8_t Buffer[EVENT_BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event* Event;
	ssize_t Len = 0;
	uint8_t* Ptr = nullptr;

	// Loop while events can be read from inotify file descriptor
	for (;;)
	{
		// default action
		FFileChangeData::EFileChangeAction Action = FFileChangeData::FCA_Unknown;

		// Read event stream
		Len = read(FileDescriptor, Buffer, EVENT_BUF_LEN);

		// If the non-blocking read() found no events to read, then it returns -1 with errno set to EAGAIN.
		if (Len == -1 && errno != EAGAIN)
		{
			int ErrNo = errno;
			UE_LOG(LogDirectoryWatcher, Error, TEXT("FDirectoryWatchRequestLinux::ProcessChanges() read() error getting events for path '%s' (errno = %d, %s)"),
				*Directory,
				ErrNo,
				ANSI_TO_TCHAR(strerror(ErrNo))
				);
			break;
		}

		if (Len <= 0)
		{
			break;
		}

		// Loop over all events in the buffer
		for (Ptr = Buffer; Ptr < Buffer + Len;)
		{
			Event = reinterpret_cast<const struct inotify_event *>(Ptr);

			// some events can be ignored to match other implementations and tests
			bool bIgnoreEvent = false;

			// skip if overflowed
			if (Event->wd != -1 && (Event->mask & IN_Q_OVERFLOW) == 0)
			{
				const FString *EventPathPtr = WatchDescriptorsToPaths.Find(Event->wd);

				UE_LOG(LogDirectoryWatcher, VeryVerbose, TEXT("Event: watch descriptor %d, mask 0x%08x, EventPath: '%s'"),
					Event->wd, Event->mask, EventPathPtr ? *(*EventPathPtr) : TEXT("nullptr"), ANSI_TO_TCHAR(Event->name));

				// if we're geting multiple events (e.g. DELETE, IGNORED) we could ahve removed descriptor on previous iteration,
				// so we need to handle inability to find it in the map
				if (EventPathPtr)
				{
					FString EventPath = *EventPathPtr;	// get a copy since we can mutate WatchDescriptorToPaths
					FString AffectedFile = EventPath / ANSI_TO_TCHAR(Event->name);	// by default, some events report about the file itself

					if ((Event->mask & IN_CREATE) || (Event->mask & IN_MOVED_TO))
					{
						// if a directory was created/moved, watch it
						if (Event->mask & IN_ISDIR)
						{
							WatchDirectoryTree(AffectedFile);
							// to be in sync with Windows implementation, ignore events about creating directories unless told so
							bIgnoreEvent = !bIncludeDirectoryChanges;
						}

						Action = FFileChangeData::FCA_Added;
					}
					else if (Event->mask & IN_MODIFY)
					{
						// if a directory was modified, we expect to get events from already watched files in it
						Action = FFileChangeData::FCA_Modified;
					}
					// check if the file/directory itself has been deleted (IGNORED can also be sent on delete)
					else if ((Event->mask & IN_DELETE_SELF) || (Event->mask & IN_IGNORED) || (Event->mask & IN_UNMOUNT))
					{
						// if a directory was deleted, we expect to get events from already watched files in it
						AffectedFile = EventPath;
						if (Event->mask & IN_ISDIR)
						{
							UnwatchDirectoryTree(EventPath);
							// to be in sync with Windows implementation, ignore events about deleting directories unless told so
							bIgnoreEvent = !bIncludeDirectoryChanges;
						}
						else
						{
							// remove itself from both mappings
							// NOTE: inotify_rm_watch() will fail here as watch descriptor is no longer valid (deficiency of inotify?).
							// Just avoid calling inotify_rm_watch() even if it feels like a leak
							WatchDescriptorsToPaths.Remove(Event->wd);
							PathsToWatchDescriptors.Remove(EventPath);
						}

						Action = FFileChangeData::FCA_Removed;
					}
					else if ((Event->mask & IN_DELETE) || (Event->mask & IN_MOVED_FROM))
					{
						// if a directory was deleted/moved, invalidate watch descriptors associated with it (unwatch it)
						if (Event->mask & IN_ISDIR)
						{
							UnwatchDirectoryTree(AffectedFile);
							// to be in sync with Windows implementation, ignore events about deleting directories unless told so
							bIgnoreEvent = !bIncludeDirectoryChanges;
						}

						Action = FFileChangeData::FCA_Removed;
					}

					if (!bIgnoreEvent && Event->len)
					{
						new (FileChanges) FFileChangeData(AffectedFile, Action);
					}
				}
			}

			Ptr += EVENT_SIZE + Event->len;
		}
	}
}
