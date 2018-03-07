// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StringCurveKeyArea.h"
#include "UObject/StructOnScope.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MovieSceneTrack.h"
#include "ClipboardTypes.h"
#include "CurveKeyEditors/SStringCurveKeyEditor.h"
#include "Curves/StringCurve.h"
#include "SequencerClipboardReconciler.h"

/* IKeyArea interface
 *****************************************************************************/

TArray<FKeyHandle> FStringCurveKeyArea::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;
	FKeyHandle CurrentKeyHandle = Curve->FindKey(Time);

	if (Curve->IsKeyHandleValid(CurrentKeyHandle) == false)
	{
		if (OwningSection->GetStartTime() > Time)
		{
			OwningSection->SetStartTime(Time);
		}

		if (OwningSection->GetEndTime() < Time)
		{
			OwningSection->SetEndTime(Time);
		}

		FString Value;
		if (ExternalValue.IsSet() && ExternalValue.Get().IsSet() && TimeToCopyFrom == FLT_MAX)
		{
			Value = ExternalValue.Get().GetValue();
		}
		else
		{
			float EvalTime = TimeToCopyFrom != FLT_MAX ? Time : TimeToCopyFrom;
			FString DefaultValue;
			Value = Curve->Eval(EvalTime, DefaultValue);
		}

		Curve->AddKey(Time, Value, CurrentKeyHandle);
		AddedKeyHandles.Add(CurrentKeyHandle);
	}
	else if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		FString Value = ExternalValue.Get().GetValue();
		Curve->UpdateOrAddKey(Time, Value);
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FStringCurveKeyArea::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	if (!Curve->IsKeyHandleValid(KeyToDuplicate))
	{
		return TOptional<FKeyHandle>();
	}

	FStringCurveKey ThisKey = Curve->GetKey(KeyToDuplicate);

	FKeyHandle KeyHandle = Curve->AddKey(GetKeyTime(KeyToDuplicate), ThisKey.Value);
	// Ensure the rest of the key properties are added
	Curve->GetKey(KeyHandle) = ThisKey;

	return KeyHandle;
}

bool FStringCurveKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FStringCurveKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SStringCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(Curve)
		.ExternalValue(ExternalValue);
};


void FStringCurveKeyArea::DeleteKey(FKeyHandle KeyHandle)
{
	if (Curve->IsKeyHandleValid(KeyHandle))
	{
		Curve->DeleteKey(KeyHandle);
	}
}


TOptional<FLinearColor> FStringCurveKeyArea::GetColor()
{
	return Color;
}


ERichCurveExtrapolation FStringCurveKeyArea::GetExtrapolationMode(bool bPreInfinity) const
{
	return RCCE_None;
}

ERichCurveInterpMode FStringCurveKeyArea::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	return RCIM_None;
}


TSharedPtr<FStructOnScope> FStringCurveKeyArea::GetKeyStruct(FKeyHandle KeyHandle)
{
	return MakeShareable(new FStructOnScope(FStringCurveKey::StaticStruct(), (uint8*)&Curve->GetKey(KeyHandle)));
}


ERichCurveTangentMode FStringCurveKeyArea::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	return RCTM_None;
}


float FStringCurveKeyArea::GetKeyTime(FKeyHandle KeyHandle) const
{
	return Curve->GetKeyTime(KeyHandle);
}


UMovieSceneSection* FStringCurveKeyArea::GetOwningSection()
{
	return OwningSection;
}


FRichCurve* FStringCurveKeyArea::GetRichCurve()
{
	return nullptr;
}


TArray<FKeyHandle> FStringCurveKeyArea::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> OutKeyHandles;
	OutKeyHandles.Reserve(Curve->GetNumKeys());

	for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
	{
		OutKeyHandles.Add(It.Key());
	}

	return OutKeyHandles;
}


FKeyHandle FStringCurveKeyArea::DilateKey(FKeyHandle KeyHandle, float Scale, float Origin)
{
	float NewKeyTime = Curve->GetKeyTime(KeyHandle);
	NewKeyTime = (NewKeyTime - Origin) * Scale + Origin;
	return Curve->SetKeyTime(KeyHandle, NewKeyTime);
}


FKeyHandle FStringCurveKeyArea::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	return Curve->SetKeyTime(KeyHandle, Curve->GetKeyTime(KeyHandle) + DeltaPosition);
}


void FStringCurveKeyArea::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	// do nothing
}

bool FStringCurveKeyArea::CanSetExtrapolationMode() const
{
	return false;
}

void FStringCurveKeyArea::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	// do nothing
}


void FStringCurveKeyArea::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	// do nothing
}


void FStringCurveKeyArea::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime)
{
	Curve->SetKeyTime(KeyHandle, NewKeyTime);
}

void FStringCurveKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const UMovieSceneSection* Section = const_cast<FStringCurveKeyArea*>(this)->GetOwningSection();
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
			FStringCurveKey Key = Curve->GetKey(Handle);
			if (!KeyTrack)
			{
				KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<FString>(GetName(), *Track);
			}

			KeyTrack->AddKey(Key.Time, Key.Value);
		}
	}
}

void FStringCurveKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
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

			FKeyHandle KeyHandle = Curve->UpdateOrAddKey(Time, Key.GetValue<FString>());
			DstEnvironment.ReportPastedKey(KeyHandle, *this);
		}
			
		return true;
	});
}
