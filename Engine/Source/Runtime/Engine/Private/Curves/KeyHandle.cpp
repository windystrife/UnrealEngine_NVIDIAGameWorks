// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Curves/KeyHandle.h"


/* FKeyHandleMap interface
 *****************************************************************************/

void FKeyHandleMap::Add( const FKeyHandle& InHandle, int32 InIndex )
{
	KeyHandlesToIndices.Add( InHandle, InIndex );
}


void FKeyHandleMap::Empty()
{
	KeyHandlesToIndices.Empty();
}


void FKeyHandleMap::Remove( const FKeyHandle& InHandle )
{
	KeyHandlesToIndices.Remove( InHandle );
}


const int32* FKeyHandleMap::Find( const FKeyHandle& InHandle ) const
{
	return KeyHandlesToIndices.Find( InHandle );
}


const FKeyHandle* FKeyHandleMap::FindKey( int32 KeyIndex ) const
{
	return KeyHandlesToIndices.FindKey( KeyIndex );
}


int32 FKeyHandleMap::Num() const
{
	return KeyHandlesToIndices.Num();
}


TMap<FKeyHandle, int32>::TConstIterator FKeyHandleMap::CreateConstIterator() const
{
	return KeyHandlesToIndices.CreateConstIterator();
}


TMap<FKeyHandle, int32>::TIterator FKeyHandleMap::CreateIterator() 
{
	return KeyHandlesToIndices.CreateIterator();
}


bool FKeyHandleMap::Serialize(FArchive& Ar)
{
	// only allow this map to be saved to the transaction buffer
	if( Ar.IsTransacting() )
	{
		Ar << KeyHandlesToIndices;
	}

	return true;
}


bool FKeyHandleMap::operator==(const FKeyHandleMap& Other) const
{
	if (KeyHandlesToIndices.Num() != Other.KeyHandlesToIndices.Num())
	{
		return false;
	}

	for (TMap<FKeyHandle, int32>::TConstIterator It(KeyHandlesToIndices); It; ++It)
	{
		int32 const* OtherVal = Other.KeyHandlesToIndices.Find(It.Key());
		if (!OtherVal || *OtherVal != It.Value() )
		{
			return false;
		}
	}

	return true;
}


bool FKeyHandleMap::operator!=(const FKeyHandleMap& Other) const
{
	return !(*this==Other);
}

void FKeyHandleMap::EnsureAllIndicesHaveHandles(int32 NumIndices)
{
	TArray<FKeyHandle, TInlineAllocator<16>> HandlesToRemove;

	for (const TPair<FKeyHandle, int32>& Pair: KeyHandlesToIndices)
	{
		if (Pair.Value >= NumIndices)
		{
			HandlesToRemove.Add(Pair.Key);
		}
	}

	for (const FKeyHandle& Handle : HandlesToRemove)
	{
		KeyHandlesToIndices.Remove(Handle);
	}

	for (int32 i = 0; i < NumIndices; ++i)
	{
		EnsureIndexHasAHandle(i);
	}
}

void FKeyHandleMap::EnsureIndexHasAHandle(int32 KeyIndex)
{
	const FKeyHandle* KeyHandle = KeyHandlesToIndices.FindKey(KeyIndex);
	if (!KeyHandle)
	{
		FKeyHandle OutKeyHandle = FKeyHandle();
		KeyHandlesToIndices.Add(OutKeyHandle, KeyIndex);
	}
}

int32 FKeyHandleLookupTable::GetIndex(FKeyHandle KeyHandle)
{
	const int32* Index = KeyHandlesToIndices.Find(KeyHandle);
	if (!Index)
	{
		// If it's not even in the map, there's no way this could be a valid handle for this container
		return INDEX_NONE;
	}
	else if (KeyHandles.IsValidIndex(*Index) && KeyHandles[*Index] == KeyHandle)
	{
		return *Index;
	}

	// slow lookup and cache
	const int32 NewCacheIndex = KeyHandles.IndexOfByPredicate(
		[KeyHandle](const TOptional<FKeyHandle>& PredKeyHandle)
		{
			return PredKeyHandle.IsSet() && PredKeyHandle.GetValue() == KeyHandle;
		}
	);

	if (NewCacheIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	KeyHandlesToIndices.Add(KeyHandle, NewCacheIndex);
	return NewCacheIndex;
}

FKeyHandle FKeyHandleLookupTable::FindOrAddKeyHandle(int32 Index)
{
	if (KeyHandles.IsValidIndex(Index) && KeyHandles[Index].IsSet())
	{
		return KeyHandles[Index].GetValue();
	}
	
	int32 NumToAdd = Index + 1 - KeyHandles.Num();
	if (NumToAdd > 0)
	{
		KeyHandles.AddDefaulted(NumToAdd);
	}

	// Allocate a new key handle
	FKeyHandle NewKeyHandle;

	KeyHandles[Index] = NewKeyHandle;
	KeyHandlesToIndices.Add(NewKeyHandle, Index);

	return NewKeyHandle;
}

void FKeyHandleLookupTable::MoveHandle(int32 OldIndex, int32 NewIndex)
{
	if (KeyHandles.IsValidIndex(OldIndex))
	{
		TOptional<FKeyHandle> Handle = KeyHandles[OldIndex];

		KeyHandles.RemoveAtSwap(OldIndex, 1, false);
		KeyHandles.Insert(Handle, NewIndex);
		if (Handle.IsSet())
		{
			KeyHandlesToIndices.Add(Handle.GetValue(), NewIndex);
		}
	}
}

FKeyHandle FKeyHandleLookupTable::AllocateHandle(int32 Index)
{
	FKeyHandle NewKeyHandle;
	KeyHandles.Insert(NewKeyHandle, Index);
	KeyHandlesToIndices.Add(NewKeyHandle, Index);
	return NewKeyHandle;
}

void FKeyHandleLookupTable::DeallocateHandle(int32 Index)
{
	TOptional<FKeyHandle> KeyHandle = KeyHandles[Index];
	KeyHandles.RemoveAtSwap(Index, 1, false);
	if (KeyHandle.IsSet())
	{
		KeyHandlesToIndices.Remove(KeyHandle.GetValue());
	}
}

void FKeyHandleLookupTable::Reset()
{
	KeyHandles.Reset();
	KeyHandlesToIndices.Reset();
}
