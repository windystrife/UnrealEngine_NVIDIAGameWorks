// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioVoiceMixer.h"
#include "UnrealAudioVoiceManager.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	FVoiceMixer::FVoiceMixer(FUnrealAudioModule* InAudioModule, FVoiceManager* InVoiceManager)
		: AudioModule(InAudioModule)
		, VoiceManager(InVoiceManager)
		, NumVoicesPerDecoder(0)
		, MaxVoices(0)
		, NumActiveVoices(0)
		, DeviceThreadCommandQueue(500)
		, AudioSystemCommandQueue(1000)
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		, AudioThreadId(INDEX_NONE)
		, DeviceThreadId(INDEX_NONE)
#endif
	{
	}

	FVoiceMixer::~FVoiceMixer()
	{
	}

	void FVoiceMixer::Init(const struct FVoiceManagerSettings& Settings)
	{
		InitAudioSystemThreadId();

		// Initialize the sound file decoders used by voice manager
		ControlUpdateRateSeconds = Settings.ControlUpdateRateSeconds;
		NumVoicesPerDecoder = Settings.MaxVoiceCount / Settings.NumDecoders;
		SoundFileDecoders.Init(FSoundFileDecoder(AudioModule), Settings.NumDecoders);
		for (uint32 i = 0; i < Settings.NumDecoders; ++i)
		{
			SoundFileDecoders[i].Init(Settings.DecoderSettings, NumVoicesPerDecoder);
		}
		// initialize the data needed for mixing
		VoiceData.Init(FVoiceDataEntry(), Settings.MaxVoiceCount);

		for (int32 i = 0; i < Settings.MaxVoiceCount; ++i)
		{
			FreeVoiceIndices.Enqueue(i);
		}

	}

	int32 FVoiceMixer::InitEntry(const FVoiceMixerVoiceData& Data)
	{
// 		int32 VoiceIndex;
// 		if (FreeVoiceIndices.Dequeue(VoiceIndex))
// 		{
// 			DeviceThreadCommandQueue.Enqueue(FCommand(EAudioDeviceThreadMixCommand::VOICE_INIT_ENTRY, VoiceIndex, Data.NumChannels, Data.FrameRate, Data.VolumeProduct, Data.PitchProduct));
// 
// // 			Entry.NumChannels = Data.NumChannels;
// // 			Entry.FrameRate = Data.FrameRate;
// // 			Entry.CurrentFrame = INDEX_NONE;
// // 			Entry.Volume.Init(Data.VolumeProduct);
// // 			Entry.Pitch.Init(Data.PitchProduct);
// // 			Entry.SampleRateConverter.Init(Data.PitchProduct, Data.NumChannesl);
// 
// 			// Now flag this voice entry as pending activation.. it won't activate until the next update
// 			// from the audio device
// 			return VoiceIndex;
// 		}

		return INDEX_NONE;
	}

	void FVoiceMixer::ReleaseEntry(uint32 VoiceIndex)
	{
// 		FreeVoiceIndices.Enqueue(VoiceIndex);
// 		DeviceThreadCommandQueue.Enqueue(FCommand(EAudioDeviceThreadMixCommand::VOICE_RELEASE_ENTRY, VoiceIndex, VoiceIndex));
	}

	void FVoiceMixer::UpdateDeviceThread(struct FCallbackInfo& CallbackInfo)
	{
// 		InitDeviceThreadId();
// 
// 		int32 FrameRate = CallbackInfo.FrameRate;
// 		float* OutBuffer = CallbackInfo.OutBuffer;
// 		uint32 NumFrames = CallbackInfo.NumFrames;
// 		uint32 NumChannels = CallbackInfo.NumChannels;
// 		const TArray<ESpeaker::Type>& OutputSpeakers = CallbackInfo.OutputSpeakers;
// 		double StreamTime = CallbackInfo.StreamTime;
// 
// 		// How many frames to interpolate any parameter changes from system thread
// 		int32 InterpolationFrames = FrameRate * ControlUpdateRateSeconds;
// 		check(InterpolationFrames > 0);
// 
// 		// Update any pending messages
// 		PumpDeviceThreadMessages();
// 
// 		uint32 VoiceCount = 0;
// 
// 		for (uint32 VoiceIndex = 0; VoiceIndex < MaxVoices && VoiceCount < NumActiveVoices.GetValue(); ++VoiceIndex)
// 		{
// 			if (!VoiceData.IsActive[VoiceIndex])
// 			{
// 				continue;
// 			}
// 
// 			++VoiceCount;
// 
// 			// Get voice data index (index in the overall data array of this voice)
// 			uint32 VoiceDataIndex = VoiceData.VoiceDataIndex[VoiceIndex];
// 			check(VoiceDataIndex != INVALID_INDEX);
// 
// 			// Get the volume and pitch information from voice manager
// 			float VolumeProduct = 0.0f;
// 			VoiceManager->GetVolumeProduct(VoiceDataIndex, VolumeProduct);
// 
// 			float PitchProduct = 1.0f;
// 			VoiceManager->GetPitchProduct(VoiceDataIndex, PitchProduct);
// 
// 			// Get current volume data
// 			float VolumeTarget = VoiceData.VolumeTarget[VoiceIndex];
// 			float VolumeCurrent = VoiceData.VolumeCurrent[VoiceIndex];
// 
// 			// Update the target volume and volume increment if different
// 			if (VolumeProduct != VolumeTarget)
// 			{
// 				VoiceData.VolumeTarget[VoiceIndex] = VolumeProduct;
// 				VoiceData.VolumeDelta[VoiceIndex] = (VolumeProduct - VolumeCurrent) / InterpolationFrames;
// 				VoiceData.VolumeFrameCount[VoiceIndex] = 0;
// 			}
// 
// 			// Get the current pitch data
// 			float PitchTarget = VoiceData.PitchTarget[VoiceIndex];
// 			float PitchCurrent = VoiceData.PitchCurrent[VoiceData];
// 
// 			if (PitchProduct != PitchTarget)
// 			{
// 				VoiceData.PitchTarget[VoiceIndex] = PitchTarget;
// 				VoiceData.PitchDelta[VoiceIndex] = (PitchProduct - PitchCurrent) / InterpolationFrames;
// 				VoiceData.PitchFrameCount[VoiceIndex] = 0;
// 			}
// 
// 			for (uint32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
// 			{
// 				// Figure out what the next frame will be based on the current pitch
// 				double NextFrame = VoiceData.CurrentFrame[VoiceIndex] + (double)VoiceData.PitchCurrent[VoiceIndex];
// 
// 				// Figure out how many frames we need to get from the decoder to account for the current pitch value
// 				uint32 DecodedFramesToRequest = 0;
// 				if ((int32)VoiceData.CurrentFrame[VoiceIndex] != (int32)NextFrame)
// 				{
// 					DecodedFramesToRequest = (int32)NextFrame - (int32)VoiceData.CurrentFrame[VoiceIndex];
// 				}
// 
// 				if (DecodedFramesToRequest != 0)
// 				{
// 					uint32 NumSamples = DecodedFramesToRequest * VoiceData.NumChannels[VoiceIndex];
// 
// 					// Reset the decode buffer to be the number of samples we're going to get this frame
// 					DecodeBuffer.Reset(NumSamples);
// 
// 					// Now fill the buffer from the decoder with the number of samples
// 					FillDecodedBuffer(VoiceIndex, DecodeBuffer);
// 				}
// 
// 				// Now we should either have a new decode buffer or the previous decode buffer (which will happen if pitch is less than 1.0)
// 
// 
// 				// Update the current volume and pitch interpolation values
// 				if (VoiceData.PitchFrameCount[VoiceIndex] < InterpolationFrames)
// 				{
// 					VoiceData.PitchFrameCount[VoiceIndex]++;
// 					VoiceData.VolumeCurrent[VoiceIndex] += VoiceData.VolumeDelta[VoiceIndex];
// 				}
// 
// 				if (VoiceData.VolumeFrameCount[VoiceIndex] < InterpolationFrames)
// 				{
// 					VoiceData.VolumeFrameCount[VoiceIndex]++;
// 					VoiceData.PitchCurrent[VoiceIndex] += VoiceData.PitchDelta[VoiceIndex];
// 				}
// 			}
// 		}
	}

	uint32 FVoiceMixer::GetNumActiveVoices() const
	{
		return 0;
//		return NumActiveVoices.GetValue();
	}

	void FVoiceMixer::FillDecodedBuffer(uint32 VoiceIndex, TArray<float>& InDecodeBuffer)
	{
// 		// first get which decoder to use given the index
// 		check(NumVoicesPerDecoder > 0);
// 		uint32 DecoderIndex = VoiceIndex / NumVoicesPerDecoder;
// 		uint32 DecoderVoiceIndex = VoiceIndex % NumVoicesPerDecoder;
// 
// 		FSoundFileDecoder& Decoder = SoundFileDecoders[DecoderIndex];
// 
// 		Decoder.GetDecodedAudioData(DecoderVoiceIndex, InDecodeBuffer);
	}

	void FVoiceMixer::InitAudioSystemThreadId()
	{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		AudioThreadId = FPlatformTLS::GetCurrentThreadId();
#endif // #if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
	}

	void FVoiceMixer::InitDeviceThreadId()
	{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		if (DeviceThreadId == INDEX_NONE)
		{
			DeviceThreadId = FPlatformTLS::GetCurrentThreadId();
		}
#endif // #if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
	}

	void FVoiceMixer::CheckAudioSystemThread()
	{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		checkf(CurrentThreadId == AudioThreadId,
			   TEXT("Function called on wrong thread with id '%d' but supposed to be called on audio thread (id=%d)."),
			   CurrentThreadId,
			   AudioThreadId
			   );
#endif // ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
	}

	void FVoiceMixer::CheckDeviceThread()
	{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		checkf(CurrentThreadId == DeviceThreadId,
			   TEXT("Function called on wrong thread with id '%d' but supposed to be called on audio device thread (id=%d)."),
			   CurrentThreadId,
			   AudioThreadId
			   );
#endif // ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
	}

}

#endif // #if ENABLE_UNREAL_AUDIO

