// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/IBreakIterator.h"

/**
 * Base class for camel case break iterators
 * A derived type must provide a way to tokenize the string for processing
 */
class FCamelCaseBreakIterator : public IBreakIterator
{
public:
	FCamelCaseBreakIterator();

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
	enum class ETokenType : uint8
	{
		Uppercase,
		Lowercase,
		Digit,
		Null,
		Other,
	};

	struct FToken
	{
		FToken(const ETokenType InTokenType, const int32 InStrIndex)
			: TokenType(InTokenType)
			, StrIndex(InStrIndex)
		{
		}

		ETokenType TokenType;
		int32 StrIndex;
	};

	virtual void TokenizeString(TArray<FToken>& OutTokens) = 0;

protected:
	void UpdateBreakPointsArray();
	void PopulateBreakPointsArray(const TArray<FToken>& InTokens);

	FString String;
	int32 CurrentPosition;
	TArray<int32> BreakPoints;
};
