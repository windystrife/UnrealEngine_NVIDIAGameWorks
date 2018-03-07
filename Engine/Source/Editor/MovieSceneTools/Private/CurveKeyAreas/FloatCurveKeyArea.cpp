// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FloatCurveKeyArea.h"
#include "UObject/StructOnScope.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "ClipboardTypes.h"
#include "CurveKeyEditors/SFloatCurveKeyEditor.h"
#include "SequencerClipboardReconciler.h"

/* IKeyArea interface
 *****************************************************************************/

TArray<FKeyHandle> FFloatCurveKeyArea::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;
	FKeyHandle CurrentKeyHandle = Curve->FindKey(Time);

	if (Curve->IsKeyHandleValid(CurrentKeyHandle) == false)
	{
		TOptional<float> ExternalValue = TimeToCopyFrom == FLT_MAX ? GetExternalValue() : TOptional<float>();

		if (OwningSection->GetStartTime() > Time)
		{
			OwningSection->SetStartTime(Time);
		}

		if (OwningSection->GetEndTime() < Time)
		{
			OwningSection->SetEndTime(Time);
		}

		float Value;
		if (ExternalValue)
		{
			Value = ExternalValue.GetValue();
		}
		else
		{
			float EvalTime = TimeToCopyFrom != FLT_MAX ? Time : TimeToCopyFrom;
			float DefaultValue = 0;
			Value = Curve->Eval(EvalTime, DefaultValue);
		}

		Curve->AddKey(Time, Value, false, CurrentKeyHandle);
		AddedKeyHandles.Add(CurrentKeyHandle);
		
		MovieSceneHelpers::SetKeyInterpolation(*Curve, CurrentKeyHandle, InKeyInterpolation);		
		
		// Copy the properties from the key if it exists
		FKeyHandle KeyHandleToCopy = Curve->FindKey(TimeToCopyFrom);

		if (Curve->IsKeyHandleValid(KeyHandleToCopy))
		{
			FRichCurveKey& CurrentKey = Curve->GetKey(CurrentKeyHandle);
			FRichCurveKey& KeyToCopy = Curve->GetKey(KeyHandleToCopy);
			CurrentKey.InterpMode = KeyToCopy.InterpMode;
			CurrentKey.TangentMode = KeyToCopy.TangentMode;
			CurrentKey.TangentWeightMode = KeyToCopy.TangentWeightMode;
			CurrentKey.ArriveTangent = KeyToCopy.ArriveTangent;
			CurrentKey.LeaveTangent = KeyToCopy.LeaveTangent;
			CurrentKey.ArriveTangentWeight = KeyToCopy.ArriveTangentWeight;
			CurrentKey.LeaveTangentWeight = KeyToCopy.LeaveTangentWeight;
		}
	}
	else if (TOptional<float> ExternalValue = GetExternalValue())
	{
		Curve->UpdateOrAddKey(Time, ExternalValue.GetValue());
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FFloatCurveKeyArea::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	if (!Curve->IsKeyHandleValid(KeyToDuplicate))
	{
		return TOptional<FKeyHandle>();
	}

	FRichCurveKey ThisKey = Curve->GetKey(KeyToDuplicate);

	FKeyHandle KeyHandle = Curve->AddKey(GetKeyTime(KeyToDuplicate), ThisKey.Value);
	// Ensure the rest of the key properties are added
	Curve->GetKey(KeyHandle) = ThisKey;

	return KeyHandle;
}

bool FFloatCurveKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FFloatCurveKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SFloatCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(Curve)
		.ExternalValue(this, &FFloatCurveKeyArea::GetExternalValue);
}


void FFloatCurveKeyArea::DeleteKey(FKeyHandle KeyHandle)
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		Curve->DeleteKey(KeyHandle);
	}
}


TOptional<FLinearColor> FFloatCurveKeyArea::GetColor()
{
	return Color;
}


ERichCurveExtrapolation FFloatCurveKeyArea::GetExtrapolationMode(bool bPreInfinity) const
{
	if (bPreInfinity)
	{
		return Curve->PreInfinityExtrap;
	}

	return Curve->PostInfinityExtrap;
}


ERichCurveInterpMode FFloatCurveKeyArea::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		return Curve->GetKeyInterpMode(KeyHandle);
	}

	return RCIM_None;
}


TSharedPtr<FStructOnScope> FFloatCurveKeyArea::GetKeyStruct(FKeyHandle KeyHandle)
{
	return MakeShareable(new FStructOnScope(FRichCurveKey::StaticStruct(), (uint8*)&Curve->GetKey(KeyHandle)));
}


