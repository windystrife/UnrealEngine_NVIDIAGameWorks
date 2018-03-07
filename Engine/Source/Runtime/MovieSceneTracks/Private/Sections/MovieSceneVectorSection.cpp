// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneVectorSection.h"
#include "UObject/StructOnScope.h"
#include "SequencerObjectVersion.h"

/* FMovieSceneVectorKeyStruct interface
 *****************************************************************************/

void FMovieSceneVectorKeyStructBase::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	for (int32 Index = 0; Index < GetChannelsUsed(); ++Index)
	{
		if (Keys[Index] == nullptr)
		{
			if (Curves[Index] != nullptr)
			{
				Curves[Index]->SetDefaultValue(GetPropertyChannelByIndex(Index));
			}
		}
		else
		{
			Keys[Index]->Value = GetPropertyChannelByIndex(Index);
			Keys[Index]->Time = Time;
		}
	}
}


/* UMovieSceneVectorSection structors
 *****************************************************************************/

UMovieSceneVectorSection::UMovieSceneVectorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChannelsUsed = 0;

	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState);
	BlendType = EMovieSceneBlendType::Absolute;
}

/* UMovieSceneSection interface
 *****************************************************************************/

void UMovieSceneVectorSection::MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);
	Super::MoveSection(DeltaTime, KeyHandles);

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ShiftCurve(DeltaTime, KeyHandles);
	}
}


void UMovieSceneVectorSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ScaleCurve(Origin, DilationFactor, KeyHandles);
	}
}


void UMovieSceneVectorSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		for (auto It(Curves[i].GetKeyHandleIterator()); It; ++It)
		{
			float Time = Curves[i].GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
	}
}


TSharedPtr<FStructOnScope> UMovieSceneVectorSection::GetKeyStruct(const TArray<FKeyHandle>& KeyHandles)
{
	TSharedPtr<FStructOnScope> KeyStruct;
	if (ChannelsUsed == 2)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector2DKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 3)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVectorKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 4)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector4KeyStruct::StaticStruct()));
	}

	if (KeyStruct.IsValid())
	{
		FMovieSceneVectorKeyStructBase* Struct = (FMovieSceneVectorKeyStructBase*)KeyStruct->GetStructMemory();

		float FirstValidKeyTime = 0.f;
		for (int32 Index = 0; Index < Struct->GetChannelsUsed(); ++Index)
		{
			Struct->Curves[Index] = &Curves[Index];
			Struct->Keys[Index] = Curves[Index].GetFirstMatchingKey(KeyHandles);
			if (Struct->Keys[Index] != nullptr)
			{
				FirstValidKeyTime = Struct->Keys[Index]->Time;
				Struct->Time = FirstValidKeyTime;
			}
		}

		for (int32 Index = 0; Index < Struct->GetChannelsUsed(); ++Index)
		{
			if (Struct->Keys[Index] == nullptr)
			{
				Struct->SetPropertyChannelByIndex(Index, Struct->Curves[Index]->Eval(FirstValidKeyTime));
			}
			else
			{
				Struct->SetPropertyChannelByIndex(Index, Struct->Keys[Index]->Value);
			}
		}
	}

	return KeyStruct;
}


TOptional<float> UMovieSceneVectorSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	for ( auto Curve : Curves )
	{
		if ( Curve.IsKeyHandleValid( KeyHandle ) )
		{
			return TOptional<float>( Curve.GetKeyTime( KeyHandle ) );
		}
	}
	return TOptional<float>();
}


void UMovieSceneVectorSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	for ( auto Curve : Curves )
	{
		if ( Curve.IsKeyHandleValid( KeyHandle ) )
		{
			Curve.SetKeyTime( KeyHandle, Time );
			break;
		}
	}
}


/* IKeyframeSection interface
 *****************************************************************************/

template<typename CurveType>
CurveType* GetCurveForChannel(EKeyVectorChannel Channel, CurveType* Curves, int32 ChannelsUsed)
{
	switch (Channel)
	{
		case EKeyVectorChannel::X:
			return &Curves[0];
		case EKeyVectorChannel::Y:
			return &Curves[1];
		case EKeyVectorChannel::Z:
			checkf(ChannelsUsed >= 3, TEXT("Can not get Z channel, it is not in use on this section."));
			return &Curves[2];
		case EKeyVectorChannel::W:
			checkf(ChannelsUsed >= 4, TEXT("Can not get W channel, it is not in use on this section."));
			return &Curves[3];
	}
	checkf(false, TEXT("Invalid channel requested"));
	return nullptr;
}


void UMovieSceneVectorSection::AddKey(float Time, const FVectorKey& Key, EMovieSceneKeyInterpolation KeyInterpolation)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	AddKeyToCurve(*ChannelCurve, Time, Key.Value, KeyInterpolation);
}


bool UMovieSceneVectorSection::NewKeyIsNewData(float Time, const FVectorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	return FMath::IsNearlyEqual(ChannelCurve->Eval(Time), Key.Value) == false;
}


bool UMovieSceneVectorSection::HasKeys(const FVectorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	return ChannelCurve->GetNumKeys() > 0;
}


void UMovieSceneVectorSection::SetDefault(const FVectorKey& Key)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	SetCurveDefault( *ChannelCurve, Key.Value );
}


void UMovieSceneVectorSection::ClearDefaults()
{
	for (auto& Curve : Curves)
	{
		Curve.ClearDefaultValue();
	}
}
