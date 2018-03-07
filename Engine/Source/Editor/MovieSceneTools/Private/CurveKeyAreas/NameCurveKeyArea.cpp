// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NameCurveKeyArea.h"
#include "UObject/StructOnScope.h"
#include "Curves/NameCurve.h"
#include "Widgets/SNullWidget.h"
#include "MovieSceneTrack.h"
#include "ClipboardTypes.h"
#include "SequencerClipboardReconciler.h"

/* IKeyArea interface
 *****************************************************************************/

TArray<FKeyHandle> FNameCurveKeyArea::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;
	FKeyHandle CurrentKey = Curve.FindKey(Time);

	if (Curve.IsKeyHandleValid(CurrentKey) == false)
	{
		if (OwningSection->GetStartTime() > Time)
		{
			OwningSection->SetStartTime(Time);
		}

		if (OwningSection->GetEndTime() < Time)
		{
			OwningSection->SetEndTime(Time);
		}

		Curve.AddKey(Time, NAME_None, CurrentKey);
		AddedKeyHandles.Add(CurrentKey);
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FNameCurveKeyArea::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	if (!Curve.IsKeyHandleValid(KeyToDuplicate))
	{
		return TOptional<FKeyHandle>();
	}

	return Curve.AddKey(GetKeyTime(KeyToDuplicate), Curve.GetKey(KeyToDuplicate).Value);
}

bool FNameCurveKeyArea::CanCreateKeyEditor()
{
	return false;
}


TSharedRef<SWidget> FNameCurveKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNullWidget::NullWidget;
}


void FNameCurveKeyArea::DeleteKey(FKeyHandle KeyHandle)
{
	Curve.DeleteKey(KeyHandle);
}


TOptional<FLinearColor> FNameCurveKeyArea::GetColor()
{
	return TOptional<FLinearColor>();
}


ERichCurveExtrapolation FNameCurveKeyArea::GetExtrapolationMode(bool bPreInfinity) const
{
	return RCCE_None;
}


ERichCurveInterpMode FNameCurveKeyArea::GetKeyInterpMode(FKeyHandle Keyhandle) const
{
	return RCIM_None;
}


TSharedPtr<FStructOnScope> FNameCurveKeyArea::GetKeyStruct(FKeyHandle KeyHandle)
{
	return MakeShareable(new FStructOnScope(FNameCurveKey::StaticStruct(), (uint8*)&Curve.GetKey(KeyHandle)));
}


ERichCurveTangentMode FNameCurveKeyArea::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	return RCTM_None;
}


float FNameCurveKeyArea::GetKeyTime(FKeyHandle KeyHandle) const
{
	return Curve.GetKeyTime(KeyHandle);
}


UMovieSceneSection* FNameCurveKeyArea::GetOwningSection()
{
	return OwningSection.Get();
}



FRichCurve* FNameCurveKeyArea::GetRichCurve()
{
	return nullptr;
}


TArray<FKeyHandle> FNameCurveKeyArea::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> OutKeyHandles;
	OutKeyHandles.Reserve(Curve.GetNumKeys());

	for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
	{
		OutKeyHandles.Add(It.Key());
	}

	return OutKeyHandles;
}


FKeyHandle FNameCurveKeyArea::DilateKey( FKeyHandle KeyHandle, float Scale, float Origin )
{
	float NewKeyTime = Curve.GetKeyTime(KeyHandle);
	NewKeyTime = (NewKeyTime - Origin) * Scale + Origin;
	return Curve.SetKeyTime(KeyHandle, NewKeyTime);
}
	

FKeyHandle FNameCurveKeyArea::MoveKey( FKeyHandle KeyHandle, float DeltaPosition )
{
	return Curve.SetKeyTime(KeyHandle, Curve.GetKeyTime(KeyHandle) + DeltaPosition);
}
	

void FNameCurveKeyArea::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	// do nothing
}


void FNameCurveKeyArea::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	// do nothing
}


void FNameCurveKeyArea::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	// do nothing
}


void FNameCurveKeyArea::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime)
{
	Curve.SetKeyTime(KeyHandle, NewKeyTime);
}

void FNameCurveKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const UMovieSceneSection* Section = const_cast<FNameCurveKeyArea*>(this)->GetOwningSection();
	UMovieSceneTrack* Track = Section ? Section->GetTypedOuter<UMovieSceneTrack>() : nullptr;
	if (!Track)
	{
		return;
	}

	FMovieSceneClipboardKeyTrack* KeyTrack = nullptr;

	for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
	{
		FKeyHandle Handle = It.Key();
		if (KeyMask(Handle, *this))
		{
			FNameCurveKey Key = Curve.GetKey(Handle);
			if (!KeyTrack)
			{
				KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<FName>(GetName(), *Track);
			}

			KeyTrack->AddKey(Key.Time, Key.Value);
		}
	}
}

void FNameCurveKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
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

			FKeyHandle KeyHandle = Curve.UpdateOrAddKey(Time, Key.GetValue<FName>());
			DstEnvironment.ReportPastedKey(KeyHandle, *this);
		}
			
		return true;
	});
}
