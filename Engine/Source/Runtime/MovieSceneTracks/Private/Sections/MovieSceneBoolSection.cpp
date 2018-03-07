// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneBoolSection.h"
#include "SequencerObjectVersion.h"

UMovieSceneBoolSection::UMovieSceneBoolSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, DefaultValue_DEPRECATED(false)
{
	SetIsInfinite(true);

	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
}

void UMovieSceneBoolSection::PostLoad()
{
	if (GetCurve().GetDefaultValue() == MAX_int32 && DefaultValue_DEPRECATED)
	{
		GetCurve().SetDefaultValue(DefaultValue_DEPRECATED);
	}
	Super::PostLoad();
}

bool UMovieSceneBoolSection::Eval( float Position, bool DefaultValue ) const
{
	return !!BoolCurve.Evaluate(Position, DefaultValue);
}


void UMovieSceneBoolSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	BoolCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneBoolSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	BoolCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneBoolSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(BoolCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = BoolCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneBoolSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( BoolCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( BoolCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneBoolSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( BoolCurve.IsKeyHandleValid( KeyHandle ) )
	{
		BoolCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneBoolSection::AddKey( float Time, const bool& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	if (TryModify())
	{
		BoolCurve.UpdateOrAddKey(Time, Value ? 1 : 0);
	}
}


void UMovieSceneBoolSection::SetDefault( const bool& Value )
{
	uint8 NewValue = Value ? 1 : 0;
	if (BoolCurve.GetDefaultValue() != NewValue && TryModify())
	{
		BoolCurve.SetDefaultValue(NewValue);
	}
}


bool UMovieSceneBoolSection::NewKeyIsNewData( float Time, const bool& Value ) const
{
	return Eval(Time, Value) != Value;
}

bool UMovieSceneBoolSection::HasKeys( const bool& Value ) const
{
	return BoolCurve.GetNumKeys() != 0;
}

void UMovieSceneBoolSection::ClearDefaults()
{
	BoolCurve.ClearDefaultValue();
}
