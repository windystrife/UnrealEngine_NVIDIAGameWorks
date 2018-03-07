// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioRecordingManager.h"
#include "AudioCapture.h"
#include "Sound/AudioSettings.h"
#include "AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "Misc/ScopeLock.h"
#include "Sound/SoundWave.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Components/AudioComponent.h"

DEFINE_LOG_CATEGORY(LogMicManager);

/**
* Callback Function For the Microphone Capture for RtAudio
*/
static int32 OnAudioCaptureCallback(void *OutBuffer, void* InBuffer, uint32 InBufferFrames, double StreamTime, RtAudioStreamStatus AudioStreamStatus, void* InUserData)
{
	// Check for stream overflow (i.e. we're feeding audio faster than we're consuming)


	// Cast the user data to the singleton mic recorder
	FAudioRecordingManager* AudioRecordingManager = (FAudioRecordingManager*)InUserData;

	// Call the mic capture callback function
	return AudioRecordingManager->OnAudioCapture(InBuffer, InBufferFrames, StreamTime, AudioStreamStatus == RTAUDIO_INPUT_OVERFLOW);
}

/**
* FAudioRecordingManager Implementation
*/

FAudioRecordingManager::FAudioRecordingManager()
	: NumRecordedSamples(0)
	, NumFramesToRecord(0)
	, RecordingBlockSize(0)
	, RecordingSampleRate(44100.0f)
	, NumInputChannels(1)
	, InputGain(0.0f)
	, bRecording(false)
	, bError(false)
{
}

FAudioRecordingManager::~FAudioRecordingManager()
{
}

FAudioRecordingManager& FAudioRecordingManager::Get()
{
	// Static isngleton instance
	static FAudioRecordingManager MicInputManager;
	return MicInputManager;
}

USoundWave* FAudioRecordingManager::StartRecording(const FDirectoryPath& Directory, const FString& AssetName, float RecordingDurationSec, float GainDB, int32 InputBufferSize)
{
	if (bError)
	{
		return nullptr;
	}

	// Stop any recordings currently going on (if there is one)
	USoundWave* NewSoundWave = StopRecording();

	RecordingBlockSize = InputBufferSize;

	// If we have a stream open close it (reusing streams can cause a blip of previous recordings audio)
	if (ADC.isStreamOpen())
	{
		try
		{
			ADC.stopStream();
			ADC.closeStream();
		}
		catch (RtAudioError& e)
		{
			FString ErrorMessage = FString(e.what());
			bError = true;
			UE_LOG(LogMicManager, Error, TEXT("Failed to close the mic capture device stream: %s"), *ErrorMessage);
			return nullptr;
		}
	}

	UE_LOG(LogMicManager, Log, TEXT("Starting mic recording."));

	// Convert input gain decibels into linear scale
	if (GainDB != 0.0f)
	{
		InputGain = FMath::Pow(10.0f, GainDB / 20.0f);
	}
	else
	{
		InputGain = 0.0f;
	}

	RtAudio::DeviceInfo Info = ADC.getDeviceInfo(StreamParams.deviceId);
	RecordingSampleRate = Info.preferredSampleRate;
	NumInputChannels = Info.inputChannels;

	// Reserve enough space in our current recording buffer for 10 seconds of audio to prevent slowing down due to allocations
	if (RecordingDurationSec != -1.0f)
	{
		int32 NumSamplesToReserve = RecordingDurationSec * RecordingSampleRate * NumInputChannels;
		CurrentRecordedPCMData.Reset(NumSamplesToReserve);
	}
	else
	{
		// if not given a duration, resize array to 60 seconds of audio anyway
		int32 NumSamplesToReserve = 60 * RecordingSampleRate * NumInputChannels;
		CurrentRecordedPCMData.Reset(NumSamplesToReserve);
	}

	CurrentRecordingName = AssetName;
	CurrentRecordingDirectory = Directory;

	NumRecordedSamples = 0;
	NumOverflowsDetected = 0;

	if (RecordingDurationSec > 0.0f)
	{
		NumFramesToRecord = RecordingSampleRate * RecordingDurationSec;
	}

	// Publish to the mic input thread that we're ready to record...
	bRecording = true;

	StreamParams.deviceId = ADC.getDefaultInputDevice(); // Only use the default input device for now

	StreamParams.nChannels = NumInputChannels;
	StreamParams.firstChannel = 0;

	uint32 BufferFrames = FMath::Max(RecordingBlockSize, 256);

	UE_LOG(LogMicManager, Log, TEXT("Initialized mic recording manager at %d hz sample rate, %d channels, and %d Recording Block Size"), RecordingSampleRate, StreamParams.nChannels, BufferFrames);

	// RtAudio uses exceptions for error handling... 
	try
	{
		// Open up new audio stream
		ADC.openStream(nullptr, &StreamParams, RTAUDIO_SINT16, RecordingSampleRate, &BufferFrames, &OnAudioCaptureCallback, this);
	}
	catch (RtAudioError& e)
	{
		FString ErrorMessage = FString(e.what());
		bError = true;
		UE_LOG(LogMicManager, Error, TEXT("Failed to open the mic capture device: %s"), *ErrorMessage);
	}
	catch (const std::exception& e)
	{
		bError = true;
		FString ErrorMessage = FString(e.what());
		UE_LOG(LogMicManager, Error, TEXT("Failed to open the mic capture device: %s"), *ErrorMessage);
	}
	catch (...)
	{
		UE_LOG(LogMicManager, Error, TEXT("Failed to open the mic capture device: unknown error"));
		bError = true;
	}

	ADC.startStream();

	return NewSoundWave;
}

