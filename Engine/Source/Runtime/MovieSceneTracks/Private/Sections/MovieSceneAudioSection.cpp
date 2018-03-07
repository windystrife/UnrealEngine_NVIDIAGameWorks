// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundBase.h"
#include "Evaluation/MovieSceneAudioTemplate.h"

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Sound = nullptr;
	StartOffset = 0.f;
	AudioStartTime_DEPRECATED = 0.f;
	AudioDilationFactor_DEPRECATED = 1.f;
	AudioVolume_DEPRECATED = 1.f;
	SoundVolume.SetDefaultValue(1.f);
	PitchMultiplier.SetDefaultValue(1.f);
	bSuppressSubtitles = false;
	bOverrideAttenuation = false;

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}

FMovieSceneEvalTemplatePtr UMovieSceneAudioSection::GenerateTemplate() const
{
	return FMovieSceneAudioSectionTemplate(*this);
}

TRange<float> UMovieSceneAudioSection::GetAudioRange() const
{
	if (!Sound)
	{
		return TRange<float>::Empty();
	}

	float AudioRangeStartTime = GetStartTime();
	if (StartOffset < 0)
	{
		AudioRangeStartTime += FMath::Abs(StartOffset);
	}

	float AudioRangeEndTime = FMath::Min(AudioRangeStartTime + Sound->GetDuration() * PitchMultiplier.GetDefaultValue(), GetEndTime());

	return TRange<float>(AudioRangeStartTime, AudioRangeEndTime);
}

void UMovieSceneAudioSection::PostLoad()
{
	Super::PostLoad();

	if (AudioDilationFactor_DEPRECATED != FLT_MAX)
	{
		PitchMultiplier.SetDefaultValue(AudioDilationFactor_DEPRECATED);

		AudioDilationFactor_DEPRECATED = FLT_MAX;
	}

	if (AudioVolume_DEPRECATED != FLT_MAX)
	{
		SoundVolume.SetDefaultValue(AudioVolume_DEPRECATED);

		AudioVolume_DEPRECATED = FLT_MAX;
	}

	if (AudioStartTime_DEPRECATED != FLT_MAX)
	{
		// Previously, start time in relation to the sequence. Start time was used to calculate the offset into the 
		// clip at the start of the section evaluation as such: Section Start Time - Start Time. 
		if (AudioStartTime_DEPRECATED != 0.f)
		{
			StartOffset = GetStartTime() - AudioStartTime_DEPRECATED;
		}
		AudioStartTime_DEPRECATED = FLT_MAX;
	}
}

void UMovieSceneAudioSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection(DeltaTime, KeyHandles);

	SoundVolume.ShiftCurve(DeltaTime, KeyHandles);
	PitchMultiplier.ShiftCurve(DeltaTime, KeyHandles);
}


void UMovieSceneAudioSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	SoundVolume.ScaleCurve(Origin, DilationFactor, KeyHandles);
	PitchMultiplier.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


UMovieSceneSection* UMovieSceneAudioSection::SplitSection(float SplitTime)
{
	float NewOffset = StartOffset + (SplitTime - GetStartTime());

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(NewSection);
		NewAudioSection->StartOffset = NewOffset;
	}
	return NewSection;
}

void UMovieSceneAudioSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(SoundVolume.GetKeyHandleIterator()); It; ++It)
	{
		float Time = SoundVolume.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}

	for (auto It(PitchMultiplier.GetKeyHandleIterator()); It; ++It)
	{
		float Time = PitchMultiplier.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


void UMovieSceneAudioSection::GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	// @todo Sequencer handle snapping for time dilation
	// @todo Don't add redundant times (can't use AddUnique due to floating point equality issues)
}
