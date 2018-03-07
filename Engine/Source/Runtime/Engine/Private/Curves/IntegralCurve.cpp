// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Curves/IntegralCurve.h"


int32 FIntegralCurve::GetNumKeys() const
{
	return Keys.Num();
}


bool FIntegralCurve::IsKeyHandleValid(FKeyHandle KeyHandle) const
{
	bool bValid = false;
	if (FIndexedCurve::IsKeyHandleValid(KeyHandle))
	{
		bValid = Keys.IsValidIndex(GetIndex(KeyHandle));
	}
	return bValid;
}


int32 FIntegralCurve::Evaluate(float Time, int32 InDefaultValue) const
{
	// If the default value hasn't been initialized, use the incoming default value
	int32 ReturnVal = DefaultValue == MAX_int32 ? InDefaultValue : DefaultValue;

	if (Keys.Num() == 0 || (bUseDefaultValueBeforeFirstKey && Time < Keys[0].Time))
	{
		// If no keys in curve, or bUseDefaultValueBeforeFirstKey is set and the time is before the first key, return the Default value.
	}
	else if (Keys.Num() < 2 || Time < Keys[0].Time)
	{
		// There is only one key or the time is before the first value. Return the first value
		ReturnVal = Keys[0].Value;
	}
	else if (Time < Keys[Keys.Num() - 1].Time)
	{
		// The key is in the range of Key[0] to Keys[Keys.Num()-1].  Find it by searching
		for (int32 i = 0; i < Keys.Num(); ++i)
		{
			if (Time < Keys[i].Time)
			{
				ReturnVal = Keys[FMath::Max(0, i - 1)].Value;
				break;
			}
		}
	}
	else
	{
		// Key is beyon the last point in the curve.  Return it's value
		ReturnVal = Keys[Keys.Num() - 1].Value;
	}

	return ReturnVal;
}


TArray<FIntegralKey>::TConstIterator FIntegralCurve::GetKeyIterator() const
{
	return Keys.CreateConstIterator();
}


FKeyHandle FIntegralCurve::AddKey(float InTime, int32 InValue, FKeyHandle InKeyHandle)
{
	int32 Index = 0;
	for (; Index < Keys.Num() && Keys[Index].Time < InTime; ++Index);
	Keys.Insert(FIntegralKey(InTime, InValue), Index);

	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		int32& KeyIndex = It.Value();
		if (KeyIndex >= Index)
		{
			++KeyIndex;
		}
	}

	KeyHandlesToIndices.Add(InKeyHandle, Index);

	return GetKeyHandle(Index);
}


void FIntegralCurve::DeleteKey(FKeyHandle InKeyHandle)
{
	int32 Index = GetIndex(InKeyHandle);

	Keys.RemoveAt(Index);

	KeyHandlesToIndices.Remove(InKeyHandle);

	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		int32& KeyIndex = It.Value();
		if (KeyIndex >= Index)
		{
			--KeyIndex;
		}
	}
}


FKeyHandle FIntegralCurve::UpdateOrAddKey(float InTime, int32 Value, float KeyTimeTolerance)
{
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		float KeyTime = Keys[KeyIndex].Time;

		if (FMath::IsNearlyEqual(KeyTime, InTime, KeyTimeTolerance))
		{
			Keys[KeyIndex].Value = Value;
			return GetKeyHandle(KeyIndex);
		}

		if (KeyTime > InTime)
		{
			// All the rest of the keys exist after the key we want to add
			// so there is no point in searching
			break;
		}
	}

	// A key wasnt found, add it now
	return AddKey(InTime, Value);
}


FKeyHandle FIntegralCurve::SetKeyTime(FKeyHandle KeyHandle, float NewTime)
{
	if (!IsKeyHandleValid(KeyHandle))
	{
		return KeyHandle;
	}

	FIntegralKey OldKey = GetKey(KeyHandle);

	DeleteKey(KeyHandle);
	AddKey(NewTime, OldKey.Value, KeyHandle);

	// Copy all properties from old key, but then fix time to be the new time
	GetKey(KeyHandle) = OldKey;
	GetKey(KeyHandle).Time = NewTime;

	return KeyHandle;
}


float FIntegralCurve::GetKeyTime(FKeyHandle KeyHandle) const
{
	if (!IsKeyHandleValid(KeyHandle))
	{
		return 0.f;
	}

	return GetKey(KeyHandle).Time;
}


