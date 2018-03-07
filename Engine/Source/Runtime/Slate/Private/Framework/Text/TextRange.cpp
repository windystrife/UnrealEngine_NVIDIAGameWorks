// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/TextRange.h"

void FTextRange::CalculateLineRangesFromString(const FString& Input, TArray<FTextRange>& LineRanges)
{
	int32 LineBeginIndex = 0;

	// Loop through splitting at new-lines
	const TCHAR* const InputStart = *Input;
	for(const TCHAR* CurrentChar = InputStart; CurrentChar && *CurrentChar; ++CurrentChar)
	{
		// Handle a chain of \r\n slightly differently to stop the FChar::IsLinebreak adding two separate new-lines
		const bool bIsWindowsNewLine = (*CurrentChar == '\r' && *(CurrentChar + 1) == '\n');
		if(bIsWindowsNewLine || FChar::IsLinebreak(*CurrentChar))
		{
			const int32 LineEndIndex = (CurrentChar - InputStart);
			check(LineEndIndex >= LineBeginIndex);
			LineRanges.Emplace(FTextRange(LineBeginIndex, LineEndIndex));

			if(bIsWindowsNewLine)
			{
				++CurrentChar; // skip the \n of the \r\n chain
			}
			LineBeginIndex = (CurrentChar - InputStart) + 1; // The next line begins after the end of the current line
		}
	}

	// Process any remaining string after the last new-line
	if(LineBeginIndex <= Input.Len())
	{
		LineRanges.Emplace(FTextRange(LineBeginIndex, Input.Len()));
	}
}
