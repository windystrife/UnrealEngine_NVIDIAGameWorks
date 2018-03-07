// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "Engine/EngineTypes.h"
#if PLATFORM_WINDOWS
	#include "WindowsHWrapper.h"
#endif
THIRD_PARTY_INCLUDES_START
	#include "RtAudio.h"
THIRD_PARTY_INCLUDES_END

class USoundWave;

DECLARE_LOG_CATEGORY_EXTERN(LogMicManager, Log, All);

/**
 * FAudioRecordingManager
 * Singleton Mic Recording Manager -- generates recordings, stores the recorded data and plays them back
 */
class FAudioRecordingManager
{
public:
	// Retrieves the singleton recording manager
	static FAudioRecordingManager& Get();

	// Starts a new recording with the given name and optional duration. 
	// If set to -1.0f, a duration won't be used and the recording length will be determined by StopRecording().
	USoundWave* StartRecording(const FDirectoryPath& Directory, const FString& AssetName, float RecordingDurationSec, float GainDB, int32 InputBufferSize);

	// Stops recording if the recording manager is recording. If not recording but has recorded data (due to set duration), it will just return the generated USoundWave.
	USoundWave* StopRecording();

	// Called by RtAudio when a new audio buffer is ready to be supplied.
	int32 OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow);

private:

	// Private Constructor
	FAudioRecordingManager();

	// Private Destructor
	~FAudioRecordingManager();

	// Saves raw PCM data recorded to a wave file format
	void SerializeWaveFile(TArray<uint8>& OutWaveFileData, const uint8* InPCMData, const int32 NumBytes);

	// The default mic sample rate
	const int32 WAVE_FILE_SAMPLERATE = 44100;

	// RtAudio ADC object -- used to interact with low-level audio device.
	RtAudio ADC;

	// Stream parameters to initialize the ADC
	RtAudio::StreamParameters StreamParams;

	// Critical section used to stop and retrieve finished audio buffers.
	FCriticalSection CriticalSection;

	// The current recording name that is actively recording. Empty string if nothing is currently recording.
	FString CurrentRecordingName;

	// Where to store the current recording.
	FDirectoryPath CurrentRecordingDirectory;

	// The data which is currently being recorded to, if the manager is actively recording. This is not safe to access while recording.
	TArray<int16> CurrentRecordedPCMData;

	// Buffer to store sample rate converted PCM data
	TArray<int16> ConvertedPCMData;

	// Reusable raw wave data buffer to generate .wav file formats
	TArray<uint8> RawWaveData;

	// The number of frames that have been recorded
	int32 NumRecordedSamples;

	// The number of frames to record if recording a set duration
	int32 NumFramesToRecord;

	// Recording block size (number of frames per callback block)
	int32 RecordingBlockSize;

	// The sample rate used in the recording
	float RecordingSampleRate;

	// Num input channels
	int32 NumInputChannels;

	// A linear gain to apply on mic input
	float InputGain;

	// Whether or not the manager is actively recording.
	FThreadSafeBool bRecording;

	// Number of overflows detected while recording
	int32 NumOverflowsDetected;

	// Whether or not we have an error
	uint32 bError : 1;

};