void FIntegralCurve::SetKeyValue(FKeyHandle KeyHandle, int32 NewValue)
{
	if (IsKeyHandleValid(KeyHandle))
	{
		GetKey(KeyHandle).Value = NewValue;
	}
}


int32 FIntegralCurve::GetKeyValue(FKeyHandle KeyHandle) const
{
	if (!IsKeyHandleValid(KeyHandle))
	{
		return 0.f;
	}

	return GetKey(KeyHandle).Value;
}


void FIntegralCurve::ShiftCurve(float DeltaTime)
{
	TSet<FKeyHandle> KeyHandles;
	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		FKeyHandle& KeyHandle = It.Key();
		KeyHandles.Add(KeyHandle);
	}

	ShiftCurve(DeltaTime, KeyHandles);
}


void FIntegralCurve::ShiftCurve(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		const FKeyHandle& KeyHandle = It.Key();
		if (KeyHandles.Num() != 0 && KeyHandles.Contains(KeyHandle))
		{
			SetKeyTime(KeyHandle, GetKeyTime(KeyHandle) + DeltaTime);
		}
	}
}


void FIntegralCurve::ScaleCurve(float ScaleOrigin, float ScaleFactor)
{
	TSet<FKeyHandle> KeyHandles;
	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		FKeyHandle& KeyHandle = It.Key();
		KeyHandles.Add(KeyHandle);
	}

	ScaleCurve(ScaleOrigin, ScaleFactor, KeyHandles);
}


void FIntegralCurve::ScaleCurve(float ScaleOrigin, float ScaleFactor, TSet<FKeyHandle>& KeyHandles)
{
	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		const FKeyHandle& KeyHandle = It.Key();
		if (KeyHandles.Num() != 0 && KeyHandles.Contains(KeyHandle))
		{
			SetKeyTime(KeyHandle, (GetKeyTime(KeyHandle) - ScaleOrigin) * ScaleFactor + ScaleOrigin);
		}
	}
}


FIntegralKey& FIntegralCurve::GetKey(FKeyHandle KeyHandle)
{
	EnsureAllIndicesHaveHandles();
	return Keys[GetIndex(KeyHandle)];
}


FIntegralKey FIntegralCurve::GetKey(FKeyHandle KeyHandle) const
{
	EnsureAllIndicesHaveHandles();
	return Keys[GetIndex(KeyHandle)];
}


FKeyHandle FIntegralCurve::FindKey(float KeyTime, float KeyTimeTolerance) const
{
	int32 Start = 0;
	int32 End = Keys.Num() - 1;

	// Binary search since the keys are in sorted order
	while (Start <= End)
	{
		int32 TestPos = Start + (End - Start) / 2;
		float TestKeyTime = Keys[TestPos].Time;

		if (FMath::IsNearlyEqual(TestKeyTime, KeyTime, KeyTimeTolerance))
		{
			return GetKeyHandle(TestPos);
		}
		else if (TestKeyTime < KeyTime)
		{
			Start = TestPos + 1;
		}
		else
		{
			End = TestPos - 1;
		}
	}

	return FKeyHandle();
}

FKeyHandle FIntegralCurve::FindKeyBeforeOrAt(float KeyTime) const
{
	// If there are no keys or the time is before the first key return an invalid handle.
	if (Keys.Num() == 0 || KeyTime < Keys[0].Time)
	{
		return FKeyHandle();
	}

	// If the time is after or at the last key return the last key.
	if (KeyTime >= Keys[Keys.Num() - 1].Time)
	{
		return GetKeyHandle(Keys.Num() - 1);
	}

	// Otherwise binary search to find the handle of the nearest key at or before the time.
	int32 Start = 0;
	int32 End = Keys.Num() - 1;
	int32 FoundIndex = -1;
	while (FoundIndex < 0)
	{
		int32 TestPos = (Start + End) / 2;
		float TestKeyTime = Keys[TestPos].Time;
		float NextTestKeyTime = Keys[TestPos + 1].Time;
		if (TestKeyTime <= KeyTime)
		{
			if (NextTestKeyTime > KeyTime)
			{
				FoundIndex = TestPos;
			}
			else
			{
				Start = TestPos + 1;
			}
		}
		else
		{
			End = TestPos;
		}
	}
	return GetKeyHandle(FoundIndex);
}