ERichCurveTangentMode FFloatCurveKeyArea::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		return Curve->GetKeyTangentMode(KeyHandle);
	}

	return RCTM_None;
}


float FFloatCurveKeyArea::GetKeyTime(FKeyHandle KeyHandle) const
{
	return Curve->GetKeyTime(KeyHandle);
}


UMovieSceneSection* FFloatCurveKeyArea::GetOwningSection()
{
	return OwningSection;
}


FRichCurve* FFloatCurveKeyArea::GetRichCurve()
{
	return Curve;
}


TArray<FKeyHandle> FFloatCurveKeyArea::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> OutKeyHandles;
	OutKeyHandles.Reserve(Curve->GetNumKeys());

	for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
	{
		OutKeyHandles.Add(It.Key());
	}

	return OutKeyHandles;
}


FKeyHandle FFloatCurveKeyArea::DilateKey(FKeyHandle KeyHandle, float Scale, float Origin)
{
	float NewKeyTime = Curve->GetKeyTime(KeyHandle);
	NewKeyTime = (NewKeyTime - Origin) * Scale + Origin;
	FKeyHandle NewKeyHandle = Curve->SetKeyTime(KeyHandle, NewKeyTime);
	Curve->AutoSetTangents();
	return NewKeyHandle;
}

FKeyHandle FFloatCurveKeyArea::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	FKeyHandle NewKeyHandle = Curve->SetKeyTime(KeyHandle, Curve->GetKeyTime(KeyHandle) + DeltaPosition);
	Curve->AutoSetTangents();
	return NewKeyHandle;
}


void FFloatCurveKeyArea::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	if (bPreInfinity)
	{
		Curve->PreInfinityExtrap = ExtrapMode;
	}
	else
	{
		Curve->PostInfinityExtrap = ExtrapMode;
	}
}

bool FFloatCurveKeyArea::CanSetExtrapolationMode() const
{
	return true;
}

void FFloatCurveKeyArea::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		Curve->SetKeyInterpMode(KeyHandle, InterpMode);
	}
}


void FFloatCurveKeyArea::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		Curve->SetKeyTangentMode(KeyHandle, TangentMode);
	}
}


void FFloatCurveKeyArea::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime)
{
	Curve->SetKeyTime(KeyHandle, NewKeyTime);
	Curve->AutoSetTangents();
}

void FFloatCurveKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const UMovieSceneSection* Section = const_cast<FFloatCurveKeyArea*>(this)->GetOwningSection();
	UMovieSceneTrack* Track = Section ? Section->GetTypedOuter<UMovieSceneTrack>() : nullptr;
	if (!Track)
	{
		return;
	}

	FMovieSceneClipboardKeyTrack* KeyTrack = nullptr;

	for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
	{
		FKeyHandle Handle = It.Key();
		if (KeyMask(Handle, *this))
		{
			if (!KeyTrack)
			{
				KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<FRichCurveKey>(GetName(), *Track);
			}

			FRichCurveKey Key = Curve->GetKey(Handle);
			KeyTrack->AddKey(Key.Time, Key);
		}
	}
}

void FFloatCurveKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
{
	float PasteAt = DstEnvironment.CardinalTime;

	KeyTrack.IterateKeys([&](const FMovieSceneClipboardKey& Key){
		UMovieSceneSection* Section = GetOwningSection();
		if (!Section)
		{
			return true;
		}

		if (Section->TryModify())
		{
			float Time = PasteAt + Key.GetTime();
			if (Section->GetStartTime() > Time)
			{
				Section->SetStartTime(Time);
			}
			if (Section->GetEndTime() < Time)
			{
				Section->SetEndTime(Time);
			}

			FRichCurveKey NewKey = Key.GetValue<FRichCurveKey>();

			// Rich curve keys store their time internally, so we need to ensure they are set up correctly here (if there's been some conversion, for instance)
			NewKey.Time = Time;

			FKeyHandle KeyHandle = Curve->UpdateOrAddKey(Time, NewKey.Value);
			// Ensure the rest of the key properties are added
			Curve->GetKey(KeyHandle) = NewKey;

			DstEnvironment.ReportPastedKey(KeyHandle, *this);
		}

		return true;
	});
}

TOptional<float> FFloatCurveKeyArea::GetExternalValue() const
{
	FOptionalMovieSceneBlendType BlendType = OwningSection->GetBlendType();
	if (ExternalValueAttribute.IsSet() && (!BlendType.IsValid() || BlendType.Get() == EMovieSceneBlendType::Absolute))
	{
		return ExternalValueAttribute.Get();
	}

	return TOptional<float>();
}