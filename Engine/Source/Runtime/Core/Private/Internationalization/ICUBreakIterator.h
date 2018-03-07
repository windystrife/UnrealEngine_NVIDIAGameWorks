// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Set.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/IBreakIterator.h"

#if UE_ENABLE_ICU

THIRD_PARTY_INCLUDES_START
	#include <unicode/brkiter.h>
THIRD_PARTY_INCLUDES_END

/**
 * Manages the lifespan of ICU break iterators
 */
class FICUBreakIteratorManager
{
public:
	static void Create();
	static void Destroy();
	static FICUBreakIteratorManager& Get();

	TWeakPtr<icu::BreakIterator> CreateCharacterBoundaryIterator();
	TWeakPtr<icu::BreakIterator> CreateWordBreakIterator();
	TWeakPtr<icu::BreakIterator> CreateLineBreakIterator();
	void DestroyIterator(TWeakPtr<icu::BreakIterator>& InIterator);

private:
	static FICUBreakIteratorManager* Singleton;
	FCriticalSection AllocatedIteratorsCS;
	TSet<TSharedPtr<icu::BreakIterator>> AllocatedIterators;
};

/**
 * Wraps an ICU break iterator instance inside our own break iterator API
 */
class FICUBreakIterator : public IBreakIterator
{
public:
	FICUBreakIterator(TWeakPtr<icu::BreakIterator>&& InICUBreakIteratorHandle);
	virtual ~FICUBreakIterator();

	virtual void SetString(const FText& InText) override;
	virtual void SetString(const FString& InString) override;
	virtual void SetString(const TCHAR* const InString, const int32 InStringLength) override;
	virtual void ClearString() override;

	virtual int32 GetCurrentPosition() const override;

	virtual int32 ResetToBeginning() override;
	virtual int32 ResetToEnd() override;

	virtual int32 MoveToPrevious() override;
	virtual int32 MoveToNext() override;
	virtual int32 MoveToCandidateBefore(const int32 InIndex) override;
	virtual int32 MoveToCandidateAfter(const int32 InIndex) override;

protected:
	TSharedRef<icu::BreakIterator> GetInternalBreakIterator() const;

private:
	TWeakPtr<icu::BreakIterator> ICUBreakIteratorHandle;
};

#endif