static void WriteUInt32ToByteArrayLE(TArray<uint8>& InByteArray, int32& Index, const uint32 Value)
{
	InByteArray[Index++] = (uint8)(Value >> 0);
	InByteArray[Index++] = (uint8)(Value >> 8);
	InByteArray[Index++] = (uint8)(Value >> 16);
	InByteArray[Index++] = (uint8)(Value >> 24);
}

static void WriteUInt16ToByteArrayLE(TArray<uint8>& InByteArray, int32& Index, const uint16 Value)
{
	InByteArray[Index++] = (uint8)(Value >> 0);
	InByteArray[Index++] = (uint8)(Value >> 8);
}

void FAudioRecordingManager::SerializeWaveFile(TArray<uint8>& OutWaveFileData, const uint8* InPCMData, const int32 NumBytes)
{
	// Reserve space for the raw wave data
	OutWaveFileData.Empty(NumBytes + 44);
	OutWaveFileData.AddZeroed(NumBytes + 44);

	int32 WaveDataByteIndex = 0;

	// Wave Format Serialization ----------

	// FieldName: ChunkID
	// FieldSize: 4 bytes
	// FieldValue: RIFF (FourCC value, big-endian)
	OutWaveFileData[WaveDataByteIndex++] = 'R';
	OutWaveFileData[WaveDataByteIndex++] = 'I';
	OutWaveFileData[WaveDataByteIndex++] = 'F';
	OutWaveFileData[WaveDataByteIndex++] = 'F';

	// ChunkName: ChunkSize: 4 bytes 
	// Value: NumBytes + 36. Size of the rest of the chunk following this number. Size of entire file minus 8 bytes.
	WriteUInt32ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, NumBytes + 36);

	// FieldName: Format 
	// FieldSize: 4 bytes
	// FieldValue: "WAVE"  (big-endian)
	OutWaveFileData[WaveDataByteIndex++] = 'W';
	OutWaveFileData[WaveDataByteIndex++] = 'A';
	OutWaveFileData[WaveDataByteIndex++] = 'V';
	OutWaveFileData[WaveDataByteIndex++] = 'E';

	// FieldName: Subchunk1ID
	// FieldSize: 4 bytes
	// FieldValue: "fmt "
	OutWaveFileData[WaveDataByteIndex++] = 'f';
	OutWaveFileData[WaveDataByteIndex++] = 'm';
	OutWaveFileData[WaveDataByteIndex++] = 't';
	OutWaveFileData[WaveDataByteIndex++] = ' ';

	// FieldName: Subchunk1Size
	// FieldSize: 4 bytes
	// FieldValue: 16 for PCM
	WriteUInt32ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, 16);

	// FieldName: AudioFormat
	// FieldSize: 2 bytes
	// FieldValue: 1 for PCM
	WriteUInt16ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, 1);

	// FieldName: NumChannels
	// FieldSize: 2 bytes
	// FieldValue: 1 for for mono
	WriteUInt16ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, NumInputChannels);

	// FieldName: SampleRate
	// FieldSize: 4 bytes
	// FieldValue: MIC_SAMPLE_RATE
	WriteUInt32ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, WAVE_FILE_SAMPLERATE);

	// FieldName: ByteRate
	// FieldSize: 4 bytes
	// FieldValue: SampleRate * NumChannels * BitsPerSample/8
	int32 ByteRate = WAVE_FILE_SAMPLERATE * NumInputChannels * 2;
	WriteUInt32ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, ByteRate);

	// FieldName: BlockAlign
	// FieldSize: 2 bytes
	// FieldValue: NumChannels * BitsPerSample/8
	int32 BlockAlign = 2;
	WriteUInt16ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, BlockAlign);

	// FieldName: BitsPerSample
	// FieldSize: 2 bytes
	// FieldValue: 16 (16 bits per sample)
	WriteUInt16ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, 16);

	// FieldName: Subchunk2ID
	// FieldSize: 4 bytes
	// FieldValue: "data" (big endian)

	OutWaveFileData[WaveDataByteIndex++] = 'd';
	OutWaveFileData[WaveDataByteIndex++] = 'a';
	OutWaveFileData[WaveDataByteIndex++] = 't';
	OutWaveFileData[WaveDataByteIndex++] = 'a';

	// FieldName: Subchunk2Size
	// FieldSize: 4 bytes
	// FieldValue: number of bytes of the data
	WriteUInt32ToByteArrayLE(OutWaveFileData, WaveDataByteIndex, NumBytes);

	// Copy the raw PCM data to the audio file
	FMemory::Memcpy(&OutWaveFileData[WaveDataByteIndex], InPCMData, NumBytes);
}

