// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneFloatSection.h"
#include "SequencerObjectVersion.h"

UMovieSceneFloatSection::UMovieSceneFloatSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
	BlendType = EMovieSceneBlendType::Absolute;
}


float UMovieSceneFloatSection::Eval( float Position, float DefaultValue ) const
{
	return FloatCurve.Eval( Position, DefaultValue );
}


void UMovieSceneFloatSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
	FloatCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneFloatSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	FloatCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneFloatSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(FloatCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = FloatCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneFloatSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( FloatCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( FloatCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneFloatSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( FloatCurve.IsKeyHandleValid( KeyHandle ) )
	{
		FloatCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneFloatSection::AddKey( float Time, const float& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	AddKeyToCurve( FloatCurve, Time, Value, KeyInterpolation );
}


bool UMovieSceneFloatSection::NewKeyIsNewData(float Time, const float& Value) const
{
	return FMath::IsNearlyEqual( FloatCurve.Eval( Time ), Value ) == false;
}

bool UMovieSceneFloatSection::HasKeys( const float& Value ) const
{
	return FloatCurve.GetNumKeys() > 0;
}

void UMovieSceneFloatSection::SetDefault( const float& Value )
{
	SetCurveDefault( FloatCurve, Value );
}


void UMovieSceneFloatSection::ClearDefaults()
{
	FloatCurve.ClearDefaultValue();
}
