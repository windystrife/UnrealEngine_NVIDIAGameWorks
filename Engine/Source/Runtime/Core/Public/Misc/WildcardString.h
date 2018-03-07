// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

/**
 * Implements a string with wild card pattern matching abilities.
 */
class FWildcardString
	: public FString
{
public:

	/** Default constructor. */
	FWildcardString( )
		: FString()
	{ }

	/**
	 * Creates and initializes a new instance with the specified pattern.
	 *
	 * @param Pattern The pattern string.
	 */
	FWildcardString( const FString& Pattern )
		: FString(Pattern)
	{ }

	/**
	 * Creates and initializes a new instance with the specified pattern.
	 *
	 * @param Pattern The pattern string.
	 */
	FWildcardString( const TCHAR* Pattern )
		: FString(Pattern)
	{ }

public:

	/**
	 * Checks whether this string contains wild card characters.
	 *
	 * @return true if this string contains wild cards, false otherwise.
	 */
	bool ContainsWildcards( ) const
	{
		return ContainsWildcards(**this);
	}

	/**
	 * Matches the given input string to this wild card pattern.
	 *
	 * @param Input The string to match.
	 * @return true if the input string matches this pattern, false otherwise.
	 */
	bool IsMatch( const TCHAR* Input ) const
	{
		return IsMatch(**this, Input);
	}

	/**
	 * Matches the given input string to this wild card pattern.
	 *
	 * @param Input The string to match.
	 * @return true if the input string matches this pattern, false otherwise.
	 */
	bool IsMatch( const FString& Input ) const
	{
		return IsMatch(**this, *Input);
	}

public:

	/**
	 * Checks whether the specified pattern contains wild card characters.
	 *
	 * @param Pattern The string to check.
	 * @return true if the string contains wild cards, false otherwise.
	 */
	static CORE_API bool ContainsWildcards( const TCHAR* Pattern );

	/**
	 * Non-recursive wild card string matching algorithm.
	 *
	 * @param Pattern The pattern to match.
	 * @param Input The input string to check.
	 */
	static CORE_API bool IsMatch( const TCHAR* Pattern, const TCHAR* Input );

protected:

	/** Holds the string terminator character. */
	static CORE_API const TCHAR EndOfString = TCHAR('\0');

	/** Holds the wild card that matches exactly one character (default is '?'). */
	static CORE_API const TCHAR ExactWildcard = TCHAR('?');

	/** Holds the wild card that matches a sequence of characters (default is '*'). */
	static CORE_API const TCHAR SequenceWildcard = TCHAR('*');
};
