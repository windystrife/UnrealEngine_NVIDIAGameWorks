// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioEmitterInternal.h"

class Error;

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	* EVoiceFlag
	* Flags used in a voice.
	*/
	namespace EVoiceFlag
	{
		enum Type
		{
			NONE		= 0,
			LOOPING		= (1 << 0),
			SPATIALIZED = (1 << 1),
		};
	}

	/** 
	* FVoice
	* Internal concrete implementation of IVoice
	*/
	class FVoice : public IVoice
	{
	public:
		FVoice(class FUnrealAudioModule* InParentModule, const FVoiceInitializationParams& InParams);
		~FVoice() override;

		/************************************************************************/
		/* IVoice																*/
		/************************************************************************/

		EVoiceError::Type Play() override;
		EVoiceError::Type Pause(float InFadeTimeSec = 0.0f) override;
		EVoiceError::Type Stop(float InFadeTimeSec = 0.0f) override;

		EVoiceError::Type AddVoiceListener(IVoiceListener* Listener) override;
		EVoiceError::Type RemoveVoiceListener(IVoiceListener* Listener) override;

		EVoiceError::Type SetVolumeScale(float InVolumeLinear, float InFadeTimeSec = 0.0f) override;
		EVoiceError::Type SetPitchScale(float InPitchScale, float InFadeTimeSec = 0.0f) override;

		EVoiceError::Type GetPitchScale(float& OutPitchScale) const override;
		EVoiceError::Type GetBaselinePitchScale(float& OutBaselinePitchScale) const override;
		EVoiceError::Type GetPitchProduct(float& OutPitchProduct) const override;
		EVoiceError::Type GetVolumeScale(float& OutVolumeScale) const override;
		EVoiceError::Type GetBaselineVolumeScale(float& OutBaselineVolumeScale) const override;
		EVoiceError::Type GetVolumeAttenuation(float& OutAttenuation) const override;
		EVoiceError::Type GetVolumeProduct(float& OutVolumeProduct) const override;
		EVoiceError::Type GetDuration(float& OutDurationSec) override;
		EVoiceError::Type GetVoiceState(EVoiceState::Type& OutVoicestate) const override;
		EVoiceError::Type GetPlayingState(EVoicePlayingState::Type& OutPlayingState) const override;
		EVoiceError::Type GetId(uint32& OutVoiceId) const override;

		EVoiceError::Type IsValid(bool& bOutIsValid) const override;
		EVoiceError::Type IsPlaying(bool& OutIsplaying) const override;

		/************************************************************************/
		/* Public internal API													*/
		/************************************************************************/

		void Init(TSharedPtr<FVoice> InThisObject);
		void NotifyPlayReal(uint32 InVoiceDataIndex);
		void NotifyPlayVirtual(uint32 InVoiceDataIndex);
		void NotifyDone();
		void NotifySuspend();
		void GetHandle(FVoiceHandle& OutHandle) const;

	private:
		EVoiceError::Type SetError(EVoiceError::Type Error);
		bool IsPlaying() const;

		struct FPlayVoiceInfo
		{
			float VolumeScale;
			float VolumeScaleTime;
			float PitchScale;
			float PitchScaletime;

			FPlayVoiceInfo()
				: VolumeScale(1.0f)
				, VolumeScaleTime(0.0f)
				, PitchScale(1.0f)
				, PitchScaletime(0.0f)
			{}
		};

		FUnrealAudioModule*			AudioModule;
		EVoicePlayingState::Type	PlayingState;
		EVoiceState::Type			VoiceState;
		FVoiceInitializationParams	Params;
		FPlayVoiceInfo				PlayInfo;
		TArray<IVoiceListener*>		VoiceListeners;
		FVoiceHandle				VoiceHandle;

		// index into the real/virtual voice data array
		uint32						VoiceDataIndex;

		TSharedPtr<FVoice>			ThisObject;

		EVoiceError::Type			LastError;
		float						DurationSeconds;
		bool						bPlayCalled;
	};


}

#endif // #if ENABLE_UNREAL_AUDIO
