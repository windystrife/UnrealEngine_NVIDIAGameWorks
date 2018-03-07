// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioSoundFileManager.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{

	FSoundFileDataEntry::FSoundFileDataEntry(class FSoundFileManager* InSoundFileManager, const FSoundFileHandle& InSoundFileHandle, bool bInIsStreamed)
		: SoundFileManager(InSoundFileManager)
		, SoundFileHandle(InSoundFileHandle)
		, SoundFileState((int32)(ESoundFileState::UNINITIALIZED))
		, Error(ESoundFileError::NONE)
		, TimeSinceUsed(0.0f)
		, NumReferences(1)
		, bIsStreamed(bInIsStreamed)
	{}

	FSoundFileDataEntry::~FSoundFileDataEntry()
	{
		// Make sure nobody is referencing this entry
		check(NumReferences == 0);

		// Release this sound entry's file handle when the entry is deleted.
		SoundFileManager->EntityManager.ReleaseEntity(SoundFileHandle);
	}

	FAsyncSoundFileLoadTask::FAsyncSoundFileLoadTask(FUnrealAudioModule* InAudioModule, FSoundFileHandle InSoundFileHandle, const FString& InPath)
		: AudioModule(InAudioModule)
		, SoundFileHandle(InSoundFileHandle)
		, Path(InPath)
	{
		// make sure the audio module knows we're doing background task work so it doesn't shutdown while we're working here
		AudioModule->IncrementBackgroundTaskCount();
	}

	FAsyncSoundFileLoadTask::~FAsyncSoundFileLoadTask()
	{
		// Let the audio module know we're done doing this background task
		AudioModule->DecrementBackgroundTaskCount();
	}

	void FAsyncSoundFileLoadTask::DoWork()
	{
		FSoundFileManager& SoundFileManager = AudioModule->GetSoundFileManager();
		TSharedPtr<FSoundFileDataEntry> DataEntry = SoundFileManager.SoundFileData[SoundFileHandle.GetIndex()];
		check(DataEntry->SoundFileHandle.Id == SoundFileHandle.Id);

		SoundFileManager.LoadSoundFileDataEntry(DataEntry);
	}

	/************************************************************************/
	/* FSoundFileManager Implementation										*/
	/************************************************************************/

	FSoundFileManager::FSoundFileManager(FUnrealAudioModule* InAudioModule)
		: EntityManager(500)
		, AudioModule(InAudioModule)
		, FileLoadingThreadPool(nullptr)
		, NumSoundFilesLoaded(0)
		, NumSoundFilesStreamed(0)
		, NumBytesLoaded(0)
		, NumActiveSounds(0)
		, NumInactiveSounds(0)
		, bLogOverMemoryTarget(true)
	{
		Settings = { 0 };
	}

	FSoundFileManager::~FSoundFileManager()
	{
		check(FileLoadingThreadPool == nullptr);
	}

	void FSoundFileManager::Init(FSoundFileManagerSettings& InSettings)
	{
		check(AudioModule != nullptr);

		Settings = InSettings;

		// Initialize sound file manager data
		SoundFileHandles.Init(FSoundFileHandle(), InSettings.MaxNumberOfLoadedSounds);
		SoundFileData.Init(nullptr, InSettings.MaxNumberOfLoadedSounds);

		// Initialize the sound file loading pool
		FileLoadingThreadPool = FQueuedThreadPool::Allocate();
		FileLoadingThreadPool->Create(Settings.NumLoadingThreads, (32 * 1024), Settings.LoadingThreadPriority);
	}

	void FSoundFileManager::Shutdown()
	{
		// Clean up the thread pool
		check(FileLoadingThreadPool);
		FileLoadingThreadPool->Destroy();
		FileLoadingThreadPool = nullptr;
	}

	TSharedPtr<ISoundFile> FSoundFileManager::CreateSoundFile()
	{
		return TSharedPtr<ISoundFile>(new FSoundFile(this));
	}

	TSharedPtr<ISoundFile> FSoundFileManager::GetSoundFile(const struct FSoundFileHandle& SoundFileHandle)
	{
		uint32 DataIndex = SoundFileHandle.GetIndex();
		check(DataIndex < (uint32)SoundFileData.Num());
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[DataIndex];

		// Make sure the data at this index is valid
		check(Entry.IsValid());
		check(Entry->SoundFileHandle.Id == SoundFileHandle.Id);

		// Bump up the ref-count of this data (so we know how many ISoundFile's are currently using this data)
		Entry->NumReferences++;

		// Make sure we reset the time-since-used parameter since we're now referencing this asset
		Entry->TimeSinceUsed = 0.0;

		TSharedPtr<ISoundFile> NewSoundFile = TSharedPtr<ISoundFile>(new FSoundFile(this));
		(static_cast<FSoundFile*>(NewSoundFile.Get()))->Init(SoundFileHandle);

		return NewSoundFile;
	}

	TSharedPtr<ISoundFile> FSoundFileManager::LoadSoundFile(const FName& Name, TArray<uint8>& InBulkData)
	{
		// TODO
		return nullptr;
	}

	TSharedPtr<ISoundFile> FSoundFileManager::LoadSoundFile(const FName& Path, bool bLoadAsync)
	{
		// First see if we've already loaded this sound by checking the sound file name
		FSoundFileHandle* ExistingHandle = NameToLoadedSoundMap.Find(Path);
		if (ExistingHandle)
		{
			return GetSoundFile(*ExistingHandle);
		}

		return CreateNewSoundFile(Path, false, bLoadAsync);
	}

	TSharedPtr<ISoundFile> FSoundFileManager::StreamSoundFile(const FName& Path, bool bLoadAsync)
	{
		FSoundFileHandle* ExistingHandle = NameToStreamedSoundMap.Find(Path);
		if (ExistingHandle)
		{
			return GetSoundFile(*ExistingHandle);
		}

		return CreateNewSoundFile(Path, true, bLoadAsync);
	}

	TSharedPtr<ISoundFile> FSoundFileManager::CreateNewSoundFile(const FName& Path, bool bIsStreamed, bool bLoadAsync)
	{
		// If we haven't already loaded this sound file (or if it's been unloaded)
		// we need to create a data entry and trigger a loading operation.

		// Create a new sound file handle
		FSoundFileHandle NewHandle(EntityManager.CreateEntity());

		// Confirm that the data at the index value in the new handle doesn't already have data
		uint32 NewDataIndex = NewHandle.GetIndex();
		check(!SoundFileData[NewDataIndex].IsValid());

		// Create a new entry
		TSharedPtr<FSoundFileDataEntry> NewEntry = TSharedPtr<FSoundFileDataEntry>(new FSoundFileDataEntry(this, NewHandle, bIsStreamed));

		NewEntry->SoundFilePath = Path;

		// Store it in the data array
		SoundFileData[NewDataIndex] = NewEntry;

		// Add the entry to the sound file name to handle map
		if (bIsStreamed)
		{
			++NumSoundFilesStreamed;
			NameToStreamedSoundMap.Add(Path, NewHandle);
		}
		else
		{
			++NumSoundFilesLoaded;
			NameToLoadedSoundMap.Add(Path, NewHandle);
		}

		// Set the sound file state to loading...
		NewEntry->SoundFileState.Set((int32)ESoundFileState::LOADING);

		// Reset the last used for this entry
		NewEntry->TimeSinceUsed = 0.0f;

		if (bLoadAsync)
		{
			// Create a task to load the sound file. Note this task will delete itself when it finishes loading the sound file
			FAsyncTask<FAsyncSoundFileLoadTask>* Task = new FAsyncTask<FAsyncSoundFileLoadTask>(AudioModule, NewHandle, Path.GetPlainNameString());
			Task->StartBackgroundTask(FileLoadingThreadPool);
		} //-V773
		else
		{
			// Immediately load the data entry if this is a synchronous load call
			LoadSoundFileDataEntry(NewEntry);
		}

		// Create a new sound file ptr and give it the handle to the internal data entry
		TSharedPtr<ISoundFile> NewSoundFile = TSharedPtr<ISoundFile>(new FSoundFile(this));
		(static_cast<FSoundFile*>(NewSoundFile.Get()))->Init(NewHandle);

		return NewSoundFile;
	}

	ESoundFileState::Type FSoundFileManager::GetState(const FSoundFileHandle& SoundFileHandle) const
	{
		if (EntityManager.IsValidEntity(SoundFileHandle))
		{
			uint32 DataIndex = SoundFileHandle.GetIndex();

			// If there is valid data at the data index
			if (SoundFileData[DataIndex].IsValid())
			{
				return (ESoundFileState::Type)(SoundFileData[DataIndex]->SoundFileState.GetValue());
			}
		}
		return ESoundFileState::UNINITIALIZED;
	}

	void FSoundFileManager::Update()
	{
		MainThreadChecker.CheckThread();

		// Loop through sound files and update their TimeSinceUsed variable 

		uint32 SoundFileEntryCount = 0;
		uint32 NumEntries = NumSoundFilesStreamed + NumSoundFilesLoaded;
		LeastRecentlyUsedSoundFiles.Reset();
		SoundFileEntryComparePredicate Predicate;
		bool bAddToLRUList = true;

		// Reset the number of bytes loaded
		NumBytesLoaded = 0;
		NumActiveSounds = 0;
		NumInactiveSounds = 0;

		for (int32 i = 0; i < SoundFileData.Num() && SoundFileEntryCount < NumEntries; ++i)
		{
			if (!SoundFileData[i].IsValid())
			{
				continue;
			}

			bAddToLRUList = true;
			SoundFileEntryCount++;
			TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[i];

			// If the entry is no longer referenced...
			if (Entry->NumReferences == 0 && (Entry->SoundFileState.GetValue() != ESoundFileState::LOADING))
			{
				// If this entry no longer has references and it was streamed (i.e. no bulk data loaded)
				// Then just remove this entry from our array altogether
				if (Entry->BulkData.Num() == 0)
				{
					bAddToLRUList = false;
					FlushSoundFileDataIndex(i);
				}
				else
				{
					// Increment the TimeSinceUsed threshold by our set amount
					Entry->TimeSinceUsed += Settings.TimeDeltaPerUpdate;

					// Check if this entry is now above our flush threshold. 
					// if it is, then we'll just remove this asset from our cache
					if (Entry->TimeSinceUsed > Settings.FlushTimeThreshold)
					{
						bAddToLRUList = false;
						FlushSoundFileDataIndex(i);
					}
					else
					{
						check(bAddToLRUList);
						// Count this entry as an inactive loaded sound
						NumInactiveSounds++;
					}
				}
			}
			else
			{
				// Make sure this is currently active
				check(Entry->TimeSinceUsed == 0.0f);

				// We count this entry then as an active loaded sound
				NumActiveSounds++;
				check(bAddToLRUList);
			}

			// If we didn't flush this entry, then add it to our time-sorted list
			if (bAddToLRUList)
			{
				NumBytesLoaded += Entry->BulkData.Num();

				// Now add this entry to our sorted active sounds queue
				LeastRecentlyUsedSoundFiles.HeapPush(FSortedSoundFileEntry(i, Entry->TimeSinceUsed), Predicate);
			}
		}

		// Make sure our counts of loaded files and our LRU match
		check((NumSoundFilesStreamed + NumSoundFilesLoaded) == LeastRecentlyUsedSoundFiles.Num());

		// If our total number of bytes loaded is greater than our cache limit, try to flush any unused sound files
		// Starting with the least recently used (at the top). If we fail to flush under our limit, that means we 
		// have so many actively playing/referenced sounds. In that case, log a warning.
		if (NumBytesLoaded > Settings.TargetMemoryLimit)
		{
			bool bFlushSucceed = false;
			for (int32 i = 0; i < LeastRecentlyUsedSoundFiles.Num(); ++i)
			{
				FSortedSoundFileEntry& SortedEntry = LeastRecentlyUsedSoundFiles[i];
				TSharedPtr<FSoundFileDataEntry> DataEntry = SoundFileData[SortedEntry.Index];

				check(DataEntry.IsValid());

				// Flush the sound file, this will decrement the NumBytesLoaded value
				int32 MemoryFlushed = FlushSoundFileDataIndex(SortedEntry.Index);
				NumBytesLoaded -= MemoryFlushed;
				
				// Decrease our count of inactive sounds
				check(NumInactiveSounds > 0);
				--NumInactiveSounds;

				// if we're now lower than great!
				if (NumBytesLoaded < Settings.TargetMemoryLimit)
				{
					bFlushSucceed = true;
					break;
				}
			}

			// If we failed to flush to under our target sound file cache limit
			// then log it (but only if it was the first time since we failed since the last time we were properly under)
			if (!bFlushSucceed && bLogOverMemoryTarget)
			{
				bLogOverMemoryTarget = false;
				UE_LOG(LogUnrealAudio, Warning, TEXT("Audio sound file memory (%d bytes) is over target memory limit (%d)."), NumBytesLoaded, Settings.TargetMemoryLimit);
			}
		}
		else
		{
			// We have less bytes loaded than our cache size, so now we can log again if
			// fail at flushing below our cache size
			bLogOverMemoryTarget = true;
		}

	}

	void FSoundFileManager::ReleaseSoundFileHandle(const FSoundFileHandle& SoundFileHandle)
	{
		uint32 DataIndex = SoundFileHandle.GetIndex();
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[DataIndex];
		check(Entry.IsValid());
		check(Entry->NumReferences >= 1);
		Entry->NumReferences--;
	}

	int32 FSoundFileManager::FlushSoundFileDataIndex(int32 Index)
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[Index];
		check(Entry.IsValid());

		// Make sure nobody is referencing this sound file before we attempt to flush it
		check(Entry->NumReferences == 0);

		int32 MemoryFlushed = 0;

		// If we have bulk data than this was a loaded sound
		if (Entry->bIsStreamed)
		{
			check(NumSoundFilesStreamed > 0);
			--NumSoundFilesStreamed;

			check(NameToStreamedSoundMap.Contains(Entry->SoundFilePath));
			NameToStreamedSoundMap.Remove(Entry->SoundFilePath);

			// Flushing a streaming entry doesn't flush loaded sound file memory
		}
		// otherwise it was a streamed sound
		else
		{
			check(NumSoundFilesLoaded > 0);
			--NumSoundFilesLoaded;

			MemoryFlushed = Entry->BulkData.Num();

			check(NameToLoadedSoundMap.Contains(Entry->SoundFilePath));
			NameToLoadedSoundMap.Remove(Entry->SoundFilePath);
		}

		// Release the entry's sound file handle
		EntityManager.ReleaseEntity(Entry->SoundFileHandle);

		// Free this entry by setting it to a nullptr
		SoundFileData[Index] = nullptr;

		return MemoryFlushed;
	}

	void FSoundFileManager::LoadSoundFileDataEntry(TSharedPtr<FSoundFileDataEntry> DataEntry)
	{
		check(DataEntry.IsValid());

		ESoundFileError::Type Error;
		FString Path = DataEntry->SoundFilePath.GetPlainNameString();
		if (DataEntry->bIsStreamed)
		{
			Error = GetSoundFileInfoFromPath(Path, DataEntry->Description, DataEntry->ChannelMap);
			if (Error == ESoundFileError::NONE)
			{
				DataEntry->SoundFileState.Set((int32)ESoundFileState::STREAMING);
			}
		}
		else
		{
			Error = LoadSoundFileFromPath(Path, DataEntry->Description, DataEntry->ChannelMap, DataEntry->BulkData);
			if (Error == ESoundFileError::NONE)
			{
				DataEntry->SoundFileState.Set((int32)ESoundFileState::LOADED);
			}
			else
			{
				DataEntry->SoundFileState.Set((int32)ESoundFileState::HAS_ERROR);
			}
		}
	}

	TSharedPtr<FSoundFileDataEntry> FSoundFileManager::GetEntry(const FSoundFileHandle& Handle)
	{
		if (EntityManager.IsValidEntity(Handle))
		{
			uint32 Index = Handle.GetIndex();
			TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[Index];
			check(Entry->SoundFileHandle.Id == Handle.Id);
			return Entry;
		}
		return nullptr;
	}

	void FSoundFileManager::LogSoundFileMemoryInfo() const
	{
		UE_LOG(LogUnrealAudio, Display, TEXT("====== Sound file memory usage info ======"), NumBytesLoaded);
		UE_LOG(LogUnrealAudio, Display, TEXT("Bytes Loaded: %d (%.2f mb), percentage: %.2f"),
			   NumBytesLoaded, 
			   (float)NumBytesLoaded / (1024 * 1024), 
			   (float)NumBytesLoaded / Settings.TargetMemoryLimit
			);
		
		UE_LOG(LogUnrealAudio, Display, TEXT("Num Sound Files Loaded: %d"), NumSoundFilesLoaded);
		UE_LOG(LogUnrealAudio, Display, TEXT("Num Sound Files Streamed: %d"), NumSoundFilesStreamed);
		UE_LOG(LogUnrealAudio, Display, TEXT("Num Active Sounds: %d"), NumActiveSounds);
		UE_LOG(LogUnrealAudio, Display, TEXT("Num Inactive Sounds: %d"), NumInactiveSounds);

		UE_LOG(LogUnrealAudio, Display, TEXT("    Loaded Sounds: "));

		uint32 NumEntries = NumSoundFilesStreamed + NumSoundFilesLoaded;
		uint32 EntryCount = 0;
		for (int32 i = 0; i < SoundFileData.Num() && EntryCount < NumEntries; ++i)
		{
			if (SoundFileData[i].IsValid())
			{
				EntryCount++;
				TSharedPtr<FSoundFileDataEntry> Entry = SoundFileData[i];
				UE_LOG(LogUnrealAudio, Display, TEXT("    ------------------"), i);
				UE_LOG(LogUnrealAudio, Display, TEXT("    EntryIndex: %d"), i);
				UE_LOG(LogUnrealAudio, Display, TEXT("    Handle: %d"), Entry->SoundFileHandle.Id);
				UE_LOG(LogUnrealAudio, Display, TEXT("    Bytes: %d (%.2f mb)"), Entry->BulkData.Num(), (float)Entry->BulkData.Num() / (1024*1024));
				UE_LOG(LogUnrealAudio, Display, TEXT("    Streamed: %s"), (Entry->bIsStreamed ? TEXT("YES") : TEXT("NO")));
				UE_LOG(LogUnrealAudio, Display, TEXT("    NumReferences: %d"), Entry->NumReferences);
				UE_LOG(LogUnrealAudio, Display, TEXT("    TimeSinceUsed: %.2f"), Entry->TimeSinceUsed);
				UE_LOG(LogUnrealAudio, Display, TEXT("    NumFrames: %d"), Entry->Description.NumFrames);
				UE_LOG(LogUnrealAudio, Display, TEXT("    NumChannels: %d"), Entry->Description.NumChannels);
				UE_LOG(LogUnrealAudio, Display, TEXT("    SampleRate: %d"), Entry->Description.SampleRate);
			}
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("=========================================="), NumBytesLoaded);
	}

	/************************************************************************/
	/* UnrealAudioModule Implementations									*/
	/************************************************************************/

	TSharedPtr<ISoundFile> FUnrealAudioModule::LoadSoundFile(const FName& Path, bool bLoadAsync)
	{
		return SoundFileManager.LoadSoundFile(Path, bLoadAsync);
	}

	TSharedPtr<ISoundFile> FUnrealAudioModule::LoadSoundFile(const FName& Name, TArray<uint8>& InBulkData)
	{
		return SoundFileManager.LoadSoundFile(Name, InBulkData);
	}

	TSharedPtr<ISoundFile> FUnrealAudioModule::StreamSoundFile(const FName& Path, bool bLoadAsync)
	{
		return SoundFileManager.StreamSoundFile(Path, bLoadAsync);
	}

	FSoundFileManager& FUnrealAudioModule::GetSoundFileManager()
	{
		return SoundFileManager;
	}

	int32 FUnrealAudioModule::GetNumSoundFilesLoaded() const
	{
		return SoundFileManager.NumSoundFilesLoaded;
	}

	int32 FUnrealAudioModule::GetNumSoundFilesStreamed() const
	{
		return SoundFileManager.NumSoundFilesStreamed;
	}

	int32 FUnrealAudioModule::GetSoundFileNumBytes() const
	{
		MainThreadChecker.CheckThread();
		return SoundFileManager.NumBytesLoaded;
	}

	float FUnrealAudioModule::GetSoundFilePercentageOfTargetMemoryLimit() const
	{
		MainThreadChecker.CheckThread();
		return (float) SoundFileManager.NumBytesLoaded / SoundFileManager.Settings.TargetMemoryLimit;
	}

	void FUnrealAudioModule::LogSoundFileMemoryInfo() const
	{
		SoundFileManager.LogSoundFileMemoryInfo();
	}


}



#endif // #if ENABLE_UNREAL_AUDIO

