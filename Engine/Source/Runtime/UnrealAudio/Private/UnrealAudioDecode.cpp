// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioDecode.h"
#include "HAL/RunnableThread.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	static int32 NumDecodeThreads = 0;

	FSoundFileDecoder::FSoundFileDecoder(FUnrealAudioModule* InAudioModule)
		: AudioModule(InAudioModule)
		, ThreadDecodeEvent(nullptr)
		, bIsDecoding(false)
		, DecodeThread(nullptr)
	{
		Settings = { 0 };
	}

	FSoundFileDecoder::~FSoundFileDecoder()
	{
		// Return the sync event from the pool
		if (ThreadDecodeEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(ThreadDecodeEvent);
		}
	}

	bool FSoundFileDecoder::Init(const FSoundFileDecoderSettings& InSettings, int32 NumVoices)
	{
		// Create a thread decode event to pause the decoding thread when all buffers are filled
		ThreadDecodeEvent = FPlatformProcess::GetSynchEventFromPool(false);

		// We really should be using at least a triple buffer decoder, but we may want more...
		check(InSettings.NumDecodeBuffers >= 3);

		for (int32 VoiceIndex = 0; VoiceIndex < NumVoices; ++VoiceIndex)
		{
			DecodeData.Add(FSoundFileDecodeData(InSettings.NumDecodeBuffers));
		}

		FString DecodeThreadName = FString::Printf(TEXT("Audio Decode Thread %d"), NumDecodeThreads++);
		DecodeThread = FRunnableThread::Create(this, *DecodeThreadName, 0, TPri_Normal);

		return true;
	}

	bool FSoundFileDecoder::Init()
	{
		bIsDecoding = true;
		return true;
	}

	uint32 FSoundFileDecoder::Run()
	{
		while (bIsDecoding)
		{
			for (int32 DataIndex = 0; DataIndex < Settings.NumDecodeBuffers; ++DataIndex)
			{
				FSoundFileDecodeData& Data = DecodeData[DataIndex];

				// If the data is not active, continue
				if (!Data.bIsActive)
				{
					continue;
				}

				int32 CurrentWriteBufferIndex = Data.CurrentWriteBufferIndex.GetValue();
				while (CurrentWriteBufferIndex != Data.CurrentReadBufferIndex.GetValue())
				{
					TArray<float>& DecodeBuffer = Data.DecodedBuffers[CurrentWriteBufferIndex];

					SoundFileCount RequestedNumSamples = (SoundFileCount)DecodeBuffer.Num();
					SoundFileCount NumSamplesActuallyRead = 0;

					// If the data is looping, then we need to keep reading samples from the file until we've reached the number of requested
					// samples. For *very* short duration sounds (or for long-duration decode buffers), we may have an edge case where
					// the sound file needs to seek from the beginning multiple times to fulfill the decode buffer size request
					if (Data.bIsLooping)
					{
						// Keep looping until we've read everything we requested
						while (true)
						{
							ESoundFileError::Type Result = Data.SoundFileReader->ReadSamples(DecodeBuffer.GetData(), RequestedNumSamples, NumSamplesActuallyRead);
							check(Result == ESoundFileError::NONE);

							// If we've read all the samples we wanted, then we're good
							if (RequestedNumSamples == NumSamplesActuallyRead)
							{
								break;
							}
							// ... otherwise, we need to seek back to the beginning of the file and try to read samples that are left
							else
							{
								// Seek to beginning of file
								SoundFileCount OutFrameOffset = 0;
								Data.SoundFileReader->SeekFrames(0, ESoundFileSeekMode::FROM_START, OutFrameOffset);

								// Calculate samples left we need to read
								RequestedNumSamples -= NumSamplesActuallyRead;
							}
						}
					}
					else
					{
						ESoundFileError::Type Result = Data.SoundFileReader->ReadSamples(DecodeBuffer.GetData(), RequestedNumSamples, NumSamplesActuallyRead);
						check(Result == ESoundFileError::NONE);

						// if the number of samples we read was not the number we requested, then we've reached the end of the file
						if (RequestedNumSamples != NumSamplesActuallyRead)
						{
							// tell the data that this entry is no longer active
							Data.bIsActive = false;

							// and what our last sample index is
							Data.LastSampleIndex = NumSamplesActuallyRead;
						}
					}

					// Increment the buffer index
					CurrentWriteBufferIndex = CurrentWriteBufferIndex % Settings.NumDecodeBuffers;

					// Publish results by writing setting buffer index so other thread can read it
					Data.CurrentWriteBufferIndex.Set(CurrentWriteBufferIndex);
				}

			}

			// Once we're done decoding everything, then put this thread to sleep
			// Any new requests (or any shutdowns) will trigger this thread to wake up.
			ThreadDecodeEvent->Wait((uint32)-1);
		}
		return true;
	}

	void FSoundFileDecoder::Stop()
	{
		bIsDecoding = false;
		Signal();
	}
	
	void FSoundFileDecoder::Exit()
	{
	}

	void FSoundFileDecoder::Signal()
	{
		check(ThreadDecodeEvent != nullptr);
		ThreadDecodeEvent->Trigger();
	}

	void FSoundFileDecoder::InitalizeEntry(uint32 VoiceIndex, TSharedPtr<ISoundFile> InSoundFileData)
	{
// 		FSoundFileDecodeData& VoiceDecodeData = DecodeData[VoiceIndex];
// 
// 		check(VoiceDecodeData.bIsActive == false);
// 
// 		// Get the number of channels in our sound file we're going to decode
// 
// 		FSoundFileManager& SoundFileManager = AudioModule->GetSoundFileManager();
// 
// 		const FSoundFileDescription& Description = InSoundFileData->GetDescription();
// 		int32 NumSamples = Settings.DecodeBufferFrames * Description.NumChannels;
// 
// 		// Make a new sound file reader with the sound file data
// 		TSharedPtr<ISoundFileReader> SoundFileReader = AudioModule->CreateSoundFileReader();
// 
// 		SoundFileReader->SetSoundFileData(InSoundFileData);
// 
// 		uint32 SoundFileId;
// 		InSoundFileData->GetId(SoundFileId);
// 
// 		VoiceDecodeData.SoundFileHandle = FSoundFileHandle(FEntityHandle(SoundFileId));
// 
// 		VoiceDecodeData.SoundFileReader = SoundFileReader;
// 		VoiceDecodeData.CurrentReadBufferIndex.Set(0);
// 		VoiceDecodeData.CurrentWriteBufferIndex.Set(1);
// 		VoiceDecodeData.CurrentReadSampleIndex = 0;
// 
// 		// Zero out the decoded data from any previous decoding sessions
// 		for (int32 BufferIndex = 0; BufferIndex < Settings.NumDecodeBuffers; ++BufferIndex)
// 		{
// 			VoiceDecodeData.DecodedBuffers[BufferIndex].Reset(NumSamples);
// 		}
// 
// 		// Publish that this entry is now active
// 		VoiceDecodeData.bIsActive = true;
// 
// 		// Wake up the writing thread in case its asleep
// 		Signal();
	}

	void FSoundFileDecoder::ClearEntry(uint32 Index)
	{
// 		FSoundFileDecodeData& VoiceDecodeData = DecodeData[Index];
// 
// 		check(VoiceDecodeData.bIsActive == true);
// 
// 		// inactivate the entry
// 		VoiceDecodeData.bIsActive = false;
// 		VoiceDecodeData.SoundFileReader = nullptr;
	}

	bool FSoundFileDecoder::GetDecodedAudioData(uint32 VoiceIndex, TArray<float>& AudioData)
	{
// 		FSoundFileDecodeData& VoiceDecodeData = DecodeData[VoiceIndex];
// 
// 		check(VoiceDecodeData.bIsActive == true);
// 
// 		int32 CurrentReadBufferIndex = VoiceDecodeData.CurrentReadBufferIndex.GetValue();
// 
// 		TArray<float>& CurrentReadBuffer = VoiceDecodeData.DecodedBuffers[CurrentReadBufferIndex];
// 		int32 ReadBufferSize = CurrentReadBuffer.Num();
// 
// 		// Get the current sample index
// 		int32& CurrentReadSampleIndex = VoiceDecodeData.CurrentReadSampleIndex;
// 		int32 NumRequestedSamples = AudioData.Num();
// 
// 		for (int32 OutputSample = 0; OutputSample < NumRequestedSamples; ++OutputSample)
// 		{
// 			// Check the non-terminating conditions
// 			if (VoiceDecodeData.bIsActive || (CurrentReadBufferIndex != VoiceDecodeData.CurrentWriteBufferIndex.GetValue()) || CurrentReadSampleIndex != VoiceDecodeData.LastSampleIndex)
// 			{
// 				// Copy the decoded data to the output buffer
// 				AudioData[OutputSample] = CurrentReadBuffer[CurrentReadSampleIndex++];
// 
// 				// if the current read sample index is equal to the current read buffer size,
// 				// then we'll need to bump the read buffer index and signal the decoding thread that 
// 				// it needs to wake up and start decoding some audio (if it's asleep)
// 				if (CurrentReadSampleIndex == ReadBufferSize)
// 				{
// 					CurrentReadBufferIndex = (CurrentReadBufferIndex + 1) % Settings.NumDecodeBuffers;
// 
// 					// first check to see if our new read buffer index is actually the same as the current
// 					// write buffer index, if it is, then we'll need to (unfortunately) wait. This should
// 					// rarely happen.
// 
// 					while (CurrentReadBufferIndex == VoiceDecodeData.CurrentWriteBufferIndex.GetValue())
// 					{
// 						// spin
// 					}
// 
// 					// free up the last read buffer for writing
// 					VoiceDecodeData.CurrentReadBufferIndex.Set(CurrentReadBufferIndex);
// 
// 					// Reset the read sample index 
// 					CurrentReadSampleIndex = 0;
// 
// 					// And update the read buffer
// 					CurrentReadBuffer = VoiceDecodeData.DecodedBuffers[CurrentReadBufferIndex];
// 				}
// 			}
// 			else
// 			{
// 				// we're done decoding this sound file... it's a "one-shot"
// 				return true;
// 			}
// 		}
// 
// 		// we still got more audio to decode on this sound file
		return false;

	}


}

#endif // #if ENABLE_UNREAL_AUDIO
