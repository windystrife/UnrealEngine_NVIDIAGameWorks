// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformSection.h"
#include "SequencerObjectVersion.h"

UMovieScene2DTransformSection::UMovieScene2DTransformSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
	BlendType = EMovieSceneBlendType::Absolute;
}


void UMovieScene2DTransformSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaTime, KeyHandles );

	Rotation.ShiftCurve( DeltaTime, KeyHandles );

	// Move all the curves in this section
	for( int32 Axis = 0; Axis < 2; ++Axis )
	{
		Translation[Axis].ShiftCurve( DeltaTime, KeyHandles );
		Scale[Axis].ShiftCurve( DeltaTime, KeyHandles );
		Shear[Axis].ShiftCurve( DeltaTime, KeyHandles );
	}
}


void UMovieScene2DTransformSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	Rotation.ScaleCurve( Origin, DilationFactor, KeyHandles);

	for( int32 Axis = 0; Axis < 2; ++Axis )
	{
		Translation[Axis].ScaleCurve( Origin, DilationFactor, KeyHandles );
		Scale[Axis].ScaleCurve( Origin, DilationFactor, KeyHandles );
		Shear[Axis].ScaleCurve( Origin, DilationFactor, KeyHandles );
	}
}


void UMovieScene2DTransformSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(Rotation.GetKeyHandleIterator()); It; ++It)
	{
		float Time = Rotation.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}

	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		for (auto It(Translation[Axis].GetKeyHandleIterator()); It; ++It)
		{
			float Time = Translation[Axis].GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
		for (auto It(Scale[Axis].GetKeyHandleIterator()); It; ++It)
		{
			float Time = Scale[Axis].GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
		for (auto It(Shear[Axis].GetKeyHandleIterator()); It; ++It)
		{
			float Time = Shear[Axis].GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
	}
}

TOptional<float> UMovieScene2DTransformSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( Rotation.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Rotation.GetKeyTime( KeyHandle ) );
	}
	if ( Translation[0].IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Translation[0].GetKeyTime( KeyHandle ) );
	}
	if ( Translation[1].IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Translation[1].GetKeyTime( KeyHandle ) );
	}
	if ( Scale[0].IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Scale[0].GetKeyTime( KeyHandle ) );
	}
	if ( Scale[1].IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Scale[1].GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}

void UMovieScene2DTransformSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if( Rotation.IsKeyHandleValid( KeyHandle ) )
	{
		Rotation.SetKeyTime( KeyHandle, Time );
	}
	else if( Translation[0].IsKeyHandleValid( KeyHandle ) )
	{
		Translation[0].SetKeyTime( KeyHandle, Time );
	}
	else if( Translation[1].IsKeyHandleValid( KeyHandle ) )
	{
		Translation[1].SetKeyTime( KeyHandle, Time );
	}
	else if( Scale[0].IsKeyHandleValid( KeyHandle ) )
	{
		Scale[0].SetKeyTime( KeyHandle, Time );
	}
	else if( Scale[1].IsKeyHandleValid( KeyHandle ) )
	{
		Scale[1].SetKeyTime( KeyHandle, Time );
	}
}

/**
 * Chooses an appropriate curve from an axis and a set of curves
 */
template<typename T>
static T* ChooseCurve( EAxis::Type Axis, T* Curves )
{
	switch(Axis)
	{
	case EAxis::X:
		return &Curves[0];

	case EAxis::Y:
		return &Curves[1];

	default:
		check(false);
		return nullptr;
	}
}


FRichCurve& UMovieScene2DTransformSection::GetTranslationCurve(EAxis::Type Axis)
{
	return *ChooseCurve( Axis, Translation );
}


const FRichCurve& UMovieScene2DTransformSection::GetTranslationCurve(EAxis::Type Axis) const
{
	return *ChooseCurve( Axis, Translation );
}


FRichCurve& UMovieScene2DTransformSection::GetRotationCurve()
{
	return Rotation;
}


const FRichCurve& UMovieScene2DTransformSection::GetRotationCurve() const
{
	return Rotation;
}


FRichCurve& UMovieScene2DTransformSection::GetScaleCurve(EAxis::Type Axis)
{
	return *ChooseCurve(Axis, Scale);
}


const FRichCurve& UMovieScene2DTransformSection::GetScaleCurve(EAxis::Type Axis) const
{
	return *ChooseCurve(Axis, Scale);
}


