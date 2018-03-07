// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioVoice.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioPrivate.h"

#define UNREAL_AUDIO_VOICE_COMMAND_QUEUE_SIZE (50)

#if ENABLE_UNREAL_AUDIO

#define AUDIO_VOICE_CHECK_ERROR()					\
		if (VoiceState == EVoiceState::HAS_ERROR)	\
		{											\
			return LastError;						\
		}

#define AUDIO_VOICE_CHECK_SUSPEND()							\
		if (PlayingState == EVoicePlayingState::SUSPENDED)	\
		{													\
			return EVoiceError::NONE;						\
		}



namespace UAudio
{
	/************************************************************************/
	/* FVoice implementation												*/
	/************************************************************************/

	FVoice::FVoice(class FUnrealAudioModule* InParentModule, const FVoiceInitializationParams& InParams)
		: AudioModule(InParentModule)
		, PlayingState(EVoicePlayingState::NOT_PLAYING)
		, VoiceState(EVoiceState::STOPPED)
		, Params(InParams)
		, VoiceDataIndex(INDEX_NONE)
		, LastError(EVoiceError::NONE)
		, DurationSeconds(0.0f)
		, bPlayCalled(false)
	{
		check(Params.SoundFile.IsValid());

		// Get the duration right away
// 		const FSoundFileDescription& Description = Params.SoundFile->GetDescription();
// 		check(Description.SampleRate > 0);
// 		DurationSeconds = static_cast<float>(Description.NumFrames) / Description.SampleRate;
	}

