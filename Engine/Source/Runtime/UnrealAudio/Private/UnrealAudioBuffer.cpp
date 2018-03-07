// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioBuffer.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	template<typename T>
	class TIntermediateBuffer : public IIntermediateBuffer
	{
	public:
		TIntermediateBuffer()
			: DataBuffer(nullptr)
		{
		}

		~TIntermediateBuffer()
		{
			if (DataBuffer)
			{
				delete[] DataBuffer;
			}
		}

		void Initialize(uint32 InNumSamples) override
		{
			DataBuffer = new T[InNumSamples];
			memset((void*)DataBuffer, 0, InNumSamples * sizeof(T));
			NumSamples = InNumSamples;
			WriteIndex = 0;
			ReadIndex = 0;
		}

		bool Write(uint8* InByteBuffer, uint32 InBufferSampleSize) override
		{
			if (!InByteBuffer || InBufferSampleSize == 0 || InBufferSampleSize > NumSamples)
			{
				return false;
			}

			uint32 RelativeReadIndex = ReadIndex;
			uint32 NextWriteIndex = WriteIndex + InBufferSampleSize;

			if (RelativeReadIndex < WriteIndex && NextWriteIndex >= NumSamples)
			{
				RelativeReadIndex += NumSamples;
			}

			if (WriteIndex <= RelativeReadIndex && NextWriteIndex > RelativeReadIndex)
			{
				return false;
			}

			// How many samples from beginning and from write index
			uint32 FromBeginning = (uint32)FMath::Max<int32>((int32)WriteIndex + (int32)InBufferSampleSize - (int32)NumSamples, 0);
			uint32 FromWrite = InBufferSampleSize - FromBeginning;

			memcpy(&DataBuffer[WriteIndex], InByteBuffer, FromWrite * sizeof(T));
			memcpy(DataBuffer, &((T*)InByteBuffer)[FromWrite], FromBeginning * sizeof(T));

			// Update the write index
			WriteIndex = (WriteIndex + InBufferSampleSize) % NumSamples;
			return true;

		}

		bool Read(uint8* OutByteBuffer, uint32 OutBufferSampleSize) override
		{
			if (!OutByteBuffer || OutBufferSampleSize == 0 || OutBufferSampleSize > NumSamples)
			{
				return false;
			}

			unsigned int RelativeWriteIndex = WriteIndex;
			unsigned int NextReadIndex = ReadIndex + OutBufferSampleSize;
			if (RelativeWriteIndex < ReadIndex && NextReadIndex >= NumSamples)
			{
				RelativeWriteIndex += NumSamples;
			}

			if (ReadIndex < RelativeWriteIndex && NextReadIndex > RelativeWriteIndex) {
				return false;
			}

			// How many samples to read from beginning and from read index
			uint32 FromBeginning = (uint32)FMath::Max<int32>((int32)ReadIndex + (int32)OutBufferSampleSize - (int32)NumSamples, 0);
			uint32 FromReadIndex = OutBufferSampleSize - FromBeginning;

			memcpy(OutByteBuffer, &DataBuffer[ReadIndex], FromReadIndex * sizeof(T));
			memcpy(&((T*)OutByteBuffer)[FromReadIndex], DataBuffer, FromBeginning * sizeof(T));

			ReadIndex = (ReadIndex + OutBufferSampleSize) % NumSamples;
			return true;
		}

	private:
		T* DataBuffer;
	};

	IIntermediateBuffer* IIntermediateBuffer::CreateIntermediateBuffer(EStreamFormat::Type Format)
	{
		switch (Format)
		{
		default:
		case EStreamFormat::UNKNOWN:
			return nullptr;
		case EStreamFormat::FLT:
			return new TIntermediateBuffer<float>();
		case EStreamFormat::DBL:
			return new TIntermediateBuffer<double>();
		case EStreamFormat::INT_16:
			return new TIntermediateBuffer<int16>();
		case EStreamFormat::INT_24:
			return new TIntermediateBuffer<FInt24>();
		case EStreamFormat::INT_32:
			return new TIntermediateBuffer<int32>();
		}
	}
}

#endif // #if ENABLE_UNREAL_AUDIO



