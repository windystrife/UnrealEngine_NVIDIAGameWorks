// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/** 
 * Playable sound object for wave files that are streamed, particularly VOIP
 */

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Sound/SoundWave.h"
#include "SoundWaveProcedural.generated.h"

DECLARE_DELEGATE_TwoParams( FOnSoundWaveProceduralUnderflow, class USoundWaveProcedural*, int32 );

UCLASS()
class ENGINE_API USoundWaveProcedural : public USoundWave
{
	GENERATED_UCLASS_BODY()

private:
	// A thread safe queue for queuing audio to be consumed on audio thread
	TQueue<TArray<uint8>> QueuedAudio;

	// The amount of bytes queued and not yet consumed
	FThreadSafeCounter AvailableByteCount;

	// The actual audio buffer that can be consumed. QueuedAudio is fed to this buffer. Accessed only audio thread.
	TArray<uint8> AudioBuffer;

	// Flag to reset the audio buffer
	FThreadSafeBool bReset;

	// Pumps audio queued from game thread
	void PumpQueuedAudio();

protected:

	// Number of samples to pad with 0 if there isn't enough audio available
	int32 NumBufferUnderrunSamples;

	// The number of PCM samples we want to generate. This can't be larger than SamplesNeeded in GeneratePCMData callback, but can be less.
	int32 NumSamplesToGeneratePerCallback;

public:

	//~ Begin UObject Interface. 
	virtual void Serialize( FArchive& Ar ) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual bool IsReadyForFinishDestroy() override;
	//~ End UObject Interface. 

	//~ Begin USoundWave Interface.
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	virtual bool HasCompressedData(FName Format) const override;
	virtual FByteBulkData* GetCompressedData(FName Format) override;
	virtual void InitAudioResource( FByteBulkData& CompressedData ) override;
	virtual bool InitAudioResource(FName Format) override;
	virtual int32 GetResourceSizeForFormat(FName Format) override;
	//~ End USoundWave Interface.

	// Virtual function to generate PCM audio from the audio render thread. 
	virtual bool OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) { return false; }

	/** Add data to the FIFO that feeds the audio device. */
	void QueueAudio(const uint8* AudioData, const int32 BufferSize);

	/** Remove all queued data from the FIFO. This is only necessary if you want to start over, or GeneratePCMData() isn't going to be called, since that will eventually drain it. */
	void ResetAudio();

	/** Query bytes queued for playback */
	int32 GetAvailableAudioByteCount();

	/** Called when GeneratePCMData is called but not enough data is available. Allows more data to be added, and will try again */
	FOnSoundWaveProceduralUnderflow OnSoundWaveProceduralUnderflow;

	/** Size in bytes of a single sample of audio in the procedural audio buffer. */
	int32 SampleByteSize;

	/** Whether or not this object is ready to be destroyed. Allows procedural sound wave generation to occur in async tasks without garbage collection deleting it from underneath. */
	bool bIsReadyForDestroy;
};
