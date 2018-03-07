// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginSection.h"

UMovieSceneMarginSection::UMovieSceneMarginSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	BlendType = EMovieSceneBlendType::Absolute;
}

void UMovieSceneMarginSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaTime, KeyHandles );

	// Move all the curves in this section
	LeftCurve.ShiftCurve(DeltaTime, KeyHandles);
	TopCurve.ShiftCurve(DeltaTime, KeyHandles);
	RightCurve.ShiftCurve(DeltaTime, KeyHandles);
	BottomCurve.ShiftCurve(DeltaTime, KeyHandles);
}

void UMovieSceneMarginSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	LeftCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	TopCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	RightCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	BottomCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}

void UMovieSceneMarginSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(LeftCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = LeftCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
	for (auto It(TopCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = TopCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
	for (auto It(RightCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = RightCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
	for (auto It(BottomCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = BottomCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneMarginSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( LeftCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( LeftCurve.GetKeyTime( KeyHandle ) );
	}
	if ( TopCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( TopCurve.GetKeyTime( KeyHandle ) );
	}
	if ( RightCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( RightCurve.GetKeyTime( KeyHandle ) );
	}
	if ( BottomCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( BottomCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneMarginSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( LeftCurve.IsKeyHandleValid( KeyHandle ) )
	{
		LeftCurve.SetKeyTime( KeyHandle, Time );
	}
	else if ( TopCurve.IsKeyHandleValid( KeyHandle ) )
	{
		TopCurve.SetKeyTime( KeyHandle, Time );
	}
	else if ( RightCurve.IsKeyHandleValid( KeyHandle ) )
	{
		RightCurve.SetKeyTime( KeyHandle, Time );
	}
	else if ( BottomCurve.IsKeyHandleValid( KeyHandle ) )
	{
		BottomCurve.SetKeyTime( KeyHandle, Time );
	}
}


template<typename CurveType>
CurveType* GetCurveForChannel( EKeyMarginChannel Channel, CurveType* LeftCurve, CurveType* TopCurve, CurveType* RightCurve, CurveType* BottomCurve )
{
	switch ( Channel )
	{
	case EKeyMarginChannel::Left:
		return LeftCurve;
	case EKeyMarginChannel::Top:
		return TopCurve;
	case EKeyMarginChannel::Right:
		return RightCurve;
	case EKeyMarginChannel::Bottom:
		return BottomCurve;
	}
	checkf(false, TEXT("Invalid curve channel"));
	return nullptr;
}


void UMovieSceneMarginSection::AddKey( float Time, const FMarginKey& Key, EMovieSceneKeyInterpolation KeyInterpolation )
{
	FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	AddKeyToCurve( *KeyCurve, Time, Key.Value, KeyInterpolation );
}


bool UMovieSceneMarginSection::NewKeyIsNewData( float Time, const FMarginKey& Key ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	return FMath::IsNearlyEqual( KeyCurve->Eval( Time ), Key.Value ) == false;
}

bool UMovieSceneMarginSection::HasKeys( const FMarginKey& Key ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	return KeyCurve->GetNumKeys() != 0;
}

void UMovieSceneMarginSection::SetDefault( const FMarginKey& Key )
{
	FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	SetCurveDefault( *KeyCurve, Key.Value );
}

void UMovieSceneMarginSection::ClearDefaults()
{
	LeftCurve.ClearDefaultValue();
	TopCurve.ClearDefaultValue();
	RightCurve.ClearDefaultValue();
	BottomCurve.ClearDefaultValue();
}
