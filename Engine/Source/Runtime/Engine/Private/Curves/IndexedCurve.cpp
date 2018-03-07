// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Curves/IndexedCurve.h"


/* FIndexedCurve interface
 *****************************************************************************/

int32 FIndexedCurve::GetIndexSafe(FKeyHandle KeyHandle) const
{
	return IsKeyHandleValid(KeyHandle) ? *KeyHandlesToIndices.Find(KeyHandle) : INDEX_NONE;
}


TMap<FKeyHandle, int32>::TConstIterator FIndexedCurve::GetKeyHandleIterator() const
{
	EnsureAllIndicesHaveHandles();
	return KeyHandlesToIndices.CreateConstIterator();
}


bool FIndexedCurve::IsKeyHandleValid(FKeyHandle KeyHandle) const
{
	EnsureAllIndicesHaveHandles();
	return KeyHandlesToIndices.Find(KeyHandle) != NULL;
}


/* FIndexedCurve implementation
 *****************************************************************************/

void FIndexedCurve::EnsureAllIndicesHaveHandles() const
{
	const int32 NumKeys = GetNumKeys();
	if (KeyHandlesToIndices.Num() != NumKeys)
	{
		KeyHandlesToIndices.EnsureAllIndicesHaveHandles(NumKeys);
	}
}


void FIndexedCurve::EnsureIndexHasAHandle(int32 KeyIndex) const
{
	KeyHandlesToIndices.EnsureIndexHasAHandle(KeyIndex);
}


int32 FIndexedCurve::GetIndex(FKeyHandle KeyHandle) const
{
	return *KeyHandlesToIndices.Find(KeyHandle);
}


FKeyHandle FIndexedCurve::GetKeyHandle(int32 KeyIndex) const
{
	check(KeyIndex >= 0 && KeyIndex < GetNumKeys());
	EnsureIndexHasAHandle(KeyIndex);

	return *KeyHandlesToIndices.FindKey(KeyIndex);
}
