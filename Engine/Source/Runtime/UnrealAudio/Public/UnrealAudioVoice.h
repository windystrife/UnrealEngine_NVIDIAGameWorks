// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioSoundFile.h"
#include "UnrealAudioEmitter.h"

namespace UAudio
{
	namespace EVoiceError
	{
		enum Type
		{
			NONE = 0,
			INVALID_HANDLE,
			VOICE_NOT_PLAYING,
			ALREADY_INITIALIZED,
			NOT_INITIALIZED,
			INVALID_ARGUMENTS,
			LISTENER_ALREADY_ADDED,
			LISTENER_NOT_ADDED,
			UNKNOWN,
		};

		inline const TCHAR* ToString(EVoiceError::Type VoiceError)
		{
			switch (VoiceError)
			{
				case NONE:					return TEXT("NONE");

				default:
				case UNKNOWN:				return TEXT("UNKNOWN");
			}
		}
	}

	namespace EVoiceState
	{
		enum Type
		{
			UNINITIALIZED = 0,
			STOPPED,
			PAUSED,
			PLAYING,
			STOPPING,
			PAUSING,
			HAS_ERROR
		};

		inline const TCHAR* ToString(EVoiceState::Type VoiceState)
		{
			switch (VoiceState)
			{
				case UNINITIALIZED:		return TEXT("UNINITIALIZED");
				case STOPPED:			return TEXT("STOPPED");
				case PAUSED:			return TEXT("PAUSED");
				case PLAYING:			return TEXT("PLAYING");
				case STOPPING:			return TEXT("STOPPING");
				case PAUSING:			return TEXT("PAUSING");
				case HAS_ERROR:			return TEXT("HAS_ERROR");
			}
		}
	}

	namespace EVoicePlayingState
	{
		enum Type
		{
			NOT_PLAYING = 0,
			PLAYING_REAL,
			PLAYING_VIRTUAL,
			SUSPENDED,
		};

		inline const TCHAR* ToString(EVoicePlayingState::Type PlayingState)
		{
			switch (PlayingState)
			{
				case NOT_PLAYING:		return TEXT("NOT_PLAYING");
				case PLAYING_REAL:		return TEXT("PLAYING_REAL");
				case PLAYING_VIRTUAL:	return TEXT("PLAYING_VIRTUAL");
				case SUSPENDED:			return TEXT("SUSPENDED");
			}
		}
	}

	/** A voice initialization parameters. */
	struct FVoiceInitializationParams
	{
		/** Sound file to use with the voice. */
		TSharedPtr<ISoundFile> SoundFile;

		/** Optional emitter to use with the voice. */
		TSharedPtr<IEmitter> Emitter;

		/** Baseline volume scale to use with this voice. */
		float BaselineVolumeScale;

		/** Baseline pitch scale to use with this voice. */
		float BaselinePitchScale;

		/** A priority weight value, used to determine voice stealing. */
		float PriorityWeight;

		/** Whether or not to loop this sound. */
		bool bIsLooping;

		/** Default constructor. */
		FVoiceInitializationParams()
			: SoundFile(nullptr)
			, Emitter(nullptr)
			, BaselineVolumeScale(1.0f)
			, BaselinePitchScale(1.0f)
			, PriorityWeight(1.0f)
			, bIsLooping(false)
		{}

		/**
		 * Constructor.
		 *
		 * @param	Other	Copy constructor.
		 */

		FVoiceInitializationParams(const FVoiceInitializationParams& Other)
			: SoundFile(Other.SoundFile)
			, Emitter(Other.Emitter)
			, BaselineVolumeScale(Other.BaselineVolumeScale)
			, BaselinePitchScale(Other.BaselinePitchScale)
			, PriorityWeight(Other.PriorityWeight)
			, bIsLooping(Other.bIsLooping)
		{}
	};

	/** An object interface for listening to voice events. */
	class UNREALAUDIO_API IVoiceListener
	{
	public:

		/**
		 * Called when a voice finishes playing or is stopped.
		 *
		 * @param	Voice	The voice object that finished playing a sound.
		 */

		virtual void OnVoiceDone(class IVoice* Voice) {}

		/**
		 * Called when a voice becomes virtual (i.e. can't be played because to many real voices are playing)
		 * 
		 * @param	Voice	The voice that became virtual.
		 */

		virtual void OnVoiceVirtual(class IVoice* Voice) {}

		/**
		 * Called when a voice becomes real (after it was virtual)
		 *
		 * @param	Voice	The voice that became real.
		 */

