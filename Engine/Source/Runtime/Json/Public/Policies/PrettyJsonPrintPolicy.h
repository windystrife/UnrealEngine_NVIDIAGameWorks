// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Policies/JsonPrintPolicy.h"

/**
 * Template for print policies that generate human readable output.
 *
 * @param CharType The type of characters to print, i.e. TCHAR or ANSICHAR.
 */
template <class CharType>
struct TPrettyJsonPrintPolicy
	: public TJsonPrintPolicy<CharType>
{
	static inline void WriteLineTerminator( FArchive* Stream )
	{
		TJsonPrintPolicy<CharType>::WriteString(Stream, LINE_TERMINATOR);
	}

	static inline void WriteTabs( FArchive* Stream, int32 Count )
	{
		CharType Tab = CharType('\t');

		for (int32 i = 0; i < Count; ++i)
		{
			TJsonPrintPolicy<CharType>::WriteChar(Stream, Tab);
		}
	}

	static inline void WriteSpace( FArchive* Stream )
	{
		TJsonPrintPolicy<CharType>::WriteChar(Stream, CharType(' '));
	}
};
