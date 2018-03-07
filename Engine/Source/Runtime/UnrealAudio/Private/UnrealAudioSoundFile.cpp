// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioSoundFile.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioSoundFileInternal.h"
#include "UnrealAudioSoundFileManager.h"
#include "Modules/ModuleManager.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	FSoundFile::FSoundFile(FSoundFileManager* InSoundFileManager)
		: SoundFileManager(InSoundFileManager)
		, Error(ESoundFileError::NONE)
	{
		check(SoundFileManager);
	}

	FSoundFile::~FSoundFile()
	{
		SoundFileManager->ReleaseSoundFileHandle(SoundFileHandle);
	}

	ESoundFileError::Type FSoundFile::GetState(ESoundFileState::Type& OutState) const
	{
		OutState = SoundFileManager->GetState(SoundFileHandle);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFile::GetError() const
	{
		return Error;
	}

	ESoundFileError::Type FSoundFile::GetId(uint32& OutId) const
	{
		if (!SoundFileManager->EntityManager.IsValidEntity(SoundFileHandle))
		{
			return ESoundFileError::INVALID_SOUND_FILE_HANDLE;
		}
		OutId = SoundFileHandle.Id;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFile::Init(const FSoundFileHandle& InSoundFileHandle)
	{
		if (!SoundFileManager->EntityManager.IsValidEntity(InSoundFileHandle))
		{
			return ESoundFileError::INVALID_SOUND_FILE_HANDLE;
		}
		SoundFileHandle = InSoundFileHandle;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFile::SetError(ESoundFileError::Type InError)
	{
		Error = InError;
		return Error;
	}

	ESoundFileError::Type FSoundFile::GetPath(FName& OutPath) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			OutPath = Entry->SoundFilePath;
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	ESoundFileError::Type FSoundFile::GetBulkData(TArray<uint8>** OutData) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			*OutData = &Entry->BulkData;
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	ESoundFileError::Type FSoundFile::GetDataSize(int32& OutDataSize) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			OutDataSize = Entry->BulkData.Num();
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	ESoundFileError::Type FSoundFile::GetDescription(FSoundFileDescription& OutDescription) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			OutDescription = Entry->Description;
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	ESoundFileError::Type FSoundFile::GetChannelMap(TArray<ESoundFileChannelMap::Type>& OutChannelMap) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			OutChannelMap = Entry->ChannelMap;
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	ESoundFileError::Type FSoundFile::IsStreamed(bool& bOutIsStreamed) const
	{
		TSharedPtr<FSoundFileDataEntry> Entry = SoundFileManager->GetEntry(SoundFileHandle);
		if (Entry.IsValid())
		{
			bOutIsStreamed = Entry->bIsStreamed;
			return ESoundFileError::NONE;
		}
		return ESoundFileError::INVALID_DATA;
	}

	void GetSoundFileListInDirectory(FString& Directory, TArray<FString>& SoundFiles, bool bRecursive /* = true */)
	{
		if (FPaths::DirectoryExists(Directory))
		{
			// Get input files in formats we're currently supporting
			IFileManager& FileManager = IFileManager::Get();
			if (bRecursive)
			{
				FileManager.FindFilesRecursive(SoundFiles, *Directory, TEXT("*.wav"), true, false, false);
				FileManager.FindFilesRecursive(SoundFiles, *Directory, TEXT("*.aif"), true, false, false);
				FileManager.FindFilesRecursive(SoundFiles, *Directory, TEXT("*.flac"), true, false, false);
				FileManager.FindFilesRecursive(SoundFiles, *Directory, TEXT("*.ogg"), true, false, false);
			}
			else
			{
				FileManager.FindFiles(SoundFiles, *(Directory / "*.wav"), true, false);
				FileManager.FindFiles(SoundFiles, *(Directory / "*.aif"), true, false);
				FileManager.FindFiles(SoundFiles, *(Directory / "*.flac"), true, false);
				FileManager.FindFiles(SoundFiles, *(Directory / "*.ogg"), true, false);
			}
		}
	}

}

#endif // #if ENABLE_UNREAL_AUDIO
