// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioPrivate.h"
#include "Modules/ModuleManager.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	// Virtual Sound File Function Pointers
	typedef SoundFileCount(*VirtualSoundFileGetLengthFuncPtr)(void* UserData);
	typedef SoundFileCount(*VirtualSoundFileSeekFuncPtr)(SoundFileCount Offset, int32 Mode, void* UserData);
	typedef SoundFileCount(*VirtualSoundFileReadFuncPtr)(void* DataPtr, SoundFileCount ByteCount, void* UserData);
	typedef SoundFileCount(*VirtualSoundFileWriteFuncPtr)(const void* DataPtr, SoundFileCount ByteCount, void* UserData);
	typedef SoundFileCount(*VirtualSoundFileTellFuncPtr)(void* UserData);

	// Struct describing function pointers to call for virtual file IO
	struct FVirtualSoundFileCallbackInfo
	{
		VirtualSoundFileGetLengthFuncPtr VirtualSoundFileGetLength;
		VirtualSoundFileSeekFuncPtr VirtualSoundFileSeek;
		VirtualSoundFileReadFuncPtr VirtualSoundFileRead;
		VirtualSoundFileWriteFuncPtr VirtualSoundFileWrite;
		VirtualSoundFileTellFuncPtr VirtualSoundFileTell;
	};

	// SoundFile Constants
	static const int32 SET_ENCODING_QUALITY = 0x1300;
	static const int32 SET_CHANNEL_MAP_INFO = 0x1101;
	static const int32 GET_CHANNEL_MAP_INFO = 0x1100;

	// Exported SoundFile Functions
	typedef LibSoundFileHandle*(*SoundFileOpenFuncPtr)(const char* Path, int32 Mode, FSoundFileDescription* Description);
	typedef LibSoundFileHandle*(*SoundFileOpenVirtualFuncPtr)(FVirtualSoundFileCallbackInfo* VirtualFileDescription, int32 Mode, FSoundFileDescription* Description, void* UserData);
	typedef int32(*SoundFileCloseFuncPtr)(LibSoundFileHandle* FileHandle);
	typedef int32(*SoundFileErrorFuncPtr)(LibSoundFileHandle* FileHandle);
	typedef const char*(*SoundFileStrErrorFuncPtr)(LibSoundFileHandle* FileHandle);
	typedef const char*(*SoundFileErrorNumberFuncPtr)(int32 ErrorNumber);
	typedef int32(*SoundFileCommandFuncPtr)(LibSoundFileHandle* FileHandle, int32 Command, void* Data, int32 DataSize);
	typedef int32(*SoundFileFormatCheckFuncPtr)(const FSoundFileDescription* Description);
	typedef SoundFileCount(*SoundFileSeekFuncPtr)(LibSoundFileHandle* FileHandle, SoundFileCount NumFrames, int32 SeekMode);
	typedef const char*(*SoundFileGetVersionFuncPtr)(void);
	typedef SoundFileCount(*SoundFileReadFramesFloatFuncPtr)(LibSoundFileHandle* FileHandle, float* Buffer, SoundFileCount NumFrames);
	typedef SoundFileCount(*SoundFileReadFramesDoubleFuncPtr)(LibSoundFileHandle* FileHandle, double* Buffer, SoundFileCount NumFrames);
	typedef SoundFileCount(*SoundFileWriteFramesFloatFuncPtr)(LibSoundFileHandle* FileHandle, const float* Buffer, SoundFileCount NumFrames);
	typedef SoundFileCount(*SoundFileWriteFramesDoubleFuncPtr)(LibSoundFileHandle* FileHandle, const double* Buffer, SoundFileCount NumFrames);
	typedef SoundFileCount(*SoundFileReadSamplesFloatFuncPtr)(LibSoundFileHandle* FileHandle, float* Buffer, SoundFileCount NumSamples);
	typedef SoundFileCount(*SoundFileReadSamplesDoubleFuncPtr)(LibSoundFileHandle* FileHandle, double* Buffer, SoundFileCount NumSamples);
	typedef SoundFileCount(*SoundFileWriteSamplesFloatFuncPtr)(LibSoundFileHandle* FileHandle, const float* Buffer, SoundFileCount NumSamples);
	typedef SoundFileCount(*SoundFileWriteSamplesDoubleFuncPtr)(LibSoundFileHandle* FileHandle, const double* Buffer, SoundFileCount NumSamples);

	SoundFileOpenFuncPtr SoundFileOpen = nullptr;
	SoundFileOpenVirtualFuncPtr SoundFileOpenVirtual = nullptr;
	SoundFileCloseFuncPtr SoundFileClose = nullptr;
	SoundFileErrorFuncPtr SoundFileError = nullptr;
	SoundFileStrErrorFuncPtr SoundFileStrError = nullptr;
	SoundFileErrorNumberFuncPtr SoundFileErrorNumber = nullptr;
	SoundFileCommandFuncPtr SoundFileCommand = nullptr;
	SoundFileFormatCheckFuncPtr SoundFileFormatCheck = nullptr;
	SoundFileSeekFuncPtr SoundFileSeek = nullptr;
	SoundFileGetVersionFuncPtr SoundFileGetVersion = nullptr;
	SoundFileReadFramesFloatFuncPtr SoundFileReadFramesFloat = nullptr;
	SoundFileReadFramesDoubleFuncPtr SoundFileReadFramesDouble = nullptr;
	SoundFileWriteFramesFloatFuncPtr SoundFileWriteFramesFloat = nullptr;
	SoundFileWriteFramesDoubleFuncPtr SoundFileWriteFramesDouble = nullptr;
	SoundFileReadSamplesFloatFuncPtr SoundFileReadSamplesFloat = nullptr;
	SoundFileReadSamplesDoubleFuncPtr SoundFileReadSamplesDouble = nullptr;
	SoundFileWriteSamplesFloatFuncPtr SoundFileWriteSamplesFloat = nullptr;
	SoundFileWriteSamplesDoubleFuncPtr SoundFileWriteSamplesDouble = nullptr;

	/**
	Function implementations of virtual function callbacks
	*/

	static SoundFileCount OnSoundFileGetLengthBytes(void* UserData)
	{
		SoundFileCount Length = 0;
		((ISoundFileParser*)UserData)->GetLengthBytes(Length);
		return Length;
	}

	static SoundFileCount OnSoundFileSeekBytes(SoundFileCount Offset, int32 Mode, void* UserData)
	{
		SoundFileCount OutOffset = 0;
		((ISoundFileParser*)UserData)->SeekBytes(Offset, (ESoundFileSeekMode::Type)Mode, OutOffset);
		return OutOffset;
	}

	static SoundFileCount OnSoundFileReadBytes(void* DataPtr, SoundFileCount ByteCount, void* UserData)
	{
		SoundFileCount OutBytesRead = 0;
		((ISoundFileParser*)UserData)->ReadBytes(DataPtr, ByteCount, OutBytesRead);
		return OutBytesRead;
	}

	static SoundFileCount OnSoundFileWriteBytes(const void* DataPtr, SoundFileCount ByteCount, void* UserData)
	{
		SoundFileCount OutBytesWritten = 0;
		((ISoundFileParser*)UserData)->WriteBytes(DataPtr, ByteCount, OutBytesWritten);
		return OutBytesWritten;
	}

	static SoundFileCount OnSoundFileTell(void* UserData)
	{
		SoundFileCount OutOffset = 0;
		((ISoundFileParser*)UserData)->GetOffsetBytes(OutOffset);
		return OutOffset;
	}

	/**
	* Gets the default channel mapping for the given channel number
	*/
	static void GetDefaultMappingsForChannelNumber(int32 NumChannels, TArray<ESoundFileChannelMap::Type>& ChannelMap)
	{
		check(ChannelMap.Num() == NumChannels);

		switch (NumChannels)
		{
		case 1:	// MONO
		ChannelMap[0] = ESoundFileChannelMap::MONO;
		break;

		case 2:	// STEREO
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		break;

		case 3:	// 2.1
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::LFE;
		break;

		case 4: // Quadraphonic
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::BACK_LEFT;
		ChannelMap[3] = ESoundFileChannelMap::BACK_RIGHT;
		break;

		case 5: // 5.0
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::CENTER;
		ChannelMap[3] = ESoundFileChannelMap::SIDE_LEFT;
		ChannelMap[4] = ESoundFileChannelMap::SIDE_RIGHT;
		break;

		case 6: // 5.1
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::CENTER;
		ChannelMap[3] = ESoundFileChannelMap::LFE;
		ChannelMap[4] = ESoundFileChannelMap::SIDE_LEFT;
		ChannelMap[5] = ESoundFileChannelMap::SIDE_RIGHT;
		break;

		case 7: // 6.1
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::CENTER;
		ChannelMap[3] = ESoundFileChannelMap::LFE;
		ChannelMap[4] = ESoundFileChannelMap::SIDE_LEFT;
		ChannelMap[5] = ESoundFileChannelMap::SIDE_RIGHT;
		ChannelMap[6] = ESoundFileChannelMap::BACK_CENTER;
		break;

		case 8: // 7.1
		ChannelMap[0] = ESoundFileChannelMap::LEFT;
		ChannelMap[1] = ESoundFileChannelMap::RIGHT;
		ChannelMap[2] = ESoundFileChannelMap::CENTER;
		ChannelMap[3] = ESoundFileChannelMap::LFE;
		ChannelMap[4] = ESoundFileChannelMap::BACK_LEFT;
		ChannelMap[5] = ESoundFileChannelMap::BACK_RIGHT;
		ChannelMap[6] = ESoundFileChannelMap::SIDE_LEFT;
		ChannelMap[7] = ESoundFileChannelMap::SIDE_RIGHT;
		break;

		default:
		break;
		}
	}

	static ESoundFileError::Type GetSoundDesriptionInternal(LibSoundFileHandle** OutFileHandle, const FString& FilePath, FSoundFileDescription& OutputDescription, TArray<ESoundFileChannelMap::Type>& OutChannelMap)
	{
		*OutFileHandle = nullptr;

		// Check to see if the file exists
		if (!FPaths::FileExists(FilePath))
		{
			UE_LOG(LogUnrealAudio, Error, TEXT("Sound file %s doesn't exist."), *FilePath);
			return ESoundFileError::FILE_DOESNT_EXIST;
		}

		// open a sound file handle to get the description
		*OutFileHandle = SoundFileOpen(TCHAR_TO_ANSI(*FilePath), ESoundFileOpenMode::READING, &OutputDescription);
		if (!*OutFileHandle)
		{
			FString StrError = FString(SoundFileStrError(nullptr));
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to open sound file %s: %s"), *FilePath, *StrError);
			return ESoundFileError::FAILED_TO_OPEN;
		}

		// Try to get a channel mapping
		int32 NumChannels = OutputDescription.NumChannels;
		OutChannelMap.Init(ESoundFileChannelMap::INVALID, NumChannels);

		int32 Result = SoundFileCommand(*OutFileHandle, GET_CHANNEL_MAP_INFO, (int32*)OutChannelMap.GetData(), sizeof(int32)*NumChannels);

		// If we failed to get the file's channel map definition, then we set the default based on the number of channels
		if (Result == 0)
		{
			GetDefaultMappingsForChannelNumber(NumChannels, OutChannelMap);
		}
		else
		{
			// Check to see if the channel map we did get back is filled with INVALID channels
			bool bIsInvalid = false;
			for (ESoundFileChannelMap::Type ChannelType : OutChannelMap)
			{
				if (ChannelType == ESoundFileChannelMap::INVALID)
				{
					bIsInvalid = true;
					break;
				}
			}
			// If invalid, then we need to get the default channel mapping
			if (bIsInvalid)
			{
				GetDefaultMappingsForChannelNumber(NumChannels, OutChannelMap);
			}
		}

		return ESoundFileError::NONE;
	}

	/************************************************************************/
	/* FSoundFileReader														*/
	/************************************************************************/

	FSoundFileReader::FSoundFileReader(FUnrealAudioModule* InAudioModule)
		: AudioModule(InAudioModule)
		, CurrentIndexBytes(0)
		, FileHandle(nullptr)
		, State(ESoundFileState::UNINITIALIZED)
		, CurrentError(ESoundFileError::NONE)
	{
	}

	FSoundFileReader::~FSoundFileReader()
	{
		Release();
		check(FileHandle == nullptr);
	}

	ESoundFileError::Type FSoundFileReader::GetLengthBytes(SoundFileCount& OutLength) const
	{
		if (!SoundFileData.IsValid())
		{
			return ESoundFileError::INVALID_DATA;
		}

		int32 DataSize;
		ESoundFileError::Type Error = SoundFileData->GetDataSize(DataSize);
		if (Error == ESoundFileError::NONE)
		{
			OutLength = DataSize;
			return ESoundFileError::NONE;
		}
		return Error;
	}

	ESoundFileError::Type FSoundFileReader::SeekBytes(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset)
	{
		if (!SoundFileData.IsValid())
		{
			return ESoundFileError::INVALID_DATA;
		}
 
		int32 DataSize;
		ESoundFileError::Type Error = SoundFileData->GetDataSize(DataSize);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		SoundFileCount MaxBytes = DataSize;
		if (MaxBytes == 0)
		{
			OutOffset = 0;
			CurrentIndexBytes = 0;
			return ESoundFileError::NONE;
		}
 
		switch (SeekMode)
		{
			case ESoundFileSeekMode::FROM_START:
				CurrentIndexBytes = Offset;
				break;

			case ESoundFileSeekMode::FROM_CURRENT:
				CurrentIndexBytes += Offset;
				break;

			case ESoundFileSeekMode::FROM_END:
				CurrentIndexBytes = MaxBytes + Offset;
				break;

			default:
				checkf(false, TEXT("Uknown seek mode!"));
				break;
		}

		// Wrap the byte index to fall between 0 and MaxBytes
		while (CurrentIndexBytes < 0)
		{
			CurrentIndexBytes += MaxBytes;
		}

		while (CurrentIndexBytes > MaxBytes)
		{
			CurrentIndexBytes -= MaxBytes;
		}

		OutOffset = CurrentIndexBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::ReadBytes(void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesRead)
	{
		if (!SoundFileData.IsValid())
		{
			return ESoundFileError::INVALID_DATA;
		}

		SoundFileCount EndByte = CurrentIndexBytes + NumBytes;

		int32 DataSize;
		ESoundFileError::Type Error = SoundFileData->GetDataSize(DataSize);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}
		SoundFileCount MaxBytes = DataSize;
		if (EndByte >= MaxBytes)
		{
			NumBytes = MaxBytes - CurrentIndexBytes;
		}

		if (NumBytes > 0)
		{
			TArray<uint8>* BulkData = nullptr;
			Error = SoundFileData->GetBulkData(&BulkData);
			if (Error != ESoundFileError::NONE)
			{
				return Error;
			}

			check(BulkData != nullptr);

			FMemory::Memcpy(DataPtr, (const void*)&(*BulkData)[CurrentIndexBytes], NumBytes);
			CurrentIndexBytes += NumBytes;
		}
		OutNumBytesRead = NumBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::WriteBytes(const void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesWritten)
	{
		// This should never get called in the reader class
		check(false);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::GetOffsetBytes(SoundFileCount& OutOffset) const
	{
		OutOffset = CurrentIndexBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::Init(TSharedPtr<ISoundFile> InSoundFileData, bool bIsStreamed)
	{
		if (bIsStreamed)
		{
			return InitStreamed(InSoundFileData);
		}
		else
		{
			return InitLoaded(InSoundFileData);
		}
	}

	ESoundFileError::Type FSoundFileReader::Release()
	{
		if (FileHandle)
		{
			SoundFileClose(FileHandle);
			FileHandle = nullptr;
		}
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset)
	{
		SoundFileCount Pos = SoundFileSeek(FileHandle, Offset, (int32)SeekMode);
		if (Pos == -1)
		{
			FString StrErr = SoundFileStrError(FileHandle);
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to seek file: %s"), *StrErr);
			return SetError(ESoundFileError::FAILED_TO_SEEK);
		}
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::ReadFrames(float* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead)
	{
		OutNumFramesRead = SoundFileReadFramesFloat(FileHandle, DataPtr, NumFrames);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::ReadFrames(double* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead)
	{
		OutNumFramesRead = SoundFileReadFramesDouble(FileHandle, DataPtr, NumFrames);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::ReadSamples(float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead)
	{
		OutNumSamplesRead = SoundFileReadSamplesFloat(FileHandle, DataPtr, NumSamples);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::ReadSamples(double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead)
	{
		OutNumSamplesRead = SoundFileReadSamplesDouble(FileHandle, DataPtr, NumSamples);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::InitLoaded(TSharedPtr<ISoundFile> InSoundFileData)
	{
		if (!(State.GetValue() == ESoundFileState::UNINITIALIZED || State.GetValue() == ESoundFileState::LOADING))
		{
			return SetError(ESoundFileError::ALREADY_INITIALIZED);
		}

		check(InSoundFileData.IsValid());
 		check(FileHandle == nullptr);

		// Setting sound file data initializes this sound file
		SoundFileData = InSoundFileData;
		check(SoundFileData.IsValid());

		bool bIsStreamed;
		ESoundFileError::Type Error = SoundFileData->IsStreamed(bIsStreamed);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		if (bIsStreamed)
		{
			return ESoundFileError::INVALID_DATA;
		}

		ESoundFileState::Type SoundFileState;
		Error = SoundFileData->GetState(SoundFileState);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		if (SoundFileState != ESoundFileState::LOADED)
		{
			return ESoundFileError::INVALID_STATE;
		}

 		// Open up a virtual file handle with this data
		FVirtualSoundFileCallbackInfo VirtualSoundFileInfo;
		VirtualSoundFileInfo.VirtualSoundFileGetLength	= OnSoundFileGetLengthBytes;
		VirtualSoundFileInfo.VirtualSoundFileSeek		= OnSoundFileSeekBytes;
		VirtualSoundFileInfo.VirtualSoundFileRead		= OnSoundFileReadBytes;
		VirtualSoundFileInfo.VirtualSoundFileWrite		= OnSoundFileWriteBytes;
		VirtualSoundFileInfo.VirtualSoundFileTell		= OnSoundFileTell;
 
		FSoundFileDescription Description;
		SoundFileData->GetDescription(Description);
 		if (!SoundFileFormatCheck(&Description))
 		{
 			return SetError(ESoundFileError::INVALID_INPUT_FORMAT);
 		}
 
 		FileHandle = SoundFileOpenVirtual(&VirtualSoundFileInfo, ESoundFileOpenMode::READING, &Description, (void*)this);
 		if (!FileHandle)
 		{
 			FString StrErr = SoundFileStrError(nullptr);
 			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to intitialize sound file: %s"), *StrErr);
 			return SetError(ESoundFileError::FAILED_TO_OPEN);
 		}
 
 		State.Set(ESoundFileState::INITIALIZED);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileReader::InitStreamed(TSharedPtr<ISoundFile> InSoundFileData)
	{
		if (!(State.GetValue() == ESoundFileState::UNINITIALIZED || State.GetValue() == ESoundFileState::LOADING))
		{
			return SetError(ESoundFileError::ALREADY_INITIALIZED);
		}

		check(InSoundFileData.IsValid());
		check(FileHandle == nullptr);

		// Setting sound file data initializes this sound file
		SoundFileData = InSoundFileData;
		check(SoundFileData.IsValid());

		bool bIsStreamed;
		ESoundFileError::Type Error = SoundFileData->IsStreamed(bIsStreamed);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		if (!bIsStreamed)
		{
			return ESoundFileError::INVALID_DATA;
		}

		ESoundFileState::Type SoundFileState;
		Error = SoundFileData->GetState(SoundFileState);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		if (SoundFileState != ESoundFileState::STREAMING)
		{
			return ESoundFileError::INVALID_STATE;
		}

		FName NamePath;
		Error = SoundFileData->GetPath(NamePath);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		FString FilePath = NamePath.GetPlainNameString();

		FSoundFileDescription Description;
		TArray<ESoundFileChannelMap::Type> ChannelMap;
		Error = GetSoundDesriptionInternal(&FileHandle, FilePath, Description, ChannelMap);
		if (Error == ESoundFileError::NONE)
		{
			// Tell this reader that we're in streaming mode.
			State.Set(ESoundFileState::STREAMING);
			return ESoundFileError::NONE;
		}
		else
		{
			return SetError(Error);
		}
	}

	ESoundFileError::Type FSoundFileReader::SetError(ESoundFileError::Type InError)
	{
		if (InError != ESoundFileError::NONE)
		{
			State.Set(ESoundFileState::HAS_ERROR);
		}
		CurrentError.Set(InError);
		return InError;
	}


	/************************************************************************/
	/* FSoundFileWriter														*/
	/************************************************************************/

	FSoundFileWriter::FSoundFileWriter(FUnrealAudioModule* InAudioModule)
		: AudioModule(InAudioModule)
		, CurrentIndexBytes(0)
		, FileHandle(nullptr)
		, EncodingQuality(0.0)
		, State(ESoundFileState::UNINITIALIZED)
		, CurrentError(ESoundFileError::NONE)
	{

	}

	FSoundFileWriter::~FSoundFileWriter()
	{

	}

	ESoundFileError::Type FSoundFileWriter::GetLengthBytes(SoundFileCount& OutLength) const
	{
		OutLength = BulkData.Num();
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::SeekBytes(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset)
	{
		int32 DataSize = BulkData.Num();

		SoundFileCount MaxBytes = DataSize;
		if (MaxBytes == 0)
		{
			OutOffset = 0;
			CurrentIndexBytes = 0;
			return ESoundFileError::NONE;
		}

		switch (SeekMode)
		{
			case ESoundFileSeekMode::FROM_START:
				CurrentIndexBytes = Offset;
				break;

			case ESoundFileSeekMode::FROM_CURRENT:
				CurrentIndexBytes += Offset;
				break;

			case ESoundFileSeekMode::FROM_END:
				CurrentIndexBytes = MaxBytes + Offset;
				break;

			default:
				checkf(false, TEXT("Uknown seek mode!"));
				break;
		}

		// Wrap the byte index to fall between 0 and MaxBytes
		while (CurrentIndexBytes < 0)
		{
			CurrentIndexBytes += MaxBytes;
		}

		while (CurrentIndexBytes > MaxBytes)
		{
			CurrentIndexBytes -= MaxBytes;
		}

		OutOffset = CurrentIndexBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::ReadBytes(void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesRead)
	{
		// This shouldn't get called in the writer
		check(false);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::WriteBytes(const void* DataPtr, SoundFileCount NumBytes, SoundFileCount& OutNumBytesWritten)
	{
		const uint8* InDataBytes = (const uint8*)DataPtr;

		SoundFileCount MaxBytes = BulkData.Num();

		// Append the data to the buffer
		if (CurrentIndexBytes == MaxBytes)
		{
			BulkData.Append(InDataBytes, NumBytes);
			CurrentIndexBytes += NumBytes;
		}
		else if ((CurrentIndexBytes + NumBytes) < MaxBytes)
		{
			// Write over the existing data in the buffer
			for (SoundFileCount i = 0; i < NumBytes; ++i)
			{
				BulkData[CurrentIndexBytes++] = InDataBytes[i];
			}
		}
		else
		{
			// Overwrite some data until end of current buffer size
			SoundFileCount RemainingBytes = MaxBytes - CurrentIndexBytes;
			SoundFileCount InputBufferIndex = 0;
			for (InputBufferIndex = 0; InputBufferIndex < RemainingBytes; ++InputBufferIndex)
			{
				BulkData[CurrentIndexBytes++] = InDataBytes[InputBufferIndex];
			}

			// Now append the remainder of the input buffer to the internal data buffer
			const uint8* RemainingData = &InDataBytes[InputBufferIndex];
			RemainingBytes = NumBytes - RemainingBytes;
			BulkData.Append(RemainingData, RemainingBytes);
			CurrentIndexBytes += RemainingBytes;
			check(CurrentIndexBytes == BulkData.Num());
		}
		OutNumBytesWritten = NumBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::GetOffsetBytes(SoundFileCount& OutOffset) const
	{
		OutOffset = CurrentIndexBytes;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::Init(const FSoundFileDescription& InDescription, const TArray<ESoundFileChannelMap::Type>& InChannelMap, double InEncodingQuality)
	{
		State.Set(ESoundFileState::INITIALIZED);

		BulkData.Reset();
		Description		= InDescription;
		ChannelMap		= InChannelMap;
		EncodingQuality = InEncodingQuality;

		// First check the input format to make sure it's valid
		if (!SoundFileFormatCheck(&InDescription))
		{
			UE_LOG(LogUnrealAudio,
				   Error,
				   TEXT("Sound file input format (%s - %s) is invalid."),
				   ESoundFileFormat::ToStringMajor(InDescription.FormatFlags),
				   ESoundFileFormat::ToStringMinor(InDescription.FormatFlags));

			return SetError(ESoundFileError::INVALID_INPUT_FORMAT);
		}

		// Make sure we have the right number of channels and our channel map size
		if (InChannelMap.Num() != InDescription.NumChannels)
		{
			UE_LOG(LogUnrealAudio, Error, TEXT("Channel map didn't match the input NumChannels"));
			return SetError(ESoundFileError::INVALID_CHANNEL_MAP);
		}

		FVirtualSoundFileCallbackInfo VirtualSoundFileInfo;
		VirtualSoundFileInfo.VirtualSoundFileGetLength	= OnSoundFileGetLengthBytes;
		VirtualSoundFileInfo.VirtualSoundFileSeek		= OnSoundFileSeekBytes;
		VirtualSoundFileInfo.VirtualSoundFileRead		= OnSoundFileReadBytes;
		VirtualSoundFileInfo.VirtualSoundFileWrite		= OnSoundFileWriteBytes;
		VirtualSoundFileInfo.VirtualSoundFileTell		= OnSoundFileTell;

		FileHandle = SoundFileOpenVirtual(&VirtualSoundFileInfo, ESoundFileOpenMode::WRITING, &Description, (void*)this);
		if (!FileHandle)
		{
			FString StrErr = FString(SoundFileStrError(nullptr));
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to open empty sound file: %s"), *StrErr);
			return SetError(ESoundFileError::FAILED_TO_OPEN);
		}

		int32 Result = SoundFileCommand(FileHandle, SET_CHANNEL_MAP_INFO, (int32*)InChannelMap.GetData(), sizeof(int32)*Description.NumChannels);
		if (Result)
		{
			FString StrErr = SoundFileStrError(nullptr);
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to set the channel map on empty file for writing: %s"), *StrErr);
			return SetError(ESoundFileError::INVALID_CHANNEL_MAP);
		}

		if ((Description.FormatFlags & ESoundFileFormat::MAJOR_FORMAT_MASK) == ESoundFileFormat::OGG)
		{
			int32 Result2 = SoundFileCommand(FileHandle, SET_ENCODING_QUALITY, &EncodingQuality, sizeof(double));
			if (Result2 != 1)
			{
				FString StrErr = SoundFileStrError(FileHandle);
				UE_LOG(LogUnrealAudio, Error, TEXT("Failed to set encoding quality: %s"), *StrErr);
				return SetError(ESoundFileError::BAD_ENCODING_QUALITY);
			}
		}

		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::Release()
	{
		if (FileHandle)
		{
			SoundFileClose(FileHandle);
			FileHandle = nullptr;
		}
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset)
	{
		SoundFileCount Pos = SoundFileSeek(FileHandle, Offset, (int32)SeekMode);
		if (Pos == -1)
		{
			FString StrErr = SoundFileStrError(FileHandle);
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to seek file: %s"), *StrErr);
			return SetError(ESoundFileError::FAILED_TO_SEEK);
		}
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::WriteFrames(const float* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten)
	{
		OutNumFramesWritten = SoundFileWriteFramesFloat(FileHandle, DataPtr, NumFrames);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::WriteFrames(const double* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten)
	{
		OutNumFramesWritten = SoundFileWriteFramesDouble(FileHandle, DataPtr, NumFrames);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::WriteSamples(const float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten)
	{
		OutNumSampleWritten = SoundFileWriteSamplesFloat(FileHandle, DataPtr, NumSamples);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::WriteSamples(const double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten)
	{
		OutNumSampleWritten = SoundFileWriteSamplesDouble(FileHandle, DataPtr, NumSamples);
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::GetData(TArray<uint8>** OutBulkData)
	{
		*OutBulkData = &BulkData;
		return ESoundFileError::NONE;
	}

	ESoundFileError::Type FSoundFileWriter::SetError(ESoundFileError::Type InError)
	{
		if (InError != ESoundFileError::NONE)
		{
			State.Set(ESoundFileState::HAS_ERROR);
		}
		CurrentError.Set(InError);
		return InError;
	}

	/************************************************************************/
	/* FUnrealAudioModule													*/
	/************************************************************************/

	static void* GetSoundFileDllHandle()
	{
		void* DllHandle = nullptr;
#if PLATFORM_WINDOWS
		FString Path = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/libsndfile/Win64/"));
		FPlatformProcess::PushDllDirectory(*Path);
		DllHandle = FPlatformProcess::GetDllHandle(*(Path + "libsndfile-1.dll"));
		FPlatformProcess::PopDllDirectory(*Path);
#elif PLATFORM_MAC
		//		FString Path = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/libsndfile/Mac/"));
		//		FPlatformProcess::PushDllDirectory(*Path);
		DllHandle = FPlatformProcess::GetDllHandle(TEXT("libsndfile.1.dylib"));
		//		FPlatformProcess::PopDllDirectory(*Path);
#endif
		return DllHandle;
	}

	bool FUnrealAudioModule::LoadSoundFileLib()
	{
		SoundFileDllHandle = GetSoundFileDllHandle();
		if (!SoundFileDllHandle)
		{
			UE_LOG(LogUnrealAudio, Error, TEXT("Failed to load Sound File dll"));
			return false;
		}

		bool bSuccess = true;

		void* LambdaDLLHandle = SoundFileDllHandle;

		// Helper function to load DLL exports and report warnings
		auto GetSoundFileDllExport = [&LambdaDLLHandle](const TCHAR* FuncName, bool& bInSuccess) -> void*
		{
			if (bInSuccess)
			{
				void* FuncPtr = FPlatformProcess::GetDllExport(LambdaDLLHandle, FuncName);
				if (FuncPtr == nullptr)
				{
					bInSuccess = false;
					UE_LOG(LogUnrealAudio, Warning, TEXT("Failed to locate the expected DLL import function '%s' in the SoundFile DLL."), *FuncName);
					FPlatformProcess::FreeDllHandle(LambdaDLLHandle);
					LambdaDLLHandle = nullptr;
				}
				return FuncPtr;
			}
			else
			{
				return nullptr;
			}
		};

		SoundFileOpen = (SoundFileOpenFuncPtr)GetSoundFileDllExport(TEXT("sf_open"), bSuccess);
		SoundFileOpenVirtual = (SoundFileOpenVirtualFuncPtr)GetSoundFileDllExport(TEXT("sf_open_virtual"), bSuccess);
		SoundFileClose = (SoundFileCloseFuncPtr)GetSoundFileDllExport(TEXT("sf_close"), bSuccess);
		SoundFileError = (SoundFileErrorFuncPtr)GetSoundFileDllExport(TEXT("sf_error"), bSuccess);
		SoundFileStrError = (SoundFileStrErrorFuncPtr)GetSoundFileDllExport(TEXT("sf_strerror"), bSuccess);
		SoundFileErrorNumber = (SoundFileErrorNumberFuncPtr)GetSoundFileDllExport(TEXT("sf_error_number"), bSuccess);
		SoundFileCommand = (SoundFileCommandFuncPtr)GetSoundFileDllExport(TEXT("sf_command"), bSuccess);
		SoundFileFormatCheck = (SoundFileFormatCheckFuncPtr)GetSoundFileDllExport(TEXT("sf_format_check"), bSuccess);
		SoundFileSeek = (SoundFileSeekFuncPtr)GetSoundFileDllExport(TEXT("sf_seek"), bSuccess);
		SoundFileGetVersion = (SoundFileGetVersionFuncPtr)GetSoundFileDllExport(TEXT("sf_version_string"), bSuccess);
		SoundFileReadFramesFloat = (SoundFileReadFramesFloatFuncPtr)GetSoundFileDllExport(TEXT("sf_readf_float"), bSuccess);
		SoundFileReadFramesDouble = (SoundFileReadFramesDoubleFuncPtr)GetSoundFileDllExport(TEXT("sf_readf_double"), bSuccess);
		SoundFileWriteFramesFloat = (SoundFileWriteFramesFloatFuncPtr)GetSoundFileDllExport(TEXT("sf_writef_float"), bSuccess);
		SoundFileWriteFramesDouble = (SoundFileWriteFramesDoubleFuncPtr)GetSoundFileDllExport(TEXT("sf_writef_double"), bSuccess);
		SoundFileReadSamplesFloat = (SoundFileReadSamplesFloatFuncPtr)GetSoundFileDllExport(TEXT("sf_read_float"), bSuccess);
		SoundFileReadSamplesDouble = (SoundFileReadSamplesDoubleFuncPtr)GetSoundFileDllExport(TEXT("sf_read_double"), bSuccess);
		SoundFileWriteSamplesFloat = (SoundFileWriteSamplesFloatFuncPtr)GetSoundFileDllExport(TEXT("sf_write_float"), bSuccess);
		SoundFileWriteSamplesDouble = (SoundFileWriteSamplesDoubleFuncPtr)GetSoundFileDllExport(TEXT("sf_write_double"), bSuccess);

		// make sure we're successful
		check(bSuccess);
		return bSuccess;
	}

	bool FUnrealAudioModule::ShutdownSoundFileLib()
	{
		if (SoundFileDllHandle)
		{
			FPlatformProcess::FreeDllHandle(SoundFileDllHandle);
			SoundFileDllHandle = nullptr;
			SoundFileOpen = nullptr;
			SoundFileOpenVirtual = nullptr;
			SoundFileClose = nullptr;
			SoundFileError = nullptr;
			SoundFileStrError = nullptr;
			SoundFileErrorNumber = nullptr;
			SoundFileCommand = nullptr;
			SoundFileFormatCheck = nullptr;
			SoundFileSeek = nullptr;
			SoundFileGetVersion = nullptr;
			SoundFileReadFramesFloat = nullptr;
			SoundFileReadFramesDouble = nullptr;
			SoundFileWriteFramesFloat = nullptr;
			SoundFileWriteFramesDouble = nullptr;
			SoundFileReadSamplesFloat = nullptr;
			SoundFileReadSamplesDouble = nullptr;
			SoundFileWriteSamplesFloat = nullptr;
			SoundFileWriteSamplesDouble = nullptr;
		}
		return true;
	}

	TSharedPtr<FSoundFileReader> FUnrealAudioModule::CreateSoundFileReader()
	{
		return TSharedPtr<FSoundFileReader>(new FSoundFileReader(this));
	}

	TSharedPtr<FSoundFileWriter> FUnrealAudioModule::CreateSoundFileWriter()
	{
		return TSharedPtr<FSoundFileWriter>(new FSoundFileWriter(this));
	}

	/************************************************************************/
	/* Exported Functions													*/
	/************************************************************************/

	bool GetListOfSoundFilesInDirectory(const FString& Directory, TArray<FString>& OutSoundFileList, FString* TypeFilter /*= nullptr */)
	{
		return false;
	}

	bool GetSoundFileDescription(const FString& FilePath, FSoundFileDescription& OutputDescription, TArray<ESoundFileChannelMap::Type>& OutChannelMap)
	{
		LibSoundFileHandle* FileHandle = nullptr;
		ESoundFileError::Type Error = GetSoundDesriptionInternal(&FileHandle, FilePath, OutputDescription, OutChannelMap);
		if (Error == ESoundFileError::NONE)
		{
			DEBUG_AUDIO_CHECK(FileHandle != nullptr);
			SoundFileClose(FileHandle);
			return true;
		}
		return false;
	}

	bool GetSoundFileDescription(const FString& FilePath, FSoundFileDescription& OutputDescription)
	{
		TArray<ESoundFileChannelMap::Type> OutChannelMap;
		return GetSoundFileDescription(FilePath, OutputDescription, OutChannelMap);
	}

	bool GetFileExtensionForFormatFlags(int32 FormatFlags, FString& OutExtension)
	{
		if (FormatFlags & ESoundFileFormat::OGG)
		{
			OutExtension = TEXT("ogg");
		}
		else if (FormatFlags & ESoundFileFormat::WAV)
		{
			OutExtension = TEXT("wav");
		}
		else if (FormatFlags & ESoundFileFormat::AIFF)
		{
			OutExtension = TEXT("aiff");
		}
		else if (FormatFlags & ESoundFileFormat::FLAC)
		{
			OutExtension = TEXT("flac");
		}
		else
		{
			return false;
		}

		return true;
	}

	ESoundFileError::Type GetSoundFileInfoFromPath(const FString& FilePath, FSoundFileDescription& Description, TArray<ESoundFileChannelMap::Type>& ChannelMap)
	{
		// Load the description and channel map info
		LibSoundFileHandle* FileHandle = nullptr;
		ESoundFileError::Type Error = GetSoundDesriptionInternal(&FileHandle, FilePath, Description, ChannelMap);
		if (FileHandle)
		{
			SoundFileClose(FileHandle);
		}
		return Error;
	}

	ESoundFileError::Type LoadSoundFileFromPath(const FString& FilePath, FSoundFileDescription& Description, TArray<ESoundFileChannelMap::Type>& ChannelMap, TArray<uint8>& BulkData)
	{
		ESoundFileError::Type Error = GetSoundFileInfoFromPath(FilePath, Description, ChannelMap);
		if (Error != ESoundFileError::NONE)
		{
			return Error;
		}

		// Now read the data from disk into the bulk data array
		if (FFileHelper::LoadFileToArray(BulkData, *FilePath))
		{
			return ESoundFileError::NONE;
		}
		else
		{
			return ESoundFileError::FAILED_TO_LOAD_BYTE_DATA;
		}
	}



}

#endif // #if ENABLE_UNREAL_AUDIO
