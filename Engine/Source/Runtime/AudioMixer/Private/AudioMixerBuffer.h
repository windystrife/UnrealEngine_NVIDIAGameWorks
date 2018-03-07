// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerSourceDecode.h"
#include "AudioMixer.h"

namespace Audio
{
	namespace EBufferType
	{
		enum Type
		{
			PCM,
			PCMPreview,
			PCMRealTime,
			Streaming,
			Invalid,
		};
	}

	class FMixerDevice;
	class FMixerBuffer;

	class FMixerBuffer : public FSoundBuffer
	{
	public:
		FMixerBuffer(FAudioDevice* InAudioDevice, USoundWave* InWave, EBufferType::Type InBufferType);
		~FMixerBuffer();

		//~ Begin FSoundBuffer Interface
		int32 GetSize() override;
		int32 GetCurrentChunkIndex() const override;
		int32 GetCurrentChunkOffset() const override;
		bool IsRealTimeSourceReady() override;
		bool ReadCompressedInfo(USoundWave* SoundWave) override;
		bool ReadCompressedData(uint8* Destination, bool bLooping) override;
		void Seek(const float SeekTime) override;
		//~ End FSoundBuffer Interface

		// Allows specifying the number of frames you want to read
		bool ReadCompressedData(uint8* Destination, int32 NumFrames, bool bLooping);

		static FMixerBuffer* Init(FAudioDevice* AudioDevice, USoundWave* InWave, bool bForceRealtime);
		static FMixerBuffer* CreatePreviewBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateProceduralBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateNativeBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateStreamingBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateRealTimeBuffer(FMixerDevice* InMixer, USoundWave* InWave);

		/** Returns the buffer's format */
		EBufferType::Type GetType() const;
		bool IsRealTimeBuffer() const;

		/** Returns the contained raw PCM data and data size */
		void GetPCMData(uint8** OutData, uint32* OutDataSize);

		void EnsureHeaderParseTaskFinished();

		float GetSampleRate() const { return SampleRate; }
		int32 GetNumChannels() const { return NumChannels; }
		void InitSampleRate(const float InSampleRate) { SampleRate = InSampleRate; }

	private:

		/** Async task for parsing real-time decompressed compressed info headers. */
		IAudioTask* RealtimeAsyncHeaderParseTask;

		/** Wrapper to handle the decompression of audio codecs. */
		ICompressedAudioInfo* DecompressionState;

		/** The ptr to this buffers sound wave procedural if it was created from a procedural sound wave. */
		USoundWaveProcedural* SoundWaveProcedural;

		/** Format of the sound referenced by this buffer */
		EBufferType::Type BufferType;

		/** Sample rate of the audio buffer. */
		int32 SampleRate;

		/** Number of bits per sample. */
		int16 BitsPerSample;

		/** Ptr to raw PCM data. */
		uint8* Data;

		/** The raw PCM data size. */
		uint32 DataSize;

		/** Bool indicating the that the real-time source is ready for real-time decoding. */
		FThreadSafeBool bIsRealTimeSourceReady;

		/** Set to true when the PCM data should be freed when the buffer is destroyed */
		bool bIsDynamicResource;
	};
}

