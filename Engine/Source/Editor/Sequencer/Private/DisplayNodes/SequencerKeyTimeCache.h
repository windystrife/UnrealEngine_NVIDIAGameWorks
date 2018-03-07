// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Range.h"
#include "Guid.h"
#include "ArrayView.h"
#include "Curves/KeyHandle.h"

#include "SharedPointer.h"
#include "Optional.h"
#include "IKeyArea.h"

/** Key information that has been cached to avoid expensive operations */
struct FSequencerCachedKey
{
	/** The key handle */
	FKeyHandle Handle;
	/** The local time of the key */
	float Time;
};

/** Simple structure that caches the sorted key times for a given key area */
struct FSequencerCachedKeys
{
	/** Update this cache to store key times and handles from the specified key area */
	void Update(TSharedRef<IKeyArea> KeyArea);

	/** Get an view of the cached array for keys that fall within the specified range */
	TArrayView<const FSequencerCachedKey> GetKeysInRange(TRange<float> Range) const;

private:
	/** Cached key information */
	TOptional<TArray<FSequencerCachedKey>> CachedKeys;
	/** The guid with which the above array was generated */
	FGuid CachedSignature;
};