FRichCurve& UMovieScene2DTransformSection::GetShearCurve(EAxis::Type Axis)
{
	return *ChooseCurve(Axis, Shear);
}


const FRichCurve& UMovieScene2DTransformSection::GetShearCurve(EAxis::Type Axis) const
{
	return *ChooseCurve(Axis, Shear);
}


FWidgetTransform UMovieScene2DTransformSection::Eval( float Position, const FWidgetTransform& DefaultValue ) const
{
	return FWidgetTransform(	FVector2D( Translation[0].Eval( Position, DefaultValue.Translation.X ), Translation[1].Eval( Position, DefaultValue.Translation.Y ) ),
								FVector2D( Scale[0].Eval( Position, DefaultValue.Scale.X ),	Scale[1].Eval( Position, DefaultValue.Scale.Y ) ),
								FVector2D( Shear[0].Eval( Position, DefaultValue.Shear.X ),	Shear[1].Eval( Position, DefaultValue.Shear.Y ) ),
								Rotation.Eval( Position, DefaultValue.Angle ) );
}

// This function uses a template to avoid duplicating this for const and non-const versions
template<typename CurveType>
CurveType* GetCurveForChannelAndAxis( EKey2DTransformChannel Channel, EKey2DTransformAxis Axis, 
	CurveType* TranslationCurves, CurveType* ScaleCurves, CurveType* ShearCurves, CurveType* RotationCurve )
{
	switch ( Channel )
	{
	case EKey2DTransformChannel::Translation:
		if ( Axis == EKey2DTransformAxis::X )
		{
			return &TranslationCurves[0];
		}
		else if ( Axis == EKey2DTransformAxis::Y )
		{
			return &TranslationCurves[1];
		}
	case EKey2DTransformChannel::Scale:
		if ( Axis == EKey2DTransformAxis::X )
		{
			return &ScaleCurves[0];
		}
		else if ( Axis == EKey2DTransformAxis::Y )
		{
			return &ScaleCurves[1];
		}
	case EKey2DTransformChannel::Shear:
		if ( Axis == EKey2DTransformAxis::X )
		{
			return &ShearCurves[0];
		}
		else if ( Axis == EKey2DTransformAxis::Y )
		{
			return &ShearCurves[1];
		}
	case EKey2DTransformChannel::Rotation:
		if ( Axis == EKey2DTransformAxis::None )
		{
			return RotationCurve;
		}
	}
	checkf( false, TEXT( "Invalid channel and axis combination." ) );
	return nullptr;
}


bool UMovieScene2DTransformSection::NewKeyIsNewData( float Time, const struct F2DTransformKey& TransformKey ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannelAndAxis( TransformKey.Channel, TransformKey.Axis, Translation, Scale, Shear, &Rotation );
	return FMath::IsNearlyEqual( KeyCurve->Eval( Time ), TransformKey.Value ) == false;
}


bool UMovieScene2DTransformSection::HasKeys( const struct F2DTransformKey& TransformKey ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannelAndAxis( TransformKey.Channel, TransformKey.Axis, Translation, Scale, Shear, &Rotation );
	return KeyCurve->GetNumKeys() != 0;
}


void UMovieScene2DTransformSection::AddKey( float Time, const struct F2DTransformKey& TransformKey, EMovieSceneKeyInterpolation KeyInterpolation )
{
	FRichCurve* KeyCurve = GetCurveForChannelAndAxis( TransformKey.Channel, TransformKey.Axis, Translation, Scale, Shear, &Rotation );
	AddKeyToCurve( *KeyCurve, Time, TransformKey.Value, KeyInterpolation );
}


void UMovieScene2DTransformSection::SetDefault( const struct F2DTransformKey& TransformKey )
{
	FRichCurve* KeyCurve = GetCurveForChannelAndAxis( TransformKey.Channel, TransformKey.Axis, Translation, Scale, Shear, &Rotation );
	SetCurveDefault( *KeyCurve, TransformKey.Value );
}


void UMovieScene2DTransformSection::ClearDefaults()
{
	Rotation.ClearDefaultValue();
	Translation[0].ClearDefaultValue();
	Translation[1].ClearDefaultValue();
	Scale[0].ClearDefaultValue();
	Scale[1].ClearDefaultValue();
	Shear[0].ClearDefaultValue();
	Shear[1].ClearDefaultValue();
}
