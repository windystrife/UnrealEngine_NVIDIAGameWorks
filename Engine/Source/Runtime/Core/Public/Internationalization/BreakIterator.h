// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/IBreakIterator.h"

struct CORE_API FBreakIterator
{
	/**
	 * Create a new instance of a break iterator designed to locate character boundary points within a string
	 */
	static TSharedRef<IBreakIterator> CreateCharacterBoundaryIterator();

	/**
	 * Create a new instance of a break iterator designed to locate word boundary points within a string
	 */
	static TSharedRef<IBreakIterator> CreateWordBreakIterator();

	/**
	 * Create a new instance of a break iterator designed to locate appropriate soft-wrapping points within a string
	 * Note that these aren't newlines as such, but rather places where the line could be soft-wrapped when displaying the text
	 */
	static TSharedRef<IBreakIterator> CreateLineBreakIterator();

	/**
	 * Create a new instance of a break iterator which is designed to locate appropriate soft-wrapping points within a string by searching for CamelCase boundaries
	 * This may produce odd results when used with languages which don't have a concept of cases for characters, however should be fine for parsing variable or asset names
	 */
	static TSharedRef<IBreakIterator> CreateCamelCaseBreakIterator();
};