	FVoice::~FVoice()
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex == INDEX_NONE);
	}

	EVoiceError::Type FVoice::Play()
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (VoiceState != EVoiceState::PLAYING)
		{
			// Make sure play is not called 2x on the same voice.
			bPlayCalled = true;

			// Build the initialization data for the play call
			FVoiceInitializationData* VoiceInitData = new FVoiceInitializationData();
			VoiceInitData->SoundFile			= Params.SoundFile;
			VoiceInitData->EmitterHandle		= Params.Emitter.IsValid() ? ((FEmitter*)Params.Emitter.Get())->GetHandle() : FEmitterHandle();
			VoiceInitData->BaselineVolumeScale	= Params.BaselineVolumeScale;
			VoiceInitData->DynamicVolumeScale	= PlayInfo.VolumeScale;
			VoiceInitData->DynamicVolumeTime	= PlayInfo.VolumeScaleTime;
			VoiceInitData->BaselinePitchScale	= Params.BaselinePitchScale;
			VoiceInitData->DynamicPitchScale	= PlayInfo.PitchScale;
			VoiceInitData->DynamicPitchTime		= PlayInfo.PitchScaletime;
			VoiceInitData->PriorityWeight		= Params.PriorityWeight;
			VoiceInitData->DurationSeconds		= DurationSeconds;

			VoiceInitData->VoiceFlags = EVoiceFlag::NONE;
			if (Params.bIsLooping)
			{
				VoiceInitData->VoiceFlags |= EVoiceFlag::LOOPING;
			}

			if (Params.Emitter.IsValid())
			{
				VoiceInitData->VoiceFlags |= EVoiceFlag::SPATIALIZED;
			}

			// Play the voice with the voice manager
			check(AudioModule != nullptr);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			EVoiceError::Type Result = VoiceManager.PlayVoice(this, VoiceInitData, VoiceHandle);

			// If nothing went wrong then our VoiceState is "playing"
			if (Result == EVoiceError::NONE)
			{
				VoiceState = EVoiceState::PLAYING;
			}
			else
			{
				VoiceState = EVoiceState::HAS_ERROR;
				LastError = Result;
			}
			return Result;
		}

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::Pause(float InFadeTimeSec /* = 0.0f */)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (VoiceState == EVoiceState::PLAYING)
		{
			if (InFadeTimeSec == 0.0f)
			{
				VoiceState = EVoiceState::PAUSED;
			}
			else
			{
				VoiceState = EVoiceState::PAUSING;
			}

			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return SetError(VoiceManager.PauseVoice(VoiceHandle, InFadeTimeSec));
		}
		else if (VoiceState == EVoiceState::STOPPED)
		{
			VoiceState = EVoiceState::PAUSED;
			if (VoiceDataIndex != INDEX_NONE)
			{
				FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
				return SetError(VoiceManager.PauseVoice(VoiceHandle, 0.0f));
			}
			else
			{
				return EVoiceError::NONE;
			}
		}

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::Stop(float InFadeTimeSec /* = 0.0f */)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (VoiceState != EVoiceState::STOPPED)
		{
			if (InFadeTimeSec == 0.0f)
			{
				VoiceState = EVoiceState::STOPPED;
			}
			else
			{
				VoiceState = EVoiceState::STOPPING;
			}

			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return SetError(VoiceManager.StopVoice(VoiceHandle, InFadeTimeSec));
		}

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::AddVoiceListener(IVoiceListener* Listener)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (VoiceListeners.Contains(Listener))
		{
			return EVoiceError::LISTENER_ALREADY_ADDED;
		}

		VoiceListeners.Add(Listener);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::RemoveVoiceListener(IVoiceListener* Listener)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (!VoiceListeners.Contains(Listener))
		{
			return EVoiceError::LISTENER_NOT_ADDED;
		}

		VoiceListeners.Remove(Listener);
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::SetVolumeScale(float InVolumeScale, float InDeltaTimeSec /*= 0.0f */)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (PlayInfo.VolumeScale != InVolumeScale || PlayInfo.VolumeScaleTime != InDeltaTimeSec)
		{
			PlayInfo.VolumeScale = InVolumeScale;
			PlayInfo.VolumeScaleTime = InDeltaTimeSec;

			if (IsPlaying())
			{
				DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
				FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
				return SetError(VoiceManager.SetVolumeScale(VoiceDataIndex, InVolumeScale, InDeltaTimeSec));
			}
		}
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::SetPitchScale(float InPitchScale, float InDeltaTimeSec /* = 0.0f */)
	{
		AUDIO_VOICE_CHECK_ERROR();
		AUDIO_VOICE_CHECK_SUSPEND();

		if (PlayInfo.PitchScale != InPitchScale || PlayInfo.PitchScaletime != InDeltaTimeSec)
		{
			PlayInfo.PitchScale = InPitchScale;
			PlayInfo.PitchScaletime = InDeltaTimeSec;
			if (IsPlaying())
			{
				DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
				FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
				return SetError(VoiceManager.SetPitchScale(VoiceDataIndex, InPitchScale, InDeltaTimeSec));
			}
		}
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetPitchScale(float& OutPitchScale) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (IsPlaying())
		{
			DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return VoiceManager.GetPitchScale(VoiceDataIndex, OutPitchScale);
		}
		OutPitchScale = 1.0f;
		return EVoiceError::VOICE_NOT_PLAYING;
	}

	EVoiceError::Type FVoice::GetBaselinePitchScale(float& OutBaselinePitchScale) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutBaselinePitchScale = Params.BaselinePitchScale;
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetPitchProduct(float& OutPitchProduct) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (IsPlaying())
		{
			DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return VoiceManager.GetPitchProduct(VoiceDataIndex, OutPitchProduct);
		}
		OutPitchProduct = 1.0f;
		return EVoiceError::VOICE_NOT_PLAYING;
	}

	EVoiceError::Type FVoice::GetVolumeScale(float& OutVolumeScale) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (IsPlaying())
		{
			DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return VoiceManager.GetVolumeScale(VoiceDataIndex, OutVolumeScale);
		}

		OutVolumeScale = 1.0f;
		return EVoiceError::VOICE_NOT_PLAYING;
	}

	EVoiceError::Type FVoice::GetBaselineVolumeScale(float& OutBaselineVolumeScale) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutBaselineVolumeScale = Params.BaselineVolumeScale;
		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetVolumeAttenuation(float& OutAttenuation) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (IsPlaying())
		{
			DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return VoiceManager.GetVolumeAttenuation(VoiceDataIndex, OutAttenuation);
		}

		OutAttenuation = 1.0f;
		return EVoiceError::VOICE_NOT_PLAYING;
	}

	EVoiceError::Type FVoice::GetVolumeProduct(float& OutVolumeProduct) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		if (IsPlaying())
		{
			DEBUG_AUDIO_CHECK(VoiceDataIndex != INDEX_NONE);
			FVoiceManager& VoiceManager = AudioModule->GetVoiceManager();
			return VoiceManager.GetVolumeProduct(VoiceDataIndex, OutVolumeProduct);
		}
		OutVolumeProduct = 1.0f;
		return EVoiceError::VOICE_NOT_PLAYING;
	}

	EVoiceError::Type FVoice::GetDuration(float& OutDurationSec)
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutDurationSec = DurationSeconds;

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetVoiceState(EVoiceState::Type& OutVoiceState) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutVoiceState = VoiceState;

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetPlayingState(EVoicePlayingState::Type& OutPlayingState) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutPlayingState = PlayingState;

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::GetId(uint32& OutVoiceId) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		OutVoiceId = VoiceHandle.Id;

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::IsValid(bool& bOutIsValid) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		bOutIsValid = AudioModule->GetVoiceManager().IsValidVoice(VoiceHandle);

		return EVoiceError::NONE;
	}

	EVoiceError::Type FVoice::IsPlaying(bool& bOutIsPlaying) const
	{
		AUDIO_VOICE_CHECK_ERROR();

		bOutIsPlaying = (PlayingState == EVoicePlayingState::PLAYING_REAL || PlayingState == EVoicePlayingState::PLAYING_VIRTUAL);

		return EVoiceError::NONE;
	}

	void FVoice::Init(TSharedPtr<FVoice> InThisObject)
	{
		ThisObject = InThisObject;
	}

	void FVoice::NotifyPlayReal(uint32 InVoiceDataIndex)
	{
		DEBUG_AUDIO_CHECK(InVoiceDataIndex != INDEX_NONE);
		VoiceDataIndex = InVoiceDataIndex;
		PlayingState = EVoicePlayingState::PLAYING_REAL;

		for (int32 i = 0; i < VoiceListeners.Num(); ++i)
		{
			VoiceListeners[i]->OnVoiceReal(this);
		}
	}

	void FVoice::NotifyPlayVirtual(uint32 InVoiceDataIndex)
	{
		DEBUG_AUDIO_CHECK(InVoiceDataIndex != INDEX_NONE);
		VoiceDataIndex = InVoiceDataIndex;
		PlayingState = EVoicePlayingState::PLAYING_VIRTUAL;

		for (int32 i = 0; i < VoiceListeners.Num(); ++i)
		{
			VoiceListeners[i]->OnVoiceVirtual(this);
		}
	}

	void FVoice::NotifyDone()
	{
		VoiceDataIndex = INDEX_NONE;
		PlayingState = EVoicePlayingState::NOT_PLAYING;

		for (int32 i = 0; i < VoiceListeners.Num(); ++i)
		{
			VoiceListeners[i]->OnVoiceDone(this);
		}
	}

	void FVoice::NotifySuspend()
	{
		VoiceDataIndex = INDEX_NONE;
		PlayingState = EVoicePlayingState::NOT_PLAYING;

		for (int32 i = 0; i < VoiceListeners.Num(); ++i)
		{
			VoiceListeners[i]->OnVoiceSuspend(this);
		}
	}

	void FVoice::GetHandle(FVoiceHandle& OutVoiceHandle) const
	{
		OutVoiceHandle = VoiceHandle;
	}

	EVoiceError::Type FVoice::SetError(EVoiceError::Type Error)
	{
		if (Error != EVoiceError::NONE)
		{
			LastError = Error;
			VoiceState = EVoiceState::HAS_ERROR;
		}
		return Error;
	}

	bool FVoice::IsPlaying() const
	{
		return PlayingState == EVoicePlayingState::PLAYING_REAL || PlayingState == EVoicePlayingState::PLAYING_VIRTUAL;
	}

}

#endif // #if ENABLE_UNREAL_AUDIO

