// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEnumSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"
#include "SequencerObjectVersion.h"

UMovieSceneEnumSection::UMovieSceneEnumSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
}


void UMovieSceneEnumSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	EnumCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneEnumSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	EnumCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneEnumSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(EnumCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = EnumCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneEnumSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( EnumCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( EnumCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneEnumSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( EnumCurve.IsKeyHandleValid( KeyHandle ) )
	{
		EnumCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneEnumSection::AddKey( float Time, const int64& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	if (TryModify())
	{
		EnumCurve.UpdateOrAddKey(Time, Value);
	}
}


bool UMovieSceneEnumSection::NewKeyIsNewData( float Time, const int64& Value ) const
{
	return EnumCurve.Evaluate( Time, Value ) != Value;
}


bool UMovieSceneEnumSection::HasKeys( const int64& Value ) const
{
	return EnumCurve.GetNumKeys() > 0;
}


void UMovieSceneEnumSection::SetDefault( const int64& Value )
{
	if (EnumCurve.GetDefaultValue() != Value && TryModify())
	{
		EnumCurve.SetDefaultValue(Value);
	}
}


void UMovieSceneEnumSection::ClearDefaults()
{
	EnumCurve.ClearDefaultValue();
}
