// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

/** FObjectKey is an immutable, copyable key which can be used to uniquely identify an object for the lifetime of the application */
struct FObjectKey
{
public:
	/** Default constructor */
	FORCEINLINE FObjectKey()
		: ObjectIndex(INDEX_NONE)
		, ObjectSerialNumber(0)
	{
	}

	/** Construct from an object pointer */
	FORCEINLINE FObjectKey(const UObject *Object)
		: ObjectIndex(INDEX_NONE)
		, ObjectSerialNumber(0)
	{
		if (Object)
		{
			FWeakObjectPtr Weak(Object);
			ObjectIndex = Weak.ObjectIndex;
			ObjectSerialNumber = Weak.ObjectSerialNumber;
		}
	}

	/** Compare this key with another */
	FORCEINLINE bool operator==(const FObjectKey &Other) const
	{
		return ObjectIndex == Other.ObjectIndex && ObjectSerialNumber == Other.ObjectSerialNumber;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator!=(const FObjectKey &Other) const
	{
		return ObjectIndex != Other.ObjectIndex || ObjectSerialNumber != Other.ObjectSerialNumber;
	}

	/**
	 * Attempt to access the object from which this key was constructed.
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	UObject* ResolveObjectPtr() const
	{
		FWeakObjectPtr WeakPtr;
		WeakPtr.ObjectIndex = ObjectIndex;
		WeakPtr.ObjectSerialNumber = ObjectSerialNumber;

		return WeakPtr.Get();
	}

	/** Hash function */
	friend uint32 GetTypeHash(const FObjectKey& Key)
	{
		return HashCombine(Key.ObjectIndex, Key.ObjectSerialNumber);
	}

private:

	int32		ObjectIndex;
	int32		ObjectSerialNumber;
};
