// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "IndexedCurve.generated.h"


/**
 * A curve base class which enables key handles to index lookups.
 *
 * @todo sequencer: Some heavy refactoring can be done here. Much more stuff can go in this base class.
 */
USTRUCT()
struct ENGINE_API FIndexedCurve
{
	GENERATED_USTRUCT_BODY()

public:

	/** Default constructor. */
	FIndexedCurve() { }

	virtual ~FIndexedCurve() { }

public:

	/** Gets the index of a handle, checks if the key handle is valid first. */
	int32 GetIndexSafe(FKeyHandle KeyHandle) const;

	/** Const iterator for the handles. */
	TMap<FKeyHandle, int32>::TConstIterator GetKeyHandleIterator() const;

	/** Get number of keys in curve. */
	virtual int32 GetNumKeys() const PURE_VIRTUAL(FIndexedCurve::GetNumKeys, return 0;);

	/** Checks to see if the key handle is valid for this curve. */
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const;

protected:

	/** Makes sure our handles are all valid and correct. */
	void EnsureAllIndicesHaveHandles() const;
	void EnsureIndexHasAHandle(int32 KeyIndex) const;

	/** Gets the index of a handle. */
	int32 GetIndex(FKeyHandle KeyHandle) const;

	/** Internal tool to get a handle from an index. */
	FKeyHandle GetKeyHandle(int32 KeyIndex) const;
	
protected:

	/** Map of which key handles go to which indices. */
	UPROPERTY(transient)
	mutable FKeyHandleMap KeyHandlesToIndices;
};
