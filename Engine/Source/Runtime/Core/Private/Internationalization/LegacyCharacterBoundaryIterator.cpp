// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Internationalization/IBreakIterator.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/BreakIterator.h"
#include "Internationalization/Text.h"

#if !UE_ENABLE_ICU

class FLegacyCharacterBoundaryIterator : public IBreakIterator
{
public:
	FLegacyCharacterBoundaryIterator();

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

FLegacyCharacterBoundaryIterator::FLegacyCharacterBoundaryIterator()
	: String()
	, CurrentPosition(0)
{
}

void FLegacyCharacterBoundaryIterator::SetString(const FText& InText)
{
	String = InText.ToString();
	ResetToBeginning();
}

void FLegacyCharacterBoundaryIterator::SetString(const FString& InString)
{
	String = InString;
	ResetToBeginning();
}

void FLegacyCharacterBoundaryIterator::SetString(const TCHAR* const InString, const int32 InStringLength) 
{
	String = FString(InString, InStringLength);
	ResetToBeginning();
}

void FLegacyCharacterBoundaryIterator::ClearString()
{
	String = FString();
	ResetToBeginning();
}

int32 FLegacyCharacterBoundaryIterator::GetCurrentPosition() const
{
	return CurrentPosition;
}

int32 FLegacyCharacterBoundaryIterator::ResetToBeginning()
{
	return CurrentPosition = 0;
}

int32 FLegacyCharacterBoundaryIterator::ResetToEnd()
{
	return CurrentPosition = String.Len();
}

int32 FLegacyCharacterBoundaryIterator::MoveToPrevious()
{
	return MoveToCandidateBefore(CurrentPosition);
}

int32 FLegacyCharacterBoundaryIterator::MoveToNext()
{
	return MoveToCandidateAfter(CurrentPosition);
}

int32 FLegacyCharacterBoundaryIterator::MoveToCandidateBefore(const int32 InIndex)
{
	CurrentPosition = FMath::Clamp( InIndex - 1, 0, String.Len() );
	return CurrentPosition >= InIndex ? INDEX_NONE : CurrentPosition;
}

int32 FLegacyCharacterBoundaryIterator::MoveToCandidateAfter(const int32 InIndex)
{
	CurrentPosition = FMath::Clamp( InIndex + 1, 0, String.Len() );
	return CurrentPosition <= InIndex ? INDEX_NONE : CurrentPosition;
}

TSharedRef<IBreakIterator> FBreakIterator::CreateCharacterBoundaryIterator()
{
	return MakeShareable(new FLegacyCharacterBoundaryIterator());
}

#endif
