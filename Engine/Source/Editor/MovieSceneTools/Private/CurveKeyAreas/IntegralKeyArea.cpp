// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IntegralKeyArea.h"
#include "UObject/StructOnScope.h"

/* IKeyArea interface
 *****************************************************************************/

TArray<FKeyHandle> FIntegralCurveKeyAreaBase::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
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

		EvaluateAndAddKey(Time, TimeToCopyFrom, CurrentKey);
		AddedKeyHandles.Add(CurrentKey);
	}
	else
	{
		UpdateKeyWithExternalValue(Time);
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FIntegralCurveKeyAreaBase::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	if (!Curve.IsKeyHandleValid(KeyToDuplicate))
	{
		return TOptional<FKeyHandle>();
	}

	return Curve.AddKey(GetKeyTime(KeyToDuplicate), Curve.GetKey(KeyToDuplicate).Value);
}

void FIntegralCurveKeyAreaBase::DeleteKey(FKeyHandle KeyHandle)
{
	Curve.DeleteKey(KeyHandle);
}


TOptional<FLinearColor> FIntegralCurveKeyAreaBase::GetColor()
{
	return TOptional<FLinearColor>();
}


ERichCurveExtrapolation FIntegralCurveKeyAreaBase::GetExtrapolationMode(bool bPreInfinity) const
{
	return RCCE_None;
}


TSharedPtr<FStructOnScope> FIntegralCurveKeyAreaBase::GetKeyStruct(FKeyHandle KeyHandle)
{
	return MakeShareable(new FStructOnScope(FIntegralKey::StaticStruct(), (uint8*)&Curve.GetKey(KeyHandle)));
}


ERichCurveTangentMode FIntegralCurveKeyAreaBase::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	return RCTM_None;
}


ERichCurveInterpMode FIntegralCurveKeyAreaBase::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	return RCIM_None;
}


UMovieSceneSection* FIntegralCurveKeyAreaBase::GetOwningSection()
{
	return OwningSection;
}


float FIntegralCurveKeyAreaBase::GetKeyTime(FKeyHandle KeyHandle) const
{
	return Curve.GetKeyTime(KeyHandle);
}


FRichCurve* FIntegralCurveKeyAreaBase::GetRichCurve()
{
	return nullptr;
}


TArray<FKeyHandle> FIntegralCurveKeyAreaBase::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> OutKeyHandles;
	OutKeyHandles.Reserve(Curve.GetNumKeys());

	for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
	{
		OutKeyHandles.Add(It.Key());
	}

	return OutKeyHandles;
}


FKeyHandle FIntegralCurveKeyAreaBase::DilateKey(FKeyHandle KeyHandle, float Scale, float Origin)
{
	float NewKeyTime = Curve.GetKeyTime(KeyHandle);
	NewKeyTime = (NewKeyTime - Origin) * Scale + Origin;
	return Curve.SetKeyTime(KeyHandle, NewKeyTime);
}


FKeyHandle FIntegralCurveKeyAreaBase::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	return Curve.SetKeyTime(KeyHandle, Curve.GetKeyTime(KeyHandle) + DeltaPosition);
}


void FIntegralCurveKeyAreaBase::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime)
{
	Curve.SetKeyTime(KeyHandle, NewKeyTime);
}
