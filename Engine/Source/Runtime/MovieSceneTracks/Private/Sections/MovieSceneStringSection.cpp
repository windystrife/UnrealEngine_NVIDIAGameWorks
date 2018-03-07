// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneStringSection.h"
#include "SequencerObjectVersion.h"

UMovieSceneStringSection::UMovieSceneStringSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
}

/* IKeyframeSection interface
 *****************************************************************************/

FString UMovieSceneStringSection::Eval(float Time, const FString& DefaultString) const
{
	return StringCurve.Eval(Time, DefaultString);
}


/* IKeyframeSection interface
 *****************************************************************************/

bool UMovieSceneStringSection::NewKeyIsNewData(float Time, const FString& Key) const
{
	return (StringCurve.Eval(Time, FString()) != Key);
}


bool UMovieSceneStringSection::HasKeys(const FString& Key) const
{
	return (StringCurve.GetNumKeys() > 0);
}

void UMovieSceneStringSection::AddKey( float Time, const FString& Key, EMovieSceneKeyInterpolation KeyInterpolation )
{
	if (TryModify())
	{
		StringCurve.UpdateOrAddKey(Time, Key);
	}
}

void UMovieSceneStringSection::SetDefault(const FString& Value)
{
	if (StringCurve.DefaultValue.Compare(Value) != 0 && TryModify())
	{
		StringCurve.SetDefaultValue(Value);
	}
}

void UMovieSceneStringSection::ClearDefaults()
{
	StringCurve.ClearDefaultValue();
}


/* UMovieSceneSection interface
 *****************************************************************************/

void UMovieSceneStringSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	StringCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneStringSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(StringCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = StringCurve.GetKeyTime(It.Key());

		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneStringSection::GetKeyTime(FKeyHandle KeyHandle) const
{
	if (StringCurve.IsKeyHandleValid(KeyHandle))
	{
		return TOptional<float>(StringCurve.GetKeyTime(KeyHandle));
	}

	return TOptional<float>();
}


void UMovieSceneStringSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaPosition, KeyHandles);

	StringCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneStringSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	if (StringCurve.IsKeyHandleValid(KeyHandle))
	{
		StringCurve.SetKeyTime(KeyHandle, Time);
	}
}
