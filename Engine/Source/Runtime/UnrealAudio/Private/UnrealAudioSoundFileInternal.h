// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioHandles.h"

class Error;

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	class FUnrealAudioModule;

	typedef int64 SoundFileCount;
	typedef struct SoundFileHandleOpaque LibSoundFileHandle;

	namespace ESoundFileSeekMode
	{
		enum Type
		{
			FROM_START = 0,
			FROM_CURRENT = 1,
			FROM_END = 2,
		};
	}

	namespace ESoundFileOpenMode
	{
		enum Type
		{
			READING = 0x10,
			WRITING = 0x20,
			UNKNOWN = 0,
		};
	}

	class ISoundFileParser
	{
	public:
		virtual ~ISoundFileParser() {}

		virtual ESoundFileError::Type GetLengthBytes(SoundFileCount& OutLength) const = 0;
		virtual ESoundFileError::Type SeekBytes(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset) = 0;
		virtual ESoundFileError::Type ReadBytes(void* DataPtr, SoundFileCount NumBytes, SoundFileCount& NumBytesRead) = 0;
		virtual ESoundFileError::Type WriteBytes(const void* DataPtr, SoundFileCount NumBytes, SoundFileCount& NumBytesWritten) = 0;
		virtual ESoundFileError::Type GetOffsetBytes(SoundFileCount& OutOffset) const = 0;
	};

	class FSoundFileReader : public ISoundFileParser
	{
	public:
		FSoundFileReader(FUnrealAudioModule* AudioModule);
		~FSoundFileReader();

		// ISoundFileParser
		ESoundFileError::Type GetLengthBytes(SoundFileCount& OutLength) const override;
		ESoundFileError::Type SeekBytes(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset) override;
		ESoundFileError::Type ReadBytes(void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesRead) override;
		ESoundFileError::Type WriteBytes(const void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesWritten) override;
		ESoundFileError::Type GetOffsetBytes(SoundFileCount& OutOffset) const override;

		virtual ESoundFileError::Type Init(TSharedPtr<ISoundFile> InSoundFileData, bool bIsStreamed);
		virtual ESoundFileError::Type Release();
		virtual ESoundFileError::Type SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset);
		virtual ESoundFileError::Type ReadFrames(float* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead);
		virtual ESoundFileError::Type ReadFrames(double* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead);
		virtual ESoundFileError::Type ReadSamples(float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead);
		virtual ESoundFileError::Type ReadSamples(double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead);

	private:
		ESoundFileError::Type InitLoaded(TSharedPtr<ISoundFile> InSoundFileData);
		ESoundFileError::Type InitStreamed(TSharedPtr<ISoundFile> InSoundFileData);
		ESoundFileError::Type SetError(ESoundFileError::Type InError);

		TSharedPtr<ISoundFile>	SoundFileData;
		FUnrealAudioModule*		AudioModule;
		SoundFileCount			CurrentIndexBytes;
		LibSoundFileHandle*		FileHandle;
		FThreadSafeCounter		State;
		FThreadSafeCounter		CurrentError;
	};

	class FSoundFileWriter : public ISoundFileParser
	{
	public:
		FSoundFileWriter(FUnrealAudioModule* AudioModule);
		~FSoundFileWriter();

		// ISoundFileParser
		ESoundFileError::Type GetLengthBytes(SoundFileCount& OutLength) const override;
		ESoundFileError::Type SeekBytes(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset) override;
		ESoundFileError::Type ReadBytes(void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesRead) override;
		ESoundFileError::Type WriteBytes(const void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesWritten) override;
		ESoundFileError::Type GetOffsetBytes(SoundFileCount& OutOffset) const override;

		virtual ESoundFileError::Type Init(const FSoundFileDescription& FileDescription, const TArray<ESoundFileChannelMap::Type>& InChannelMap, double EncodingQuality);
		virtual ESoundFileError::Type Release();
		virtual ESoundFileError::Type SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset);
		virtual ESoundFileError::Type WriteFrames(const float* Data, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten);
		virtual ESoundFileError::Type WriteFrames(const double* Data, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten);
		virtual ESoundFileError::Type WriteSamples(const float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten);
		virtual ESoundFileError::Type WriteSamples(const double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten);
		virtual ESoundFileError::Type GetData(TArray<uint8>** OutData);

	private:
		ESoundFileError::Type SetError(ESoundFileError::Type InError);

		FUnrealAudioModule*					AudioModule;
		SoundFileCount						CurrentIndexBytes;
		LibSoundFileHandle*					FileHandle;
		FSoundFileDescription				Description;
		TArray<ESoundFileChannelMap::Type>	ChannelMap;
		TArray<uint8>						BulkData;
		double								EncodingQuality;
		FThreadSafeCounter					State;
		FThreadSafeCounter					CurrentError;
	};


	class FSoundFile : public ISoundFile
	{
	public:
		FSoundFile(class FSoundFileManager* InSoundFileManager);
		~FSoundFile();

		ESoundFileError::Type GetState(ESoundFileState::Type& OutState) const override;
		ESoundFileError::Type GetError() const override;
		ESoundFileError::Type GetId(uint32& OutId) const override;
		ESoundFileError::Type GetPath(FName& OutPath) const override;
		ESoundFileError::Type GetBulkData(TArray<uint8>** OutData) const override;
		ESoundFileError::Type GetDataSize(int32& DataSize) const override;
		ESoundFileError::Type GetDescription(FSoundFileDescription& OutDescription) const override;
		ESoundFileError::Type GetChannelMap(TArray<ESoundFileChannelMap::Type>& OutChannelMap) const override;
		ESoundFileError::Type IsStreamed(bool& bOutIsStreamed) const override;

		// Internal API
		ESoundFileError::Type Init(const FSoundFileHandle& InSoundFileHandle);
		ESoundFileError::Type SetError(ESoundFileError::Type InError);

	private:
		FSoundFileHandle SoundFileHandle;
		class FSoundFileManager* SoundFileManager;
		ESoundFileError::Type Error;
	};

	ESoundFileError::Type LoadSoundFileFromPath(const FString& FilePath, FSoundFileDescription& Description, TArray<ESoundFileChannelMap::Type>& ChannelMap, TArray<uint8>& BulkData);
	ESoundFileError::Type GetSoundFileInfoFromPath(const FString& FilePath, FSoundFileDescription& Description, TArray<ESoundFileChannelMap::Type>& ChannelMap);

}



#endif // #if ENABLE_UNREAL_AUDIO