		virtual void OnVoiceReal(class IVoice* Voice) {}


		virtual void OnVoiceSuspend(class IVoice* Voice) {}

	};

	/** An IVoice interface */
	class UNREALAUDIO_API IVoice
	{
	public:
		/** Destructor. */
		virtual ~IVoice() {}

		/** Adds the given notifier to the voice. The notifier will receive callbacks on various voice events. */
		virtual EVoiceError::Type AddVoiceListener(IVoiceListener* Listener) = 0;

		/**
		 * Removes the voice listener described by Listener.
		 *
		 * @param [in]	Listener	The IVoiceListener to remove.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type RemoveVoiceListener(IVoiceListener* Listener) = 0;

		/**
		 * Sets the volume scale for this voice using linear volume scale.
		 *
		 * @param	InVolumeLinear	Linear volume set this voice to.
		 * @param	InFadeTimeSec 	The time to take to get to the given volume scale from its current
		 * 							volume scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type SetVolumeScale(float InVolumeLinear, float InFadeTimeSec = 0.0f) = 0;

		/**
		 * Gets volume scale.
		 *
		 * @param [in,out]	OutVolumeScale	The volume scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetVolumeScale(float& OutVolumeScale) const = 0;

		/**
		 * Gets baseline volume scale.
		 *
		 * @param [out]	OutBaselineVolumeScale	The baseline volume scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetBaselineVolumeScale(float& OutBaselineVolumeScale) const = 0;

		/**
		 * Gets volume attenuation (due to 3d spatialization).
		 *
		 * @param [in,out]	OutAttenuation	The out attenuation.
		 *
		 * @return	The volume attenuation.
		 */

		virtual EVoiceError::Type GetVolumeAttenuation(float& OutAttenuation) const = 0;

		/**
		 * Gets the volume product.
		 *
		 * @param [out]	OutVolumeProduct	The volume product.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetVolumeProduct(float& OutVolumeProduct) const = 0;

		/**
		 * Sets the pitch scale for this voice using linear volume scale.
		 *
		 * @param	InPitchScale 	Pitch scale to set for this voice.
		 * @param	InFadeTimeSec	The time to scale the pitch to the given pitch scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type SetPitchScale(float InPitchScale, float InFadeTimeSec = 0.0f) = 0;

		/**
		 * Gets dynamic pitch scale of this voice.
		 *
		 * @param [out]	OutPitchScale	The pitch scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetPitchScale(float& OutPitchScale) const = 0;

		/**
		 * Gets the baseline pitch scale of this voice.
		 *
		 * @param [out]	OutBaselinePitchScale	The out baseline pitch scale.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetBaselinePitchScale(float& OutBaselinePitchScale) const = 0;

		/**
		 * Gets the pitch product.
		 *
		 * @param [in,out]	OutPitchProduct	The out pitch product.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type GetPitchProduct(float& OutPitchProduct) const = 0;

		/**
		 * Plays this voice.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type Play() = 0;

		/**
		 * Pauses this voice.
		 *
		 * @param	InFadeTimeSec	(Optional) The amount of time to spend fading out this sound before pausing it.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type Pause(float InFadeTimeSec = 0.0f) = 0;

		/**
		 * Stops this voice.
		 *
		 * @param	InFadeTimeSec	The amount of time to spend fading out this sound.
		 * 
		 * @note The voice is automatically released (i.e. freed) when the voice stops.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type Stop(float InFadeTimeSec = 0.0f) = 0;

		/**
		 * Gets a duration.
		 *
		 * @param [out]	OutDurationSec	The out duration security.
		 *
		 * @return	The duration.
		 */

		virtual EVoiceError::Type GetDuration(float& OutDurationSec) = 0;

		/**
		 * Is valid.
		 *
		 * @param [out]	bOutIsValid	The out is valid.
		 *
		 * @return	An EVoiceError::Type.
		 */

		virtual EVoiceError::Type IsValid(bool& bOutIsValid) const = 0;


		virtual EVoiceError::Type GetVoiceState(EVoiceState::Type& OutVoicestate) const = 0;
		virtual EVoiceError::Type GetPlayingState(EVoicePlayingState::Type& OutPlayingState) const = 0;
		virtual EVoiceError::Type IsPlaying(bool& OutIsplaying) const = 0;
		virtual EVoiceError::Type GetId(uint32& OutVoiceId) const = 0;

	};
}

