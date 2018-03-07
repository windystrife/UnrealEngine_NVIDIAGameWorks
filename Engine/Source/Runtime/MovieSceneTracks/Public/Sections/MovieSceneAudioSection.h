// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Runtime/Engine/Classes/Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "MovieSceneAudioSection.generated.h"

class USoundBase;

/**
 * Audio section, for use in the master audio, or by attached audio objects
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneAudioSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets this section's sound */
	void SetSound(class USoundBase* InSound) {Sound = InSound;}

	/** Gets the sound for this section */
	class USoundBase* GetSound() const {return Sound;}
	
	/** Set the offset into the beginning of the audio clip */
	void SetStartOffset(float InStartOffset) {StartOffset = InStartOffset;}
	
	/** Get the offset into the beginning of the audio clip */
	float GetStartOffset() const {return StartOffset;}
	
	/**
	 * @return The range of times that the sound plays, truncated by the section limits
	 */
	TRange<float> GetAudioRange() const;
	
	DEPRECATED(4.15, "Audio true range no longer supported.")
	TRange<float> GetAudioTrueRange() const;
	
	/**
	 * Gets the sound volume curve
	 *
	 * @return The rich curve for this sound volume
	 */
	FRichCurve& GetSoundVolumeCurve() { return SoundVolume; }
	const FRichCurve& GetSoundVolumeCurve() const { return SoundVolume; }

	/**
	 * Gets the sound pitch curve
	 *
	 * @return The rich curve for this sound pitch
	 */
	FRichCurve& GetPitchMultiplierCurve() { return PitchMultiplier; }
	const FRichCurve& GetPitchMultiplierCurve() const { return PitchMultiplier; }

	/**
	 * Return the sound volume
	 *
	 * @param InTime	The position in time within the movie scene
	 * @return The volume the sound will be played with.
	 */
	float GetSoundVolume(float InTime) const { return SoundVolume.Eval(InTime); }

	/**
	 * Sets the sound volume
	 *
	 * @param InTime	The position in time within the movie scene
	 * @param InVolume	The volume to set
	 */
	void SetSoundVolume(float InTime, float InVolume) { SoundVolume.AddKey(InTime, InVolume); }

	/**
	 * Return the pitch multiplier
	 *
	 * @param Position	The position in time within the movie scene
	 * @return The pitch multiplier the sound will be played with.
	 */
	float GetPitchMultiplier(float InTime) const { return PitchMultiplier.Eval(InTime); }

	/**
	 * Set the pitch multiplier
	 *
	 * @param Position	The position in time within the movie scene
	 * @param InPitch The pitch multiplier to set
	 */
	void SetPitchMultiplier(float InTime, float InPitchMultiplier) { PitchMultiplier.AddKey(InTime, InPitchMultiplier); }

	/**
	 * Returns whether or not a provided position in time is within the timespan of the audio range
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinAudioRange( float Position ) const 
	{
		TRange<float> AudioRange = GetAudioRange();
		return Position >= AudioRange.GetLowerBoundValue() && Position <= AudioRange.GetUpperBoundValue();
	}

	/**
	 * @return Whether subtitles should be suppressed
	 */
	bool GetSuppressSubtitles() const
	{
		return bSuppressSubtitles;
	}

	/**
	 * @return Whether override settings on this section should be used
	 */
	bool GetOverrideAttenuation() const
	{
		return bOverrideAttenuation;
	}

	/**
	 * @return The attenuation settings
	 */
	USoundAttenuation* GetAttenuationSettings() const
	{
		return AttenuationSettings;
	}

	/** ~UObject interface */
	virtual void PostLoad() override;

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	void SetOnQueueSubtitles(const FOnQueueSubtitles& InOnQueueSubtitles)
	{
		OnQueueSubtitles = InOnQueueSubtitles;
	}

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	const FOnQueueSubtitles& GetOnQueueSubtitles() const
	{
		return OnQueueSubtitles;
	}

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	void SetOnAudioFinished(const FOnAudioFinished& InOnAudioFinished)
	{
		OnAudioFinished = InOnAudioFinished;
	}

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	const FOnAudioFinished& GetOnAudioFinished() const
	{
		return OnAudioFinished;
	}
	
	void SetOnAudioPlaybackPercent(const FOnAudioPlaybackPercent& InOnAudioPlaybackPercent)
	{
		OnAudioPlaybackPercent = InOnAudioPlaybackPercent;
	}

	const FOnAudioPlaybackPercent& GetOnAudioPlaybackPercent() const
	{
		return OnAudioPlaybackPercent;
	}

public:

	// MovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual UMovieSceneSection* SplitSection(float SplitTime) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<float> GetOffsetTime() const override { return TOptional<float>(StartOffset); }
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override { return TOptional<float>(); }
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override { }
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;

private:

	/** The sound cue or wave that this section plays */
	UPROPERTY(EditAnywhere, Category="Audio")
	USoundBase* Sound;

	/** The offset into the beginning of the audio clip */
	UPROPERTY(EditAnywhere, Category="Audio")
	float StartOffset;

	/** The absolute time that the sound starts playing at */
	UPROPERTY( )
	float AudioStartTime_DEPRECATED;
	
	/** The amount which this audio is time dilated by */
	UPROPERTY( )
	float AudioDilationFactor_DEPRECATED;

	/** The volume the sound will be played with. */
	UPROPERTY( )
	float AudioVolume_DEPRECATED;

	/** The volume the sound will be played with. */
	UPROPERTY( EditAnywhere, Category = "Audio" )
	FRichCurve SoundVolume;

	/** The pitch multiplier the sound will be played with. */
	UPROPERTY( EditAnywhere, Category = "Audio" )
	FRichCurve PitchMultiplier;

	UPROPERTY( EditAnywhere, Category="Audio" )
	bool bSuppressSubtitles;

	/** Should the attenuation settings on this section be used. */
	UPROPERTY( EditAnywhere, Category="Attenuation" )
	bool bOverrideAttenuation;

	/** The attenuation settings to use. */
	UPROPERTY( EditAnywhere, Category="Attenuation" )
	class USoundAttenuation* AttenuationSettings;

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	UPROPERTY()
	FOnQueueSubtitles OnQueueSubtitles;

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	UPROPERTY()
	FOnAudioFinished OnAudioFinished;

	UPROPERTY()
	FOnAudioPlaybackPercent OnAudioPlaybackPercent;
};
