// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Internationalization/IBreakIterator.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/BreakIterator.h"
#include "Internationalization/Text.h"

#if !UE_ENABLE_ICU

class FLegacyWordBreakIterator : public IBreakIterator
{
public:
	FLegacyWordBreakIterator();

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

private:
	FString String;
	int32 CurrentPosition;
};

FLegacyWordBreakIterator::FLegacyWordBreakIterator()
	: String()
	, CurrentPosition(0)
{
}

void FLegacyWordBreakIterator::SetString(const FText& InText)
{
	String = InText.ToString();
	ResetToBeginning();
}

void FLegacyWordBreakIterator::SetString(const FString& InString)
{
	String = InString;
	ResetToBeginning();
}

void FLegacyWordBreakIterator::SetString(const TCHAR* const InString, const int32 InStringLength) 
{
	String = FString(InString, InStringLength);
	ResetToBeginning();
}

void FLegacyWordBreakIterator::ClearString()
{
	String = FString();
	ResetToBeginning();
}

int32 FLegacyWordBreakIterator::GetCurrentPosition() const
{
	return CurrentPosition;
}

int32 FLegacyWordBreakIterator::ResetToBeginning()
{
	return CurrentPosition = 0;
}

int32 FLegacyWordBreakIterator::ResetToEnd()
{
	return CurrentPosition = String.Len();
}

int32 FLegacyWordBreakIterator::MoveToPrevious()
{
	return MoveToCandidateBefore(CurrentPosition);
}

int32 FLegacyWordBreakIterator::MoveToNext()
{
	return MoveToCandidateAfter(CurrentPosition);
}

int32 FLegacyWordBreakIterator::MoveToCandidateBefore(const int32 InIndex)
{
	// Can break between char and whitespace.
	for(CurrentPosition = FMath::Clamp( InIndex - 1, 0, String.Len() ); CurrentPosition >= 1; --CurrentPosition)
	{
		bool bPreviousCharWasWhitespace = FChar::IsWhitespace(String[CurrentPosition - 1]);
		bool bCurrentCharIsWhiteSpace = FChar::IsWhitespace(String[CurrentPosition]);
		if(bPreviousCharWasWhitespace != bCurrentCharIsWhiteSpace)
		{
			break;
		}
	}

	return CurrentPosition >= InIndex ? INDEX_NONE : CurrentPosition;
}

int32 FLegacyWordBreakIterator::MoveToCandidateAfter(const int32 InIndex)
{
	// Can break between char and whitespace.
	for(CurrentPosition = FMath::Clamp( InIndex + 1, 0, String.Len() ); CurrentPosition < String.Len(); ++CurrentPosition)
	{
		bool bPreviousCharWasWhitespace = FChar::IsWhitespace(String[CurrentPosition - 1]);
		bool bCurrentCharIsWhiteSpace = FChar::IsWhitespace(String[CurrentPosition]);
		if(bPreviousCharWasWhitespace != bCurrentCharIsWhiteSpace)
		{
			break;
		}
	}

	return CurrentPosition <= InIndex ? INDEX_NONE : CurrentPosition;
}

TSharedRef<IBreakIterator> FBreakIterator::CreateWordBreakIterator()
{
	return MakeShareable(new FLegacyWordBreakIterator());
}

#endif
