// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerBus.h"
#include "AudioMixerSourceManager.h"

namespace Audio
{
	FMixerBus::FMixerBus(FMixerSourceManager* InSourceManager, const int32 InNumChannels, const int32 InNumFrames)
		: CurrentBufferIndex(1)
		, NumChannels(InNumChannels)
		, NumFrames(InNumFrames)
		, SourceManager(InSourceManager)
	{
		// Prepare the previous buffer with zero'd data
		const int32 NumSamples = InNumChannels * InNumFrames;
		for (int32 i = 0; i < 2; ++i)
		{
			MixedSourceData[i].Reset();
			MixedSourceData[i].AddZeroed(NumSamples);
		}
	}

	void FMixerBus::Update()
	{
		CurrentBufferIndex = !CurrentBufferIndex;
	}

	void FMixerBus::AddInstanceId(const int32 InSourceId)
	{
		InstanceIds.Add(InSourceId);
	}

	bool FMixerBus::RemoveInstanceId(const int32 InSourceId)
	{
		InstanceIds.Remove(InSourceId);

		// Return true if there is no more instances or sends
		return !InstanceIds.Num() && !BusSends.Num();
	}

	void FMixerBus::AddBusSend(const FBusSend& InBusSend)
	{
		BusSends.Add(InBusSend);
	}

	bool FMixerBus::RemoveBusSend(const int32 InSourceId)
	{
		for (int32 i = BusSends.Num() - 1; i >= 0; --i)
		{
			// Remove this source id's send
			if (BusSends[i].SourceId == InSourceId)
			{
				BusSends.RemoveAtSwap(i, 1, false);

				// There will only be one entry
				break;
			}
		}

		// Return true if there is no more instances or sends
		return !InstanceIds.Num() && !BusSends.Num();
	}

	void FMixerBus::MixBuffer()
	{
		// Reset and zero the mixed source data buffer for this bus
		const int32 NumSamples = NumFrames * NumChannels;

		MixedSourceData[CurrentBufferIndex].Reset();
		MixedSourceData[CurrentBufferIndex].AddZeroed(NumSamples);

		float* BusDataBufferPtr = MixedSourceData[CurrentBufferIndex].GetData();

		// Loop through the send list for this bus
		for (FBusSend& BusSend : BusSends)
		{
			const float* SourceBufferPtr = nullptr;

			// If the source is a bus, we need to use the previous renderer buffer 
			if (SourceManager->IsBus(BusSend.SourceId))
			{
				SourceBufferPtr = SourceManager->GetPreviousBusBuffer(BusSend.SourceId);
			}
			// If the source mixing into this is not itself a bus, then simply mix the pre-attenuation audio of the source into the bus
			// The source will have already computed its buffers for this frame
			else
			{
				SourceBufferPtr = SourceManager->GetPreDistanceAttenuationBuffer(BusSend.SourceId);
			}

			const int32 NumSourceChannels = SourceManager->GetNumChannels(BusSend.SourceId);
			const int32 NumSourceSamples = NumSourceChannels * SourceManager->GetNumOutputFrames();

			// If source channels are 1 but the bus is 2 channels, we need to up-mix
			if (NumSourceChannels == 1 && NumChannels == 2)
			{
				int32 BusSampleIndex = 0;
				for (int32 SourceSampleIndex = 0; SourceSampleIndex < NumSourceSamples; ++SourceSampleIndex)
				{
					// Take half the source sample to up-mix to stereo
					const float SourceSample = 0.5f * BusSend.SendLevel * SourceBufferPtr[SourceSampleIndex];

					// Mix in the source sample
					for (int32 BusChannel = 0; BusChannel < NumChannels; ++BusChannel)
					{
						BusDataBufferPtr[BusSampleIndex++] += SourceSample;
					}
				}
			}
			// If the source channels are 2 but the bus is 1 channel, we need to down-mix
			else if (NumSourceChannels == 2 && NumChannels == 1)
			{
				int32 SourceSampleIndex = 0;
				for (int32 BusSampleIndex = 0; BusSampleIndex < NumSourceSamples; ++BusSampleIndex)
				{
					// Downmix the stereo source to mono before summing to bus
					float SourceSample = 0.0f;
					for (int32 SourceChannel = 0; SourceChannel < 2; ++SourceChannel)
					{
						SourceSample += SourceBufferPtr[SourceSampleIndex++];
					}

					SourceSample *= 0.5f;
					SourceSample *= BusSend.SendLevel;

					BusDataBufferPtr[BusSampleIndex] += SourceSample;
				}
			}
			// If they're the same channels, just mix it in
			else
			{
				for (int32 SampleIndex = 0; SampleIndex < NumSourceSamples; ++SampleIndex)
				{
					BusDataBufferPtr[SampleIndex] += (BusSend.SendLevel * SourceBufferPtr[SampleIndex]);
				}
			}
		}
	}


	const float* FMixerBus::GetCurrentBusBuffer() const
	{
		return MixedSourceData[CurrentBufferIndex].GetData();
	}

	const float* FMixerBus::GetPreviousBusBuffer() const
	{
		return MixedSourceData[!CurrentBufferIndex].GetData();
	}

}