static void SampleRateConvert(float CurrentSR, float TargetSR, int32 NumChannels, const TArray<int16>& CurrentRecordedPCMData, int32 NumSamplesToConvert, TArray<int16>& OutConverted)
{
	int32 NumInputSamples = CurrentRecordedPCMData.Num();
	int32 NumOutputSamples = NumInputSamples * TargetSR / CurrentSR;

	OutConverted.Reset(NumOutputSamples);

	float SrFactor = (double)CurrentSR / TargetSR;
	float CurrentFrameIndexInterpolated = 0.0f;
	check(NumSamplesToConvert <= CurrentRecordedPCMData.Num());
	int32 NumFramesToConvert = NumSamplesToConvert / NumChannels;
	int32 CurrentFrameIndex = 0;

	for (;;)
	{
		int32 NextFrameIndex = CurrentFrameIndex + 1;
		if (CurrentFrameIndex >= NumFramesToConvert || NextFrameIndex >= NumFramesToConvert)
		{
			break;
		}

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			int32 CurrentSampleIndex = CurrentFrameIndex * NumChannels + Channel;
			int32 NextSampleIndex = CurrentSampleIndex + NumChannels;

			int16 CurrentSampleValue = CurrentRecordedPCMData[CurrentSampleIndex];
			int16 NextSampleValue = CurrentRecordedPCMData[NextSampleIndex];

			int16 NewSampleValue = FMath::Lerp(CurrentSampleValue, NextSampleValue, CurrentFrameIndexInterpolated);

			OutConverted.Add(NewSampleValue);
		}

		CurrentFrameIndexInterpolated += SrFactor;

		// Wrap the interpolated frame between 0.0 and 1.0 to maintain float precision
		while (CurrentFrameIndexInterpolated >= 1.0f)
		{
			CurrentFrameIndexInterpolated -= 1.0f;
			
			// Every time it wraps, we increment the frame index
			++CurrentFrameIndex;
		}
	}
}

