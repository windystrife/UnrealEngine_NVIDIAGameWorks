// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	class FMixerSourceManager;

	// Struct holding mappings of runtime source ids (bus instances) to bus send level
	struct FBusSend
	{
		int32 SourceId;
		float SendLevel;
	};

	// Bus instance data. Holds source id bus instances and bus sends data
	class FMixerBus
	{
	public:
		FMixerBus(FMixerSourceManager* SourceManager, const int32 InNumChannels, const int32 InNumFrames);
		
		// Update the mixer bus after a render block
		void Update();

		// Adds a source id for instances of this bus
		void AddInstanceId(const int32 InSourceInstanceId);

		// Removes the source id from this bus. Returns true if there are no more instances or sends.
		bool RemoveInstanceId(const int32 InSourceId);

		// Adds a buss end to the bus
		void AddBusSend(const FBusSend& InBusSend);

		// Removes the source instance from this bus's send list
		bool RemoveBusSend(const int32 InSourceId);

		// Gets the current mixed bus buffer
		const float* GetCurrentBusBuffer() const;

		// Gets the previous mixed bus buffer
		const float* GetPreviousBusBuffer() const;

		// Compute the mixed buffer
		void MixBuffer();

	private:

		// Array of instance ids. These are sources which are instances of this.
		// It's possible for this data to have bus sends but no instance ids.
		// This means a source would send its audio to the bus if the bus had an instance.
		// Once and instance plays, it will then start sending its audio to the bus instances.
		TArray<int32> InstanceIds;

		// Bus sends to this instance
		TArray<FBusSend> BusSends;

		// The mixed source data. This is double-buffered to allow buses to send audio to themselves.
		// Buses feed audio to each other by storing their previous buffer. Current buses mix in previous other buses (including themselves)
		TArray<float> MixedSourceData[2];

		// The index of the bus data currently being rendered
		int32 CurrentBufferIndex;

		// The number of channels of this bus
		int32 NumChannels;

		// The number of output frames
		int32 NumFrames;

		// Owning soruce manager
		FMixerSourceManager* SourceManager;

		FMixerBus();
	};

}
