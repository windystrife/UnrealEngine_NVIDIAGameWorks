// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerKeyTimeCache.h"
#include "MovieSceneSection.h"
#include "Algo/Sort.h"
#include "Algo/BinarySearch.h"

void FSequencerCachedKeys::Update(TSharedRef<IKeyArea> KeyArea)
{
	if (!CachedKeys.IsSet())
	{
		CachedKeys.Emplace();
	}

	UMovieSceneSection* Section = KeyArea->GetOwningSection();
	if (!Section || !CachedSignature.IsValid() || Section->GetSignature() != CachedSignature)
	{
		CachedSignature = Section ? Section->GetSignature() : FGuid();

		CachedKeys->Reset();

		// Generate and cache
		for (FKeyHandle KeyHandle : KeyArea->GetUnsortedKeyHandles())
		{
			CachedKeys->Add(FSequencerCachedKey{ KeyHandle, KeyArea->GetKeyTime(KeyHandle) });
		}

		Algo::SortBy(CachedKeys.GetValue(), &FSequencerCachedKey::Time);
	}
}

TArrayView<const FSequencerCachedKey> FSequencerCachedKeys::GetKeysInRange(TRange<float> ViewRange) const
{
	// Binary search the first time that's >= the lower bound
	int32 FirstVisibleIndex = Algo::LowerBoundBy(CachedKeys.GetValue(), ViewRange.GetLowerBoundValue(), &FSequencerCachedKey::Time, TLess<float>());
	// Binary search the last time that's > the upper bound
	int32 LastVisibleIndex = Algo::UpperBoundBy(CachedKeys.GetValue(), ViewRange.GetUpperBoundValue(), &FSequencerCachedKey::Time, TLess<float>());

	int32 Num = LastVisibleIndex - FirstVisibleIndex;
	if (CachedKeys->IsValidIndex(FirstVisibleIndex) && LastVisibleIndex <= CachedKeys->Num())
	{
		return MakeArrayView(&CachedKeys.GetValue()[FirstVisibleIndex], Num);
	}

	return TArrayView<const FSequencerCachedKey>();
}