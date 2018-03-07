// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Runtime/Engine/Classes/Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "MovieSceneAudioTemplate.generated.h"

class UAudioComponent;
class UMovieSceneAudioSection;
class USoundBase;

USTRUCT()
struct FMovieSceneAudioSectionTemplateData
{
	GENERATED_BODY()

	FMovieSceneAudioSectionTemplateData() {}
	FMovieSceneAudioSectionTemplateData(const UMovieSceneAudioSection& Section);

	/** Ensure that the sound is playing for the specified audio component and data */
	void EnsureAudioIsPlaying(UAudioComponent& AudioComponent, FPersistentEvaluationData& PersistentData, const FMovieSceneContext& Context, bool bAllowSpatialization, IMovieScenePlayer& Player) const;

	/** The sound cue or wave that this template plays. Not to be dereferenced on a background thread */
	UPROPERTY()
	USoundBase* Sound;

	/** The offset into the beginning of the audio clip */
	UPROPERTY()
	float AudioStartOffset;
	
	/** The time range this audio plays */
	UPROPERTY()
	FFloatRange AudioRange;

	/** The amount which this audio is time dilated by */
	UPROPERTY()
	FRichCurve AudioPitchMultiplierCurve;

	/** The volume the sound will be played with. */
	UPROPERTY()
	FRichCurve AudioVolumeCurve;

	/** The row index of the section */
	UPROPERTY()
	int32 RowIndex;

	/** Should the attenuation settings on this section be used. */
	UPROPERTY()
	bool bOverrideAttenuation;

	/** The attenuation settings */
	UPROPERTY()
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

USTRUCT()
struct FMovieSceneAudioSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneAudioSectionTemplate() {}
	FMovieSceneAudioSectionTemplate(const UMovieSceneAudioSection& Section);

	UPROPERTY()
	FMovieSceneAudioSectionTemplateData AudioData;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
