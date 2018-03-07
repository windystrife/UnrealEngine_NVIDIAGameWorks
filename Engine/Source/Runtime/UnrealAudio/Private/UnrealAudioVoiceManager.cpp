// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioVoiceManager.h"
#include "UnrealAudioPrivate.h"

#define UNREAL_AUDIO_VOICE_COMMAND_QUEUE_SIZE (50)

#define VOICE_CHECK_INITIALIZATION()													\
	if (VoiceState == EVoiceState::UNINITIALIZED)										\
	{																					\
		UA_SYSTEM_ERROR(ESystemError::NOT_INITIALIZED, TEXT("Voice not initialized.")); \
		return ESystemError::NOT_INITIALIZED;											\
	}


#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/************************************************************************/
	/* Main Thread Functions												*/
	/************************************************************************/

	FVoiceManager::FVoiceManager(FUnrealAudioModule* InAudioModule)
		: EntityManager(500)
		, AudioModule(InAudioModule)
		, PitchManager(InAudioModule)
		, VolumeManager(InAudioModule)
		, VoiceMixer(InAudioModule, this)
		, NumVirtualVoices(0)
		, NumRealVoices(0)
	{
		Settings = { 0 };
	}

	FVoiceManager::~FVoiceManager()
	{

	}

	void FVoiceManager::Init(const FVoiceManagerSettings& InSettings)
	{
		DEBUG_AUDIO_CHECK(InSettings.MaxVoiceCount > 0);
		DEBUG_AUDIO_CHECK(InSettings.MaxVirtualVoiceCount > 0);
		DEBUG_AUDIO_CHECK(Settings.MaxVoiceCount == 0);

		Settings = InSettings;
		uint32 TotalNumVoices = Settings.MaxVirtualVoiceCount + Settings.MaxVoiceCount;

		VoiceHandles.Init(FVoiceHandle(), TotalNumVoices);
		States.Init(EVoiceState::UNINITIALIZED, TotalNumVoices);
		PlayingStates.Init(EVoicePlayingState::NOT_PLAYING, TotalNumVoices);
		Flags.Init(0, TotalNumVoices);
		VolumeManager.Init(TotalNumVoices);
		PitchManager.Init(TotalNumVoices);
		VoiceMixer.Init(InSettings);

		for (uint32 i = 0; i < TotalNumVoices; ++i)
		{
			FreeDataIndicies.Enqueue(i);
		}
	}

	EVoiceError::Type FVoiceManager::PlayVoice(FVoice* Voice, const FVoiceInitializationData* InitData, FVoiceHandle& OutVoiceHandle)
	{
		// First create the voice handle
		FVoiceHandle VoiceHandle(EntityManager.CreateEntity());

		// Store the parent voice object ptr at the index associated with this handle.
		// This will allow us to send messages back to that voice object from the audio thread

		uint32 VoiceIndex = VoiceHandle.GetIndex();
		if (VoiceIndex < (uint32)VoiceObjects.Num())
		{
			check(VoiceObjects[VoiceIndex] == nullptr);
			VoiceObjects[VoiceIndex] = Voice;
		}
		else
		{
			check(VoiceIndex == (uint32)VoiceObjects.Num());
			VoiceObjects.Add(Voice);
		}

		OutVoiceHandle = VoiceHandle;

		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::VOICE_PLAY, VoiceHandle, (void*)InitData));

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::PauseVoice(const FVoiceHandle& VoiceHandle, float InFadeTimeSec)
	{
		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::VOICE_PAUSE, VoiceHandle, InFadeTimeSec));
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::StopVoice(const FVoiceHandle& VoiceHandle, float InFadeTimeSec)
	{
		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::VOICE_STOP, VoiceHandle, InFadeTimeSec));
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::SetVolumeScale(uint32 VoiceDataIndex, float InVolumeScale, float InFadeTimeSec)
	{
		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::VOICE_SET_VOLUME_SCALE, VoiceDataIndex, InVolumeScale, InFadeTimeSec));
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::SetPitchScale(uint32 VoiceDataIndex, float InPitchScale, float InFadeTimeSec)
	{
		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::VOICE_SET_PITCH_SCALE, VoiceDataIndex, InPitchScale, InFadeTimeSec));
		return EVoiceError::NONE;
	}

	void FVoiceManager::NotifyVoiceDone(const FCommand& Command)
	{
		check(Command.NumArguments == 1);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		uint32 Index = VoiceHandle.GetIndex();

		// Get the voice object
		FVoice* Voice = VoiceObjects[Index];
		check(Voice);

		// Notify that its done
		Voice->NotifyDone();

		// Free the slot in the voice objects array
		VoiceObjects[Index] = nullptr;

		// And free the handle
		EntityManager.ReleaseEntity(VoiceHandle);
	}

	void FVoiceManager::NotifyVoiceReal(const FCommand& Command)
	{
		check(Command.NumArguments == 2);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);
		check(Command.Arguments[1].DataType == ECommandData::UINT32);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		uint32 Index = VoiceHandle.GetIndex();

		uint32 VoiceDataIndex = Command.Arguments[1].Data.UnsignedInt32;

		// Get the voice object
		FVoice* Voice = VoiceObjects[Index];
		check(Voice);

		Voice->NotifyPlayReal(VoiceDataIndex);
	}

	void FVoiceManager::NotifyVoiceVirtual(const FCommand& Command)
	{
		check(Command.NumArguments == 2);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);
		check(Command.Arguments[1].DataType == ECommandData::UINT32);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		uint32 Index = VoiceHandle.GetIndex();

		uint32 VoiceDataIndex = Command.Arguments[1].Data.UnsignedInt32;

		// Get the voice object
		FVoice* Voice = VoiceObjects[Index];
		check(Voice != nullptr);

		Voice->NotifyPlayVirtual(VoiceDataIndex);
	}

	void FVoiceManager::NotifyVoiceSuspend(const FCommand& Command)
	{
		check(Command.NumArguments == 1);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		uint32 Index = VoiceHandle.GetIndex();

		// Get the voice object
		FVoice* Voice = VoiceObjects[Index];
		check(Voice);

		Voice->NotifySuspend();
	}

	EVoiceError::Type FVoiceManager::GetNumPlayingVoices(int32& OutNumPlayingVoices) const
	{
		OutNumPlayingVoices = NumRealVoices;
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetMaxNumberOfPlayingVoices(int32& OutMaxNumPlayingVoices) const
	{
		OutMaxNumPlayingVoices = Settings.MaxVoiceCount;
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetNumVirtualVoices(int32& OutNumVirtualVoices) const
	{
		OutNumVirtualVoices = NumVirtualVoices;
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetVolumeScale(uint32 VoiceDataIndex, float& OutVolumeProduct) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutVolumeProduct = VolumeManager.GetVolumeProduct(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetVolumeAttenuation(uint32 VoiceDataIndex, float& OutVolumeAttenuation) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutVolumeAttenuation = VolumeManager.GetVolumeAttenuation(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetVolumeFade(uint32 VoiceDataIndex, float& OutVolumeFade) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutVolumeFade = VolumeManager.GetVolumeFade(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetVolumeProduct(uint32 VoiceDataIndex, float& OutVolumeProduct) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutVolumeProduct = VolumeManager.GetVolumeProduct(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetPitchScale(uint32 VoiceDataIndex, float& OutPitchScale) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutPitchScale = PitchManager.GetPitchScale(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoiceManager::GetPitchProduct(uint32 VoiceDataIndex, float& OutPitchProduct) const
	{
		check(VoiceDataIndex != INDEX_NONE);
		OutPitchProduct = PitchManager.GetPitchProduct(VoiceDataIndex);
		return EVoiceError::NONE;
	}

	bool FVoiceManager::IsValidVoice(const FVoiceHandle& VoiceHandle) const
	{
		return EntityManager.IsValidEntity(VoiceHandle);
	}

	/************************************************************************/
	/* Audio Thread Functions												*/
	/************************************************************************/

	void FVoiceManager::PlayVoice(const FCommand& Command)
	{
		check(Command.NumArguments == 2);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);
		check(Command.Arguments[1].DataType == ECommandData::POINTER);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		uint32 VoiceIndex = VoiceHandle.GetIndex();

		check(Command.Arguments[1].Data.PtrVal != nullptr);
		FVoiceInitializationData* VoiceInitData = (FVoiceInitializationData*)Command.Arguments[1].Data.PtrVal;

		EVoicePlayingState::Type PlayType = EVoicePlayingState::SUSPENDED;
		int32 SuspendedDataVoiceIndex = INDEX_NONE;
		bool bPlayReal = false;
		float VolumeProduct = -1.0f;
		float VolumeAttenuation = -1.0f;

		// If there are any free real voice slots, then this is easy: we play it as a real voice
		if (NumRealVoices < Settings.MaxVoiceCount)
		{
			PlayType = EVoicePlayingState::PLAYING_REAL;
		}
		// ... otherwise we need to figure out what to do with the voice
		else
		{
			// Get the weighted priority of the new voice
			float WeightedPriority = 0.0f;
			ComputeInitialVolumeValues(VoiceInitData, VolumeProduct, VolumeAttenuation, WeightedPriority);
			check(VolumeProduct >= 0.0f && VolumeProduct <= 1.0f);

			// if any currently playing real voices have a lower priority than this voice, they need to become virtual (or suspended)
			const TArray<FSortedVoiceEntry>& SortedEntries = VolumeManager.GetSortedVoices();
			check(SortedEntries.Num() == (Settings.MaxVoiceCount + Settings.MaxVirtualVoiceCount));

			// Get the least-priority real voice entry 
			const FSortedVoiceEntry& LeastPriorityRealEntry = SortedEntries[Settings.MaxVoiceCount - 1];

			// If the new voice's weighted priority is higher than the least real voice...
			if (WeightedPriority > LeastPriorityRealEntry.PriorityWeightedVolume)
			{
				// ... then we're going to play the new voice as a real voice 
				PlayType = EVoicePlayingState::PLAYING_REAL;

				// Now we figure out  what to do with the real voice we're going to "steal"...

				// First, sanity check and make sure the least-pri playing voice playing at this index is indeed real
				check(PlayingStates[LeastPriorityRealEntry.Index] == EVoicePlayingState::PLAYING_REAL);

				// if we have any virtual voices at all...
				if (Settings.MaxVirtualVoiceCount > 0)
				{
					// ... and if we don't have any virtual voice slots left...
					if (NumVirtualVoices == Settings.MaxVirtualVoiceCount)
					{
						// ... then suspend the least priority virtual voice
						const FSortedVoiceEntry& LastVirtualEntry = SortedEntries[SortedEntries.Num() - 1];
						SuspendedDataVoiceIndex = LastVirtualEntry.Index;
					}

					// Now make the playing real voice virtual
					// TODO: un-register this voice as a real voice, and turn it virtual
					PlayingStates[LeastPriorityRealEntry.Index] = EVoicePlayingState::PLAYING_VIRTUAL;
				}
				else
				{
					// ... otherwise just suspend the lower-priority real voice
					SuspendedDataVoiceIndex = LeastPriorityRealEntry.Index;
				}
			}
			// ... else if we have any virtual voice slots at all...
			else if (Settings.MaxVirtualVoiceCount > 0)
			{
				// ... but if we don't have any free virtual voices left
				if (NumVirtualVoices == Settings.MaxVirtualVoiceCount)
				{
					// ... check the least priority virtual voice entry (i.e. the last entry)
					const FSortedVoiceEntry& LeastPriorityVirtualEntry = SortedEntries[SortedEntries.Num() - 1];

					// if the new voice has a higher priority than the last entry
					if (WeightedPriority > LeastPriorityVirtualEntry.PriorityWeightedVolume)
					{
						// ... then we're going to play that new voice as a virtual voice
						PlayType = EVoicePlayingState::PLAYING_VIRTUAL;

						// and suspend the last virtual voice entry
						SuspendedDataVoiceIndex = LeastPriorityRealEntry.Index;
					}
					// ... otherwise the new voice is lower priority than even the lowest-priority virtual voice
					else
					{
						// and needs to be suspended
						PlayType = EVoicePlayingState::SUSPENDED;
					}
				}
				// .. otherwise we must have room in our virtual voice allocation
				else
				{
					// But sanity check that we indeed have room left
					check(NumVirtualVoices < Settings.MaxVirtualVoiceCount);

					// Set the new voice to play as virtual
					PlayType = EVoicePlayingState::PLAYING_VIRTUAL;

					// No need to suspend anything...
				}
			}
			// ... otherwise, we'll have to immediately suspend the new voice
			else
			{
				PlayType = EVoicePlayingState::SUSPENDED;
			}
		}

		// If this new voice is immediately suspended...
		if (PlayType == EVoicePlayingState::SUSPENDED)
		{
			// ... send a message back to main thread of the fact
			AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_SUSPEND, VoiceHandle));
		}
		else
		{
			// We are playing this voice as either real or virtual and need to determine the voice data index
			uint32 VoiceDataIndex = INDEX_NONE;

			// First, if we had are suspending a virtual or real voice, we need to suspend it and take that index as the new index
			if (SuspendedDataVoiceIndex != INDEX_NONE)
			{
				// Get the voice handle at this index
				const FVoiceHandle& SuspendedVoiceHandle = VoiceHandles[VoiceIndex];

				// Release the voice index entries in the pitch and volume managers
				PitchManager.ReleaseEntry(VoiceIndex);
				VolumeManager.ReleaseEntry(VoiceIndex);

				// Remove this voice handle from the handle-to-index map
				HandleToIndex.Remove(SuspendedVoiceHandle.GetIndex());

				// ... and set the suspended voice index as the index of the new voice
				VoiceDataIndex = SuspendedDataVoiceIndex;

				// Send message to main that this voice was suspended
				AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_SUSPEND, SuspendedVoiceHandle));
			}
			else
			{
				// ... otherwise, we need to get a new voice data index. Since we didn't suspend anything,
				// and this is a real/virtual voice, then we need to get a new voice data index
				FreeDataIndicies.Dequeue(VoiceDataIndex);
			}

			// Sanity check that we got a new voice data index
			check(VoiceDataIndex != INDEX_NONE);

			// Add this VoiceDataIndex to the handle-to-index map
			HandleToIndex.Add(VoiceHandle.GetIndex(), VoiceDataIndex);

			// We might need to compute the volume product and the volume attenuation for the new voice
			if (VolumeProduct < 0.0f)
			{
				float WeightedPriority = 0.0f;
				ComputeInitialVolumeValues(VoiceInitData, VolumeProduct, VolumeAttenuation, WeightedPriority);
			}

			// Now we can just initialize the voice at the suspended index
			check(VolumeProduct >= 0.0f && VolumeAttenuation >= 0.0f);

			// Initialize data
			VoiceHandles[VoiceDataIndex] = VoiceHandle;
			PlayingStates[VoiceDataIndex] = PlayType;
			States[VoiceDataIndex] = EVoiceState::PLAYING;
			Flags[VoiceDataIndex] = VoiceInitData->VoiceFlags;

			FPitchInitParam PitchParams =
			{
				VoiceInitData->BaselinePitchScale,
				VoiceInitData->DynamicPitchScale,
				VoiceInitData->DynamicPitchTime,
				VoiceInitData->DurationSeconds
			};

			PitchManager.InitializeEntry(VoiceDataIndex, PitchParams);

			FVolumeInitParam VolumeParams =
			{
				VoiceInitData->EmitterHandle,
				VoiceInitData->BaselineVolumeScale,
				VoiceInitData->DynamicVolumeScale,
				VoiceInitData->DynamicVolumeTime,
				VolumeProduct,
				VolumeAttenuation,
				VoiceInitData->PriorityWeight
			};

			VolumeManager.InitializeEntry(VoiceDataIndex, VolumeParams);

			// Send a message to main thread that this voice is now playing...
			if (PlayType == EVoicePlayingState::PLAYING_REAL)
			{
				AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_REAL, VoiceHandle, VoiceDataIndex));
				//AudioModule->SendDeviceThreadCommand(FCommand(EDeviceThreadCommand::VOICE_REAL, VoiceHandle, VoiceDataIndex));
			}
			else
			{
				AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_VIRTUAL, VoiceHandle, VoiceDataIndex));
			}
		}
	}

	void FVoiceManager::PauseVoice(const FCommand& Command)
	{
		check(Command.NumArguments == 2);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);
		check(Command.Arguments[1].DataType == ECommandData::FLOAT_32);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		float FadeTimeSec = Command.Arguments[1].Data.Float32Val;

		uint32 VoiceDataIndex = GetVoiceDataIndex(VoiceHandle);

		States[VoiceDataIndex] = EVoiceState::PAUSING;
		VolumeManager.SetFadeOut(VoiceDataIndex, FadeTimeSec);
	}

	void FVoiceManager::StopVoice(const FCommand& Command)
	{
		check(Command.NumArguments == 2);
		check(Command.Arguments[0].DataType == ECommandData::HANDLE);
		check(Command.Arguments[1].DataType == ECommandData::FLOAT_32);

		FVoiceHandle VoiceHandle = Command.Arguments[0].Data.Handle;
		float FadeTimeSec = Command.Arguments[1].Data.Float32Val;

		uint32 VoiceDataIndex = GetVoiceDataIndex(VoiceHandle);
		States[VoiceDataIndex] = EVoiceState::STOPPING;
		VolumeManager.SetFadeOut(VoiceDataIndex, FadeTimeSec);
	}

	void FVoiceManager::SetVolumeScale(const FCommand& Command)
	{
		check(Command.NumArguments == 3);
		check(Command.Arguments[0].DataType == ECommandData::UINT32);
		check(Command.Arguments[1].DataType == ECommandData::FLOAT_32);
		check(Command.Arguments[2].DataType == ECommandData::FLOAT_32);

		uint32 VoiceDataIndex = Command.Arguments[0].Data.UnsignedInt32;
		float VolumeScale = Command.Arguments[1].Data.Float32Val;
		float DeltaTimeSeconds = Command.Arguments[2].Data.Float32Val;

		VolumeManager.SetDynamicVolumeScale(VoiceDataIndex, VolumeScale, DeltaTimeSeconds);
	}

	void FVoiceManager::SetPitchScale(const FCommand& Command)
	{
		check(Command.NumArguments == 3);
		check(Command.Arguments[0].DataType == ECommandData::UINT32);
		check(Command.Arguments[1].DataType == ECommandData::FLOAT_32);
		check(Command.Arguments[2].DataType == ECommandData::FLOAT_32);

		uint32 VoiceDataIndex = Command.Arguments[0].Data.UnsignedInt32;
		float PitchScale = Command.Arguments[1].Data.Float32Val;
		float DeltaTimeSeconds = Command.Arguments[2].Data.Float32Val;

		PitchManager.SetDynamicPitchScale(VoiceDataIndex, PitchScale, DeltaTimeSeconds);
	}

	void FVoiceManager::Update()
	{
		VolumeManager.Update();
		PitchManager.Update();

		// Handle any changes virtual/real voice changes after the update
		UpdateStates();
	}

	void FVoiceManager::UpdateStates()
	{
		//////////////////////////////////////////////////////////////////////////
		// Update virtual and real voices, swapping each other based on a sorted volume array
		//
		// 1) Skip not-playing voices since they aren't playing at all (i.e. they're paused or stopped)
		//
		// 2) Keep two separate counts, one count is the actual new real voice count after some voices
		//		have possibly turned from virtual to real. The other count is the old real-voice count.
		//		We use the previous playing sound count to early-out the iteration.
		//		The other counter is used to track the new real voice count. Once a voice which was
		//		previously real is found while the number of new voice counts are higher than
		//		the max voice count, that voice (and subsequent voices) will be virtualized.
		//
		//////////////////////////////////////////////////////////////////////////

		const TArray<FSortedVoiceEntry>& SortedVoiceEntries = VolumeManager.GetSortedVoices();

		// If there are no voices playing then there's nothing to do...
		if (SortedVoiceEntries.Num() == 0)
		{
			return;
		}

		// Keep track of how many real-playing voices were playing in the last update
		int32 PrevRealCount = 0;

		// Keep track of how many real-playing voices are going to be playing in the next update
		int32 NextRealCount = 0;

		// Loop through the highest-pri voice entries (from the front)
		for (const FSortedVoiceEntry& Entry : SortedVoiceEntries)
		{
			// Get the state of the voice
			EVoicePlayingState::Type PlayingState = PlayingStates[Entry.Index];
			check(PlayingState != EVoicePlayingState::NOT_PLAYING);

			// If it's a real voice...
			if (PlayingState == EVoicePlayingState::PLAYING_REAL)
			{
				// count this voice as a "next" real-voice
				++NextRealCount;

				// if the number of real voices for the next update is already greater than
				// the max number of voices, then we must suspend this voice entry
				if (NextRealCount >= Settings.MaxVoiceCount)
				{
					PlayingStates[Entry.Index] = EVoicePlayingState::PLAYING_VIRTUAL;

					// Notify the main thread that this voice has become virtual
					AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_VIRTUAL, VoiceHandles[Entry.Index]));

					// TODO: Remove this DataVoiceIndex from active playing sounds
				}

				if (++PrevRealCount == Settings.MaxVoiceCount)
				{
					break;
				}
			}
			// else if this is a virtual voice, then this must be a higher pri voice than one inside the highest voice pri's
			else if (PlayingState == EVoicePlayingState::PLAYING_VIRTUAL)
			{
				check(NextRealCount < Settings.MaxVoiceCount);

				// count this new voice as a real voice for the next update loop
				++NextRealCount;

				// Turn the virtual voice into a real voice
				PlayingStates[Entry.Index] = EVoicePlayingState::PLAYING_REAL;

				// Notify the main thread that this voice has become real
				AudioModule->SendMainThreadCommand(FCommand(EMainThreadCommand::VOICE_REAL, VoiceHandles[Entry.Index]));

				// TODO: ADD THIS STATE TO ACTIVE PLAYING SOUNDS AND SOUND OUTPUT
			}
		}
	}

	void FVoiceManager::ComputeInitialVolumeValues(const FVoiceInitializationData* VoiceInitData, float& OutVolumeProduct, float& OutVolumeAttenuation, float& OutWeightedPriority) const
	{
		// Initialize the volume product to the baseline volume scale
		float VolumeProduct = VoiceInitData->BaselineVolumeScale;

		// If the new volume-time for the volume-scale is 0.0, then we immediately factor the volume scale
		if (VoiceInitData->DynamicVolumeTime == 0.0f)
		{
			VolumeProduct *= VoiceInitData->DynamicVolumeScale;
		}

		// If this is a 3d voice, then we need to factor in attenuation
		OutVolumeAttenuation = 1.0f;
		if (VoiceInitData->VoiceFlags & EVoiceFlag::SPATIALIZED)
		{
			// TODO: Plug this into attenuation manager/code to get proper attenuation
			VolumeProduct *= OutVolumeAttenuation;
		}

		OutVolumeProduct = VolumeProduct;
		OutWeightedPriority = VoiceInitData->PriorityWeight * VolumeProduct;
	}

	uint32 FVoiceManager::GetVoiceDataIndex(const FVoiceHandle& VoiceHandle) const
	{
		uint32 VoiceIndex = VoiceHandle.GetIndex();
		const uint32 * VoiceDataIndex = HandleToIndex.Find(VoiceIndex);
		check(VoiceDataIndex != nullptr);
		return *VoiceDataIndex;
	}

	void FVoiceManager::Mix(struct FCallbackInfo& CallbackInfo)
	{

	}

}

#endif // #if ENABLE_UNREAL_AUDIO

