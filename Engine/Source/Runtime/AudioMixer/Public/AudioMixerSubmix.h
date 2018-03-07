// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "Sound/SoundSubmix.h"

class USoundEffectSubmix;

namespace Audio
{
	class IAudioMixerEffect;
	class FMixerSourceVoice;
	class FMixerDevice;

	typedef TSharedPtr<FSoundEffectSubmix, ESPMode::ThreadSafe> FSoundEffectSubmixPtr;

	class FMixerSubmix
	{
	public:
		FMixerSubmix(FMixerDevice* InMixerDevice);
		~FMixerSubmix();

		// Initialize the submix object with the USoundSubmix ptr. Sets up child and parent connects.
		void Init(USoundSubmix* InSoundSubmix);

		// Returns the mixer submix Id
		uint32 GetId() const { return Id; }

		// Sets the parent submix to the given submix
		void SetParentSubmix(TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> Submix);

		// Adds the given submix to this submix's children
		void AddChildSubmix(TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> Submix);

		// Gets this submix's parent submix
		TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> GetParentSubmix();

		// Returns the number of source voices currently a part of this submix.
		int32 GetNumSourceVoices() const;

		// Returns the number of wet effects in this submix.
		int32 GetNumEffects() const;

		// Add (if not already added) or sets the amount of the source voice's send amount
		void AddOrSetSourceVoice(FMixerSourceVoice* InSourceVoice, const float SendLevel);

		/** Removes the given source voice from the submix. */
		void RemoveSourceVoice(FMixerSourceVoice* InSourceVoice);

		/** Appends the effect submix to the effect submix chain. */
		void AddSoundEffectSubmix(uint32 SubmixPresetId, FSoundEffectSubmixPtr InSoundEffectSubmix);

		/** Removes the submix effect from the effect submix chain. */
		void RemoveSoundEffectSubmix(uint32 SubmixPresetId);

		/** Clears all submix effects from the effect submix chain. */
		void ClearSoundEffectSubmixes();

		// Function which processes audio.
		void ProcessAudio(AlignedFloatBuffer& OutAudio);

		// Returns the device sample rate this submix is rendering to
		int32 GetSampleRate() const;

		// Returns the output channels this submix is rendering to
		int32 GetNumOutputChannels() const;

		// Updates the submix from the main thread.
		void Update();

		// Returns the number of effects in this submix's effect chain
		int32 GetNumChainEffects() const;

		// Returns the submix effect at the given effect chain index
		FSoundEffectSubmixPtr GetSubmixEffect(const int32 InIndex);

	protected:
		// Down mix the given buffer to the desired down mix channel count
		void DownmixBuffer(const int32 InputChannelCount, const AlignedFloatBuffer& InBuffer, const int32 DownMixChannelCount, AlignedFloatBuffer& OutDownmixedBuffer);

	protected:

		// Pump command queue
		void PumpCommandQueue();

		// Add command to the command queue
		void SubmixCommand(TFunction<void()> Command);

		// This mixer submix's Id
		uint32 Id;

		// Parent submix. 
		TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> ParentSubmix;

		// Child submixes
		TMap<uint32, TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe>> ChildSubmixes;

		// Info struct for a submix effect instance
		struct FSubmixEffectInfo
		{
			// The preset object id used to spawn this effect instance
			uint32 PresetId;

			// The effect instance ptr
			FSoundEffectSubmixPtr EffectInstance;

			FSubmixEffectInfo()
				: PresetId(INDEX_NONE)
			{
			}
		};

		// The effect chain of this submix, based on the sound submix preset chain
		TArray<FSubmixEffectInfo> EffectSubmixChain;

		// Owning mixer device. 
		FMixerDevice* MixerDevice;

		// Map of mixer source voices with a given send level for this submix
		TMap<FMixerSourceVoice*, float> MixerSourceVoices;

		AlignedFloatBuffer ScratchBuffer;
		AlignedFloatBuffer DownmixedBuffer;

		// Submic command queue to shuffle commands from audio thread to audio render thread.
		TQueue<TFunction<void()>> CommandQueue;

		friend class FMixerDevice;
	};



}
