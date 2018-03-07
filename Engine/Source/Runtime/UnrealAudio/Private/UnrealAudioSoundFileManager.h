// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioHandles.h"
#include "Async/AsyncWork.h"

class Error;

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	class FUnrealAudioModule;

	class FAsyncSoundFileLoadTask : public FNonAbandonableTask
	{
	public:
		FAsyncSoundFileLoadTask(FUnrealAudioModule* InAudioModule, FSoundFileHandle InSoundFileHandle, const FString& InPath);
		~FAsyncSoundFileLoadTask();

		void DoWork();
		bool CanAbandon() { return false; }
		void Abandon() {}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncSoundFileLoadTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		FUnrealAudioModule* AudioModule;
		FSoundFileHandle SoundFileHandle;
		FString Path;
		friend class FAsyncTask<FAsyncSoundFileLoadTask>;
	};

	struct FSoundFileManagerSettings
	{
		int32			MaxNumberOfLoadedSounds;
		int32			NumLoadingThreads;
		int32			TargetMemoryLimit;
		float			FlushTimeThreshold;
		float			TimeDeltaPerUpdate;
		EThreadPriority LoadingThreadPriority;
	};

	struct FSoundFileDataEntry
	{
		class FSoundFileManager*			SoundFileManager;
		FName								SoundFilePath;
		FSoundFileHandle					SoundFileHandle;
		FSoundFileDescription				Description;
		TArray<ESoundFileChannelMap::Type>	ChannelMap;
		TArray<uint8>						BulkData;
		FThreadSafeCounter					SoundFileState;
		int32								NumReferences;
		ESoundFileError::Type				Error;
		float								TimeSinceUsed;
		bool								bIsStreamed;

		FSoundFileDataEntry(class FSoundFileManager* InSoundFileManager, const FSoundFileHandle& InSoundFileHandle, bool bInIsStreamed);
		~FSoundFileDataEntry();
	};

	class FSoundFileManager
	{
	public:
		FSoundFileManager(FUnrealAudioModule* InAudioModule);
		~FSoundFileManager();

		void Init(FSoundFileManagerSettings& InSettings);
		void Shutdown();

		TSharedPtr<ISoundFile> CreateSoundFile();

		/**
		 * Returns a new ISoundFile shared ptr from the given sound file handle. 
		 * If the SoundFileHandle is invalid, the returned shared ptr will not be valid.
		 * @param SoundFileHandle	The input sound file handle.
		 * @return					TSharedPtr of ISoundFile.
		 */
		TSharedPtr<ISoundFile> GetSoundFile(const FSoundFileHandle& SoundFileHandle);

		/** 
		 * Asynchronously loads a sound file into memory from the given bulk data array.
		 */
		TSharedPtr<ISoundFile> LoadSoundFile(const FName& Name, TArray<uint8>& InBulkData);

		/** 
		 * Asynchronously loads a sound file into memory from the given file path.
		 */
		TSharedPtr<ISoundFile> LoadSoundFile(const FName& Path, bool bLoadAsync);

		/** 
		 * Creates a streaming sound file (doesn't load into memory but does parse file header)
		 */
		TSharedPtr<ISoundFile> StreamSoundFile(const FName& Path, bool bLoadAsync);

		ESoundFileState::Type GetState(const FSoundFileHandle& SoundFileHandle) const;

		void ReleaseSoundFileHandle(const FSoundFileHandle& SoundFileHandle);

		void LogSoundFileMemoryInfo() const;

		void Update();

	private:

		int32 FlushSoundFileDataIndex(int32 Index);
		TSharedPtr<ISoundFile> CreateNewSoundFile(const FName& Path, bool bIsStreamed, bool bLoadAsync);
		void LoadSoundFileDataEntry(TSharedPtr<FSoundFileDataEntry> DataEntry);
		TSharedPtr<FSoundFileDataEntry> GetEntry(const FSoundFileHandle& Handle);

		FEntityManager EntityManager;
		FUnrealAudioModule* AudioModule;
		FSoundFileManagerSettings Settings;

		FQueuedThreadPool* FileLoadingThreadPool;

		TArray<FSoundFileHandle>				SoundFileHandles;
		TArray<TSharedPtr<FSoundFileDataEntry>>	SoundFileData;

		// Sorted by last time active (greatest "last used" value on top)
		TMap<FName, FSoundFileHandle>			NameToLoadedSoundMap;
		TMap<FName, FSoundFileHandle>			NameToStreamedSoundMap;
		int32									NumSoundFilesLoaded;
		int32									NumSoundFilesStreamed;
		int32									NumBytesLoaded;
		int32									NumActiveSounds;
		int32									NumInactiveSounds;
		bool									bLogOverMemoryTarget;

		FThreadChecker MainThreadChecker;

		struct FSortedSoundFileEntry
		{
			// Index of this entry in the sound file data array
			int32 Index;

			float TimeSinceUsed;

			FSortedSoundFileEntry(int32 InIndex, float InTimeSinceUsed)
				: Index(InIndex)
				, TimeSinceUsed(InTimeSinceUsed)
			{}
		};

		/** A sound file entry compare predicate. */
		struct SoundFileEntryComparePredicate
		{
			bool operator()(const FSortedSoundFileEntry& A, const FSortedSoundFileEntry& B) const
			{
				return A.TimeSinceUsed > B.TimeSinceUsed;
			}
		};

		TArray<FSortedSoundFileEntry> LeastRecentlyUsedSoundFiles;

		friend class FAsyncSoundFileLoadTask;
		friend struct FSoundFileDataEntry;
		friend class FUnrealAudioModule;
		friend class FSoundFile;
	};



}



#endif // #if ENABLE_UNREAL_AUDIO

