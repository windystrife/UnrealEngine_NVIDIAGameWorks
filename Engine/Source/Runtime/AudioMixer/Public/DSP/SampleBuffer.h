// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"

namespace Audio
{
	// An object which fully loads/decodes a sound wave and allows direct access to sound wave data.	
	class AUDIOMIXER_API FSampleBuffer
	{
	public:
		FSampleBuffer();
		~FSampleBuffer();

		// Gets the raw PCM data of the sound wave
		const int16* GetData() const;

		// Gets the number of samples of the sound wave
		int32 GetNumSamples() const;

		// Gets the number of frames of the sound wave
		int32 GetNumFrames() const;

		// Gets the number of channels of the sound wave
		int32 GetNumChannels() const;

		// Gets the sample rate of the sound wave
		int32 GetSampleRate() const;

		// Ptr to raw PCM data buffer
		int16* RawPCMData;
		// The number of samples in the buffer
		int32 NumSamples;
		// The number of frames in the buffer
		int32 NumFrames;
		// The number of channels in the buffer
		int32 NumChannels;
		// The sample rate of the buffer	
		int32 SampleRate;
		// The duration of the buffer in seconds
		float SampleDuration;
	};

	// Class which handles loading and decoding a USoundWave asset into a PCM buffer
	class AUDIOMIXER_API FSoundWavePCMLoader
	{
	public:
		FSoundWavePCMLoader();

		// Intialize loader with audio device
		void Init(FAudioDevice* InAudioDevice);

		// Loads a USoundWave, call on game thread.
		void LoadSoundWave(USoundWave* InSoundWave);

		// Update the loading state. Returns true once the sound wave is loaded/decoded.
		// Call on game thread.
		bool Update();

		// Returns the sample buffer data once the sound wave is loaded/decoded. Call on game thread thread.
		void GetSampleBuffer(FSampleBuffer& OutSampleBuffer);

		// Empties pending sound wave load references. Call on audio rendering thread.
		void Reset();

		// Queries whether the current sound wave has finished loading/decoding
		bool IsSoundWaveLoaded() const { return bIsLoaded; }

	private:
		
		// Ptr to the audio device to use to do the decoding
		FAudioDevice* AudioDevice;
		// Reference to current loading sound wave
		USoundWave* SoundWave;	
		// Struct to meta-data of decoded PCM buffer and ptr to PCM data
		FSampleBuffer SampleBuffer;
		// Queue of sound wave ptrs to hold references to them until fully released in audio render thread
		TQueue<USoundWave*> PendingStoppingSoundWaves;
		// Whether the sound wave load/decode is in-flight
		bool bIsLoading;
		// Whether or not the sound wave has already been loaded
		bool bIsLoaded;
	};
}

