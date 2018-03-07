// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/SampleBuffer.h"
#include "AudioMixer.h"

namespace Audio
{
	FSampleBuffer::FSampleBuffer()
		: RawPCMData(nullptr)
		, NumSamples(0)	
		, NumFrames(0)
		, NumChannels(0)
		, SampleRate(0)
		, SampleDuration(0.0f)
	{
	}

	FSampleBuffer::~FSampleBuffer()
	{
	}

	const int16* FSampleBuffer::GetData() const
	{
		return RawPCMData;
	}

	int32 FSampleBuffer::GetNumSamples() const
	{
		return NumSamples;
	}
	
	int32 FSampleBuffer::GetNumFrames() const
	{
		return NumFrames;
	}

	int32 FSampleBuffer::GetNumChannels() const
	{
		return NumChannels;
	}

	int32 FSampleBuffer::GetSampleRate() const
	{
		return SampleRate;
	}

	FSoundWavePCMLoader::FSoundWavePCMLoader()
		: AudioDevice(nullptr)
		, SoundWave(nullptr)
		, bIsLoading(false)
		, bIsLoaded(false)
	{
	}

	void FSoundWavePCMLoader::Init(FAudioDevice* InAudioDevice)
	{
		AudioDevice = InAudioDevice;
	}

	void FSoundWavePCMLoader::LoadSoundWave(USoundWave* InSoundWave)
	{
		if (!AudioDevice || !InSoundWave)
		{
			return;
		}

		// Queue existing sound wave reference so it can be
		// cleared when the audio thread gets newly loaded audio data. 
		// Don't want to kill the sound wave PCM data while it's playing on audio thread.
		if (SoundWave)
		{
			PendingStoppingSoundWaves.Enqueue(SoundWave);
		}

		SoundWave = InSoundWave;
		if (!SoundWave->RawPCMData || SoundWave->AudioDecompressor)
		{
			bIsLoaded = false;
			bIsLoading = true;

			if (!SoundWave->RawPCMData)
			{
				// Kick off a decompression/precache of the sound wave
				AudioDevice->Precache(SoundWave, false, true, true);
			}
		}
		else
		{
			bIsLoading = true;
			bIsLoaded = true;
		}
	}

	bool FSoundWavePCMLoader::Update()
	{
		if (bIsLoading)
		{
			check(SoundWave);
	
			if (bIsLoaded || (SoundWave->AudioDecompressor && SoundWave->AudioDecompressor->IsDone()))
			{
				if (SoundWave->AudioDecompressor)
				{
					check(!bIsLoaded);
					delete SoundWave->AudioDecompressor;
					SoundWave->AudioDecompressor = nullptr;
				}
				bIsLoading = false;
				bIsLoaded = true;

				SampleBuffer.RawPCMData = (int16*)SoundWave->RawPCMData;
				SampleBuffer.NumSamples = SoundWave->RawPCMDataSize / sizeof(int16);
				SampleBuffer.NumChannels = SoundWave->NumChannels;
				SampleBuffer.NumFrames = SampleBuffer.NumSamples / SoundWave->NumChannels;
				SampleBuffer.SampleRate = SoundWave->SampleRate;
				SampleBuffer.SampleDuration = (float)SampleBuffer.NumFrames / SampleBuffer.SampleRate;

				return true;
			}
		}

		return false;
	}

	void FSoundWavePCMLoader::GetSampleBuffer(FSampleBuffer& OutSampleBuffer)
	{
		OutSampleBuffer = SampleBuffer;
	}

	void FSoundWavePCMLoader::Reset()
	{
		PendingStoppingSoundWaves.Empty();
	}
}


