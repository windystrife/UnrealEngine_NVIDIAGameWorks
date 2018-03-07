// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScenePropertyRecorder.h"
#include "MovieScene.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Sections/MovieSceneByteSection.h"
#include "Tracks/MovieSceneByteTrack.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Tracks/MovieSceneEnumTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Tracks/MovieSceneVectorTrack.h"

// current set of compiled-in property types

template <>
bool FMovieScenePropertyRecorder<bool>::ShouldAddNewKey(const bool& InNewValue) const
{
	return InNewValue != PreviousValue;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<bool>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneBoolTrack* Track = InMovieScene->AddTrack<UMovieSceneBoolTrack>(InGuid);
	if (Track)
	{
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>(Track->CreateNewSection());
		Section->SetDefault(PreviousValue);
		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);
		Section->AddKey(InTime, PreviousValue, EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<bool>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<bool>& InKey)
{
	CastChecked<UMovieSceneBoolSection>(InSection)->AddKey(InKey.Time, InKey.Value, EMovieSceneKeyInterpolation::Break);
}

template <>
void FMovieScenePropertyRecorder<bool>::ReduceKeys(UMovieSceneSection* InSection)
{
}

template <>
bool FMovieScenePropertyRecorder<uint8>::ShouldAddNewKey(const uint8& InNewValue) const
{
	return InNewValue != PreviousValue;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<uint8>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneByteTrack* Track = InMovieScene->AddTrack<UMovieSceneByteTrack>(InGuid);
	if (Track)
	{
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneByteSection* Section = Cast<UMovieSceneByteSection>(Track->CreateNewSection());
		Section->SetDefault(PreviousValue);
		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);
		Section->AddKey(InTime, PreviousValue, EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<uint8>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<uint8>& InKey)
{
	CastChecked<UMovieSceneByteSection>(InSection)->AddKey(InKey.Time, InKey.Value, EMovieSceneKeyInterpolation::Break);
}

template <>
void FMovieScenePropertyRecorder<uint8>::ReduceKeys(UMovieSceneSection* InSection)
{
}

bool FMovieScenePropertyRecorderEnum::ShouldAddNewKey(const int64& InNewValue) const
{
	return InNewValue != PreviousValue;
}

UMovieSceneSection* FMovieScenePropertyRecorderEnum::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneEnumTrack* Track = InMovieScene->AddTrack<UMovieSceneEnumTrack>(InGuid);
	if (Track)
	{
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneEnumSection* Section = Cast<UMovieSceneEnumSection>(Track->CreateNewSection());
		Section->SetDefault(PreviousValue);
		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);
		Section->AddKey(InTime, PreviousValue, EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

void FMovieScenePropertyRecorderEnum::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<int64>& InKey)
{
	CastChecked<UMovieSceneEnumSection>(InSection)->AddKey(InKey.Time, InKey.Value, EMovieSceneKeyInterpolation::Break);
}

void FMovieScenePropertyRecorderEnum::ReduceKeys(UMovieSceneSection* InSection)
{
}

template <>
bool FMovieScenePropertyRecorder<float>::ShouldAddNewKey(const float& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<float>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneFloatTrack* Track = InMovieScene->AddTrack<UMovieSceneFloatTrack>(InGuid);
	if (Track)
	{
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
		Section->SetDefault(PreviousValue);
		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);
		Section->AddKey(InTime, PreviousValue, EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<float>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<float>& InKey)
{
	CastChecked<UMovieSceneFloatSection>(InSection)->AddKey(InKey.Time, InKey.Value, EMovieSceneKeyInterpolation::Auto);
}

template <>
void FMovieScenePropertyRecorder<float>::ReduceKeys(UMovieSceneSection* InSection)
{
	CastChecked<UMovieSceneFloatSection>(InSection)->GetFloatCurve().RemoveRedundantKeys(KINDA_SMALL_NUMBER);
}

template <>
bool FMovieScenePropertyRecorder<FColor>::ShouldAddNewKey(const FColor& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<FColor>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneColorTrack* Track = InMovieScene->AddTrack<UMovieSceneColorTrack>(InGuid);
	if (Track)
	{
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>(Track->CreateNewSection());
		Section->SetDefault(FColorKey(EKeyColorChannel::Red, PreviousValue.R, false));
		Section->SetDefault(FColorKey(EKeyColorChannel::Green, PreviousValue.G, false));
		Section->SetDefault(FColorKey(EKeyColorChannel::Blue, PreviousValue.B, false));
		Section->SetDefault(FColorKey(EKeyColorChannel::Alpha, PreviousValue.A, false));

		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);

		Section->AddKey(InTime, FColorKey(EKeyColorChannel::Red, PreviousValue.R, false), EMovieSceneKeyInterpolation::Break);
		Section->AddKey(InTime, FColorKey(EKeyColorChannel::Green, PreviousValue.G, false), EMovieSceneKeyInterpolation::Break);
		Section->AddKey(InTime, FColorKey(EKeyColorChannel::Blue, PreviousValue.B, false), EMovieSceneKeyInterpolation::Break);
		Section->AddKey(InTime, FColorKey(EKeyColorChannel::Alpha, PreviousValue.A, false), EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<FColor>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<FColor>& InKey)
{
	UMovieSceneColorSection* ColorSection = CastChecked<UMovieSceneColorSection>(InSection);
	ColorSection->AddKey(InKey.Time, FColorKey(EKeyColorChannel::Red, InKey.Value.R, false), EMovieSceneKeyInterpolation::Auto);
	ColorSection->AddKey(InKey.Time, FColorKey(EKeyColorChannel::Green, InKey.Value.G, false), EMovieSceneKeyInterpolation::Auto);
	ColorSection->AddKey(InKey.Time, FColorKey(EKeyColorChannel::Blue, InKey.Value.B, false), EMovieSceneKeyInterpolation::Auto);
	ColorSection->AddKey(InKey.Time, FColorKey(EKeyColorChannel::Alpha, InKey.Value.A, false), EMovieSceneKeyInterpolation::Auto);
}

template <>
void FMovieScenePropertyRecorder<FColor>::ReduceKeys(UMovieSceneSection* InSection)
{
	UMovieSceneColorSection* ColorSection = CastChecked<UMovieSceneColorSection>(InSection);
	ColorSection->GetRedCurve().RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	ColorSection->GetGreenCurve().RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	ColorSection->GetBlueCurve().RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	ColorSection->GetAlphaCurve().RemoveRedundantKeys(KINDA_SMALL_NUMBER);
}

template <>
bool FMovieScenePropertyRecorder<FVector>::ShouldAddNewKey(const FVector& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<FVector>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	UMovieSceneVectorTrack* Track = InMovieScene->AddTrack<UMovieSceneVectorTrack>(InGuid);
	if (Track)
	{
		Track->SetNumChannelsUsed(3);
		if (InObjectToRecord)
		{
			Track->SetPropertyNameAndPath(*Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString(), Binding.GetPropertyPath());
		}

		UMovieSceneVectorSection* Section = Cast<UMovieSceneVectorSection>(Track->CreateNewSection());

		Section->SetDefault(FVectorKey(EKeyVectorChannel::X, PreviousValue.X));
		Section->SetDefault(FVectorKey(EKeyVectorChannel::Y, PreviousValue.Y));
		Section->SetDefault(FVectorKey(EKeyVectorChannel::Z, PreviousValue.Z));

		Section->SetStartTime(InTime);
		Section->SetEndTime(InTime);
		
		Section->AddKey(InTime, FVectorKey(EKeyVectorChannel::X, PreviousValue.X), EMovieSceneKeyInterpolation::Break);
		Section->AddKey(InTime, FVectorKey(EKeyVectorChannel::Y, PreviousValue.Y), EMovieSceneKeyInterpolation::Break);
		Section->AddKey(InTime, FVectorKey(EKeyVectorChannel::Z, PreviousValue.Z), EMovieSceneKeyInterpolation::Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<FVector>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<FVector>& InKey)
{
	UMovieSceneVectorSection* VectorSection = CastChecked<UMovieSceneVectorSection>(InSection);

	VectorSection->AddKey(InKey.Time, FVectorKey(EKeyVectorChannel::X, InKey.Value.X), EMovieSceneKeyInterpolation::Auto);
	VectorSection->AddKey(InKey.Time, FVectorKey(EKeyVectorChannel::Y, InKey.Value.Y), EMovieSceneKeyInterpolation::Auto);
	VectorSection->AddKey(InKey.Time, FVectorKey(EKeyVectorChannel::Z, InKey.Value.Z), EMovieSceneKeyInterpolation::Auto);
}

template <>
void FMovieScenePropertyRecorder<FVector>::ReduceKeys(UMovieSceneSection* InSection)
{
	UMovieSceneVectorSection* VectorSection = CastChecked<UMovieSceneVectorSection>(InSection);

	VectorSection->GetCurve(0).RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	VectorSection->GetCurve(1).RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	VectorSection->GetCurve(2).RemoveRedundantKeys(KINDA_SMALL_NUMBER);
}

template class FMovieScenePropertyRecorder<bool>;
template class FMovieScenePropertyRecorder<uint8>;
template class FMovieScenePropertyRecorder<float>;
template class FMovieScenePropertyRecorder<FColor>;
template class FMovieScenePropertyRecorder<FVector>;
