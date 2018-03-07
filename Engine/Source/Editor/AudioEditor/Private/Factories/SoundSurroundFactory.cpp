// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundSurroundFactory.h"
#include "PackageTools.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"

USoundSurroundFactory::USoundSurroundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("WAV;Multichannel Sound"));

	bCreateNew = false;
	CueVolume = 0.75f;
	bEditorImport = true;
}

bool USoundSurroundFactory::FactoryCanImport(const FString& Filename)
{
	// Find the root name
	FString RootName = FPaths::GetBaseFilename(Filename);
	FString SpeakerLocation = RootName.Right(3).ToLower();

	// Find which channel this refers to		
	for (int32 SpeakerIndex = 0; SpeakerIndex < SPEAKER_Count; SpeakerIndex++)
	{
		if (SpeakerLocation == SurroundSpeakerLocations[SpeakerIndex])
		{
			return(true);
		}
	}

	return(false);
}

UObject* USoundSurroundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
	)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, FileType);

	int32		SpeakerIndex, i;

	// Only import wavs
	if (FCString::Stricmp(FileType, TEXT("WAV")) == 0)
	{
		// Find the root name
		FString RootName = Name.GetPlainNameString();
		FString SpeakerLocation = RootName.Right(3).ToLower();
		FName BaseName = FName(*RootName.LeftChop(3));

		// Find which channel this refers to		
		for (SpeakerIndex = 0; SpeakerIndex < SPEAKER_Count; SpeakerIndex++)
		{
			if (SpeakerLocation == SurroundSpeakerLocations[SpeakerIndex])
			{
				break;
			}
		}

		if (SpeakerIndex == SPEAKER_Count)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Failed to find speaker location; valid extensions are _fl, _fr, _fc, _lf, _sl, _sr, _bl, _br."));
			FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
			return(nullptr);
		}

		// Find existing soundwave
		USoundWave* Sound = FindObject<USoundWave>(InParent, *BaseName.ToString());

		// Create new sound if necessary
		if (Sound == nullptr)
		{
			// If This is a single asset package, then create package so that its name will be identical to the asset.
			if (PackageTools::IsSingleAssetPackage(InParent->GetName()))
			{
				InParent = CreatePackage(nullptr, *InParent->GetName().LeftChop(3));

				// Make sure the destination package is loaded
				CastChecked<UPackage>(InParent)->FullyLoad();

				Sound = FindObject<USoundWave>(InParent, *BaseName.ToString());
			}

			if (Sound == nullptr)
			{
				Sound = NewObject<USoundWave>(InParent, BaseName, Flags);
			}
		}

		// Clear resources so that if it's already been played, it will reload the wave data
		Sound->FreeResources();

		// Presize the offsets array, in case the sound was new or the original sound data was stripped by cooking.
		if (Sound->ChannelOffsets.Num() != SPEAKER_Count)
		{
			Sound->ChannelOffsets.Empty(SPEAKER_Count);
			Sound->ChannelOffsets.AddZeroed(SPEAKER_Count);
		}
		// Presize the sizes array, in case the sound was new or the original sound data was stripped by cooking.
		if (Sound->ChannelSizes.Num() != SPEAKER_Count)
		{
			Sound->ChannelSizes.Empty(SPEAKER_Count);
			Sound->ChannelSizes.AddZeroed(SPEAKER_Count);
		}

		// Store the current file path and timestamp for re-import purposes
		Sound->AssetImportData->Update(CurrentFilename);

		// Compressed data is now out of date.
		Sound->InvalidateCompressedData();

		// Delete the old version of the wave from the bulk data
		uint8* RawWaveData[SPEAKER_Count] = { nullptr };
		uint8* RawData = (uint8*)Sound->RawData.Lock(LOCK_READ_WRITE);
		int32 RawDataOffset = 0;
		int32 TotalSize = 0;

		// Copy off the still used waves
		for (i = 0; i < SPEAKER_Count; i++)
		{
			if (i != SpeakerIndex && Sound->ChannelSizes[i])
			{
				RawWaveData[i] = new uint8[Sound->ChannelSizes[i]];
				FMemory::Memcpy(RawWaveData[i], RawData + Sound->ChannelOffsets[i], Sound->ChannelSizes[i]);
				TotalSize += Sound->ChannelSizes[i];
			}
		}

		// Copy them back without the one that will be updated
		RawData = (uint8*)Sound->RawData.Realloc(TotalSize);

		for (i = 0; i < SPEAKER_Count; i++)
		{
			if (RawWaveData[i])
			{
				FMemory::Memcpy(RawData + RawDataOffset, RawWaveData[i], Sound->ChannelSizes[i]);
				Sound->ChannelOffsets[i] = RawDataOffset;
				RawDataOffset += Sound->ChannelSizes[i];

				delete[] RawWaveData[i];
			}
		}

		uint32 RawDataSize = BufferEnd - Buffer;
		uint8* LockedData = (uint8*)Sound->RawData.Realloc(RawDataOffset + RawDataSize);
		LockedData += RawDataOffset;
		FMemory::Memcpy(LockedData, Buffer, RawDataSize);

		Sound->ChannelOffsets[SpeakerIndex] = RawDataOffset;
		Sound->ChannelSizes[SpeakerIndex] = RawDataSize;

		Sound->RawData.Unlock();

		// Calculate duration.
		FWaveModInfo WaveInfo;
		FString ErrorReason;
		if (WaveInfo.ReadWaveInfo(LockedData, RawDataSize, &ErrorReason))
		{
			// Calculate duration in seconds
			int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;
			if (DurationDiv)
			{
				Sound->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
			}
			else
			{
				Sound->Duration = 0.0f;
			}

			if (*WaveInfo.pBitsPerSample != 16)
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("Currently, only 16 bit WAV files are supported (%s)."), *Name.ToString());
				Sound->MarkPendingKill();
				Sound = nullptr;
			}

			if (*WaveInfo.pChannels != 1)
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("Currently, only mono WAV files can be imported as channels of surround audio (%s)."), *Name.ToString());
				Sound->MarkPendingKill();
				Sound = nullptr;
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Unable to read wave file '%s' - \"%s\""), *Name.ToString(), *ErrorReason);
			Sound->MarkPendingKill();
			Sound = nullptr;
		}
		if (Sound)
		{
			Sound->NumChannels = 0;
			for (i = 0; i < SPEAKER_Count; i++)
			{
				if (Sound->ChannelSizes[i])
				{
					Sound->NumChannels++;
				}
			}
		}

		FEditorDelegates::OnAssetPostImport.Broadcast(this, Sound);

		return(Sound);
	}
	else
	{
		// Unrecognized.
		Warn->Logf(ELogVerbosity::Error, TEXT("Unrecognized sound extension '%s' in %s"), FileType, *Name.ToString());
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
	}

	return(nullptr);
}

