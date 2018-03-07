// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundWaveProcedural.h"

USoundWaveProcedural::USoundWaveProcedural(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bReset = false;
	NumBufferUnderrunSamples = 512;
	NumSamplesToGeneratePerCallback = 1024;

	SampleByteSize = 2;

	// This is set to true to default to old behavior in old audio engine
	// Audio mixer uses sound wave procedural in async tasks and sets this to false when using it.
	
	// This is actually a 'bIsNotReadyForDestroy', but we can't change headers for a hotfix release.
	// This should be renamed as soon as possible, or a USoundWaveProcedural(FVTableHelper& Helper) constructor
	// should be added which sets this to true.
	bIsReadyForDestroy = false;

	checkf(NumSamplesToGeneratePerCallback >= NumBufferUnderrunSamples, TEXT("Should generate more samples than this per callback."));
}

void USoundWaveProcedural::QueueAudio(const uint8* AudioData, const int32 BufferSize)
{
	Audio::EAudioMixerStreamDataFormat::Type Format = GetGeneratedPCMDataFormat();
	SampleByteSize = (Format == Audio::EAudioMixerStreamDataFormat::Int16) ? 2 : 4;

	if (BufferSize == 0 || !ensure((BufferSize % SampleByteSize) == 0))
	{
		return;
	}

	TArray<uint8> NewAudioBuffer;
	NewAudioBuffer.AddUninitialized(BufferSize);
	FMemory::Memcpy(NewAudioBuffer.GetData(), AudioData, BufferSize);
	QueuedAudio.Enqueue(NewAudioBuffer);

	AvailableByteCount.Add(BufferSize);
}

void USoundWaveProcedural::PumpQueuedAudio()
{
	// Pump the enqueued audio
	TArray<uint8> NewQueuedBuffer;
	while (QueuedAudio.Dequeue(NewQueuedBuffer))
	{
		AudioBuffer.Append(NewQueuedBuffer);
	}
}

int32 USoundWaveProcedural::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	// Check if we've been told to reset our audio buffer
	if (bReset)
	{
		bReset = false;
		AudioBuffer.Reset();
		AvailableByteCount.Reset();
	}

	Audio::EAudioMixerStreamDataFormat::Type Format = GetGeneratedPCMDataFormat();
	SampleByteSize = (Format == Audio::EAudioMixerStreamDataFormat::Int16) ? 2 : 4;

	int32 SamplesAvailable = AudioBuffer.Num() / SampleByteSize;
	int32 SamplesToGenerate = FMath::Min(NumSamplesToGeneratePerCallback, SamplesNeeded);

	check(SamplesToGenerate >= NumBufferUnderrunSamples);

	bool bPumpQueuedAudio = true;

	if (SamplesAvailable < SamplesToGenerate)
	{
		// First try to use the virtual function which assumes we're writing directly into our audio buffer
		// since we're calling from the audio render thread.
		if (OnGeneratePCMAudio(AudioBuffer, SamplesToGenerate))
		{
			bPumpQueuedAudio = false;
		}
		else if (OnSoundWaveProceduralUnderflow.IsBound())
		{
			// Note that this delegate may or may not fire inline here. If you need
			// To gaurantee that the audio will be filled, don't use this delegate function
			OnSoundWaveProceduralUnderflow.Execute(this, SamplesToGenerate);
		}
	}

	if (bPumpQueuedAudio)
	{
		PumpQueuedAudio();
	}

	SamplesAvailable = AudioBuffer.Num() / SampleByteSize;

	// Wait until we have enough samples that are requested before starting.
	if (SamplesAvailable >= SamplesToGenerate)
	{
		const int32 SamplesToCopy = FMath::Min<int32>(SamplesToGenerate, SamplesAvailable);
		const int32 BytesToCopy = SamplesToCopy * SampleByteSize;

		FMemory::Memcpy((void*)PCMData, &AudioBuffer[0], BytesToCopy);
		AudioBuffer.RemoveAt(0, BytesToCopy);

		// Decrease the available by count
		AvailableByteCount.Subtract(BytesToCopy);

		return BytesToCopy;
	}

	// There wasn't enough data ready, write out zeros
	const int32 BytesCopied = NumBufferUnderrunSamples * SampleByteSize;
	FMemory::Memzero(PCMData, BytesCopied);
	return BytesCopied;
}

void USoundWaveProcedural::ResetAudio()
{
	// Empty out any enqueued audio buffers
	QueuedAudio.Empty();

	// Flag that we need to reset our audio buffer (on the audio thread)
	bReset = true;
}

int32 USoundWaveProcedural::GetAvailableAudioByteCount()
{
	return AvailableByteCount.GetValue();
}

int32 USoundWaveProcedural::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

void USoundWaveProcedural::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

bool USoundWaveProcedural::IsReadyForFinishDestroy()
{
	return !bIsReadyForDestroy;
}

bool USoundWaveProcedural::HasCompressedData(FName Format) const
{
	return false;
}

FByteBulkData* USoundWaveProcedural::GetCompressedData(FName Format)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
	return nullptr;
}

void USoundWaveProcedural::Serialize(FArchive& Ar)
{
	// Do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}

void USoundWaveProcedural::InitAudioResource(FByteBulkData& CompressedData)
{
	// Should never be pushing compressed data to a SoundWaveProcedural
	check(false);
}

bool USoundWaveProcedural::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a USoundWaveProcedural
	return true;
}