USoundWave* FAudioRecordingManager::StopRecording()
{
	FScopeLock Lock(&CriticalSection);

	// If we're currently recording, stop the recording
	if (bRecording)
	{
		bRecording = false;

		if (CurrentRecordedPCMData.Num() > 0)
		{
			NumRecordedSamples = FMath::Min(NumFramesToRecord * NumInputChannels, CurrentRecordedPCMData.Num());

			UE_LOG(LogMicManager, Log, TEXT("Stopping mic recording. Recorded %d frames of audio (%.4f seconds). Detected %d buffer overflows."), 
				NumRecordedSamples, 
				(float)NumRecordedSamples / RecordingSampleRate,
				NumOverflowsDetected);

			// Get a ptr to the buffer we're actually going to serialize
			TArray<int16>* PCMDataToSerialize = nullptr;

			// If our sample rate isn't 44100, then we need to do a SRC
			if (RecordingSampleRate != 44100.0f)
			{
				UE_LOG(LogMicManager, Log, TEXT("Converting sample rate from %d hz to 44100 hz."), (int32)RecordingSampleRate);

				SampleRateConvert(RecordingSampleRate, (float)WAVE_FILE_SAMPLERATE, NumInputChannels, CurrentRecordedPCMData, NumRecordedSamples, ConvertedPCMData);

				// Update the recorded samples to the converted buffer samples
				NumRecordedSamples = ConvertedPCMData.Num();
				PCMDataToSerialize = &ConvertedPCMData;
			}
			else
			{
				// Just use the original recorded buffer to serialize
				PCMDataToSerialize = &CurrentRecordedPCMData;
			}

			// Scale by the linear gain if it's been set to something (0.0f is ctor default and impossible to set by dB)
			if (InputGain != 0.0f)
			{
				UE_LOG(LogMicManager, Log, TEXT("Scaling gain of recording by %.2f linear gain."), InputGain);

				for (int32 i = 0; i < NumRecordedSamples; ++i)
				{
					// Scale by input gain, clamp to prevent integer overflow when casting back to int16. Will still clip.
					(*PCMDataToSerialize)[i] = (int16)FMath::Clamp(InputGain * (float)(*PCMDataToSerialize)[i], -32767.0f, 32767.0f);
				}
			}

			// Get the raw data
			const uint8* RawData = (const uint8*)PCMDataToSerialize->GetData();
			int32 NumBytes = NumRecordedSamples * sizeof(int16);

			USoundWave* NewSoundWave = nullptr;
			USoundWave* ExistingSoundWave = nullptr;
			TArray<UAudioComponent*> ComponentsToRestart;
			bool bCreatedPackage = false;

			if (CurrentRecordingDirectory.Path.IsEmpty() || CurrentRecordingName.IsEmpty())
			{
				// Create a new sound wave object from the transient package
				NewSoundWave = NewObject<USoundWave>(GetTransientPackage(), *CurrentRecordingName);
			}
			else
			{
				// Create a new package
				FString PackageName = CurrentRecordingDirectory.Path / CurrentRecordingName;
				UPackage* Package = CreatePackage(nullptr, *PackageName);

				//FEditorDelegates::OnAssetPreImport.Broadcast(this, USoundWave::StaticClass(), Package, *CurrentRecordingName, TEXT("wav"));

				// Create a raw .wav file to stuff the raw PCM data in so when we create the sound wave asset it's identical to a normal imported asset
				SerializeWaveFile(RawWaveData, RawData, NumBytes);

				// Check to see if a sound wave already exists at this location
				ExistingSoundWave = FindObject<USoundWave>(Package, *CurrentRecordingName);

				// See if it's currently being played right now
				FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
				if (AudioDeviceManager && ExistingSoundWave)
				{
					AudioDeviceManager->StopSoundsUsingResource(ExistingSoundWave, &ComponentsToRestart);
				}

				// Create a new sound wave object
				if (ExistingSoundWave)
				{
					NewSoundWave = ExistingSoundWave;
					NewSoundWave->FreeResources();
				}
				else
				{
					NewSoundWave = NewObject<USoundWave>(Package, *CurrentRecordingName, RF_Public | RF_Standalone);
				}

				// Compressed data is now out of date.
				NewSoundWave->InvalidateCompressedData();

				// Copy the raw wave data file to the sound wave for storage. Will allow the recording to be exported.
				NewSoundWave->RawData.Lock(LOCK_READ_WRITE);
				void* LockedData = NewSoundWave->RawData.Realloc(RawWaveData.Num());
				FMemory::Memcpy(LockedData, RawWaveData.GetData(), RawWaveData.Num());
				NewSoundWave->RawData.Unlock();

				bCreatedPackage = true;
			}

			if (NewSoundWave)
			{
				// Copy the recorded data to the sound wave so we can quickly preview it
				NewSoundWave->RawPCMDataSize = NumBytes;
				NewSoundWave->RawPCMData = (uint8*)FMemory::Malloc(NewSoundWave->RawPCMDataSize);
				FMemory::Memcpy(NewSoundWave->RawPCMData, RawData, NumBytes);

				// Calculate the duration of the sound wave
				NewSoundWave->Duration = (float)(NumRecordedSamples / NumInputChannels) / WAVE_FILE_SAMPLERATE;
				NewSoundWave->SampleRate = WAVE_FILE_SAMPLERATE;
				NewSoundWave->NumChannels = NumInputChannels;

				if (bCreatedPackage)
				{
					//FEditorDelegates::OnAssetPostImport.Broadcast(this, NewSoundWave);

					FAssetRegistryModule::AssetCreated(NewSoundWave);
					NewSoundWave->MarkPackageDirty();

					// Restart any audio components if they need to be restarted
					for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToRestart.Num(); ++ComponentIndex)
					{
						ComponentsToRestart[ComponentIndex]->Play();
					}
				}
			}

			return NewSoundWave;
		}
	}


	return nullptr;
}

int32 FAudioRecordingManager::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	FScopeLock Lock(&CriticalSection);

	if (bRecording)
	{
		if (bOverflow) 
			++NumOverflowsDetected;

		CurrentRecordedPCMData.Append((int16*)InBuffer, InBufferFrames * NumInputChannels);
		return 0;
	}

	return 1;
}