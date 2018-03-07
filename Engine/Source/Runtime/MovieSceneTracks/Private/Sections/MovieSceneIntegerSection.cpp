// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneIntegerSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"
#include "SequencerObjectVersion.h"

UMovieSceneIntegerSection::UMovieSceneIntegerSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
	BlendType = EMovieSceneBlendType::Absolute;
}


void UMovieSceneIntegerSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	IntegerCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneIntegerSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	IntegerCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneIntegerSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(IntegerCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = IntegerCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneIntegerSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( IntegerCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( IntegerCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneIntegerSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( IntegerCurve.IsKeyHandleValid( KeyHandle ) )
	{
		IntegerCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneIntegerSection::AddKey( float Time, const int32& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	if (TryModify())
	{
		IntegerCurve.UpdateOrAddKey(Time, Value);
	}
}


bool UMovieSceneIntegerSection::NewKeyIsNewData( float Time, const int32& Value ) const
{
	return IntegerCurve.Evaluate( Time, Value ) != Value;
}


bool UMovieSceneIntegerSection::HasKeys( const int32& Value ) const
{
	return IntegerCurve.GetNumKeys() > 0;
}


void UMovieSceneIntegerSection::SetDefault( const int32& Value )
{
	if (IntegerCurve.GetDefaultValue() != Value && TryModify())
	{
		IntegerCurve.SetDefaultValue(Value);
	}
}


void UMovieSceneIntegerSection::ClearDefaults()
{
	IntegerCurve.ClearDefaultValue();
}
