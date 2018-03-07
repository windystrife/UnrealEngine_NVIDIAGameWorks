// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneComposurePostMoveSettingsSection.h"

UMovieSceneComposurePostMoveSettingsSection::UMovieSceneComposurePostMoveSettingsSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
	BlendType = EMovieSceneBlendType::Absolute;
}

void UMovieSceneComposurePostMoveSettingsSection::GetAllCurves(TArray<FRichCurve*>& AllCurves)
{
	AllCurves.Add(&Pivot[0]);
	AllCurves.Add(&Pivot[1]);
	AllCurves.Add(&Translation[0]);
	AllCurves.Add(&Translation[1]);
	AllCurves.Add(&RotationAngle);
	AllCurves.Add(&Scale);
}

void UMovieSceneComposurePostMoveSettingsSection::GetAllCurves(TArray<const FRichCurve*>& AllCurves) const
{
	AllCurves.Add(&Pivot[0]);
	AllCurves.Add(&Pivot[1]);
	AllCurves.Add(&Translation[0]);
	AllCurves.Add(&Translation[1]);
	AllCurves.Add(&RotationAngle);
	AllCurves.Add(&Scale);
}

void UMovieSceneComposurePostMoveSettingsSection::MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaTime, KeyHandles);

	TArray<FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (FRichCurve* Curve : AllCurves)
	{
		Curve->ShiftCurve(DeltaTime, KeyHandles);
	}
}


void UMovieSceneComposurePostMoveSettingsSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	TArray<FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (FRichCurve* Curve : AllCurves)
	{
		Curve->ScaleCurve(Origin, DilationFactor, KeyHandles);
	}
}


void UMovieSceneComposurePostMoveSettingsSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	TArray<const FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (const FRichCurve* Curve : AllCurves)
	{
		for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
		{
			float Time = Curve->GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
	}
}

TOptional<float> UMovieSceneComposurePostMoveSettingsSection::GetKeyTime(FKeyHandle KeyHandle) const
{
	TArray<const FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (const FRichCurve* Curve : AllCurves)
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			return Curve->GetKeyTime(KeyHandle);
		}
	}
	return TOptional<float>();
}

void UMovieSceneComposurePostMoveSettingsSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	TArray<FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (FRichCurve* Curve : AllCurves)
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			Curve->SetKeyTime(KeyHandle, Time);
			break;
		}
	}
}

// This function uses a template to avoid duplicating this for const and non-const versions
template<typename CurveType>
CurveType* GetCurveForChannelAndAxis(EComposurePostMoveSettingsChannel Channel, EComposurePostMoveSettingsAxis Axis,
	CurveType* Pivot, CurveType* Translation, CurveType* RotationAngle, CurveType* Scale)
{
	CurveType* Curve = nullptr;
	switch (Channel)
	{
	case EComposurePostMoveSettingsChannel::Pivot:
		if (Axis == EComposurePostMoveSettingsAxis::X)
		{
			Curve = &Pivot[0];
		}
		else if (Axis == EComposurePostMoveSettingsAxis::Y)
		{
			Curve = &Pivot[1];
		}
		break;
	case EComposurePostMoveSettingsChannel::Translation:
		if (Axis == EComposurePostMoveSettingsAxis::X)
		{
			Curve = &Translation[0];
		}
		else if (Axis == EComposurePostMoveSettingsAxis::Y)
		{
			Curve = &Translation[1];
		}
		break;
	case EComposurePostMoveSettingsChannel::RotationAngle:
		if (Axis == EComposurePostMoveSettingsAxis::None)
		{
			Curve = RotationAngle;
		}
		break;
	case EComposurePostMoveSettingsChannel::Scale:
		if (Axis == EComposurePostMoveSettingsAxis::None)
		{
			Curve = Scale;
		}
		break;
	}
	checkf(Curve != nullptr, TEXT("Invalid channel/axis combination"));
	return Curve;
}

FRichCurve& UMovieSceneComposurePostMoveSettingsSection::GetCurve(EComposurePostMoveSettingsChannel Channel, EComposurePostMoveSettingsAxis Axis)
{
	return *GetCurveForChannelAndAxis(Channel, Axis, Pivot, Translation, &RotationAngle, &Scale);
}

const FRichCurve& UMovieSceneComposurePostMoveSettingsSection::GetCurve(EComposurePostMoveSettingsChannel Channel, EComposurePostMoveSettingsAxis Axis) const
{
	return *GetCurveForChannelAndAxis(Channel, Axis, Pivot, Translation, &RotationAngle, &Scale);
}

bool UMovieSceneComposurePostMoveSettingsSection::NewKeyIsNewData(float Time, const struct FComposurePostMoveSettingsKey& Key) const
{
	const FRichCurve& Curve = GetCurve(Key.Channel, Key.Axis);
	return FMath::IsNearlyEqual(Curve.Eval(Time), Key.Value);
}

bool UMovieSceneComposurePostMoveSettingsSection::HasKeys(const struct FComposurePostMoveSettingsKey& Key) const
{
	const FRichCurve& Curve = GetCurve(Key.Channel, Key.Axis);
	return Curve.GetNumKeys() != 0;
}

void UMovieSceneComposurePostMoveSettingsSection::AddKey(float Time, const struct FComposurePostMoveSettingsKey& Key, EMovieSceneKeyInterpolation KeyInterpolation)
{
	FRichCurve& Curve = GetCurve(Key.Channel, Key.Axis);
	AddKeyToCurve(Curve, Time, Key.Value, KeyInterpolation);
}

void UMovieSceneComposurePostMoveSettingsSection::SetDefault(const struct FComposurePostMoveSettingsKey& Key)
{
	FRichCurve& Curve = GetCurve(Key.Channel, Key.Axis);
	SetCurveDefault(Curve, Key.Value);
}

void UMovieSceneComposurePostMoveSettingsSection::ClearDefaults()
{
	TArray<FRichCurve*> AllCurves;
	GetAllCurves(AllCurves);

	for (FRichCurve* Curve : AllCurves)
	{
		Curve->ClearDefaultValue();
	}
}