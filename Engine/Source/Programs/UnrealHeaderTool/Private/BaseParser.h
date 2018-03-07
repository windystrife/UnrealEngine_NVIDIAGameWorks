// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FToken;

/////////////////////////////////////////////////////
// FBaseParser


enum class ESymbolParseOption
{
	Normal,
	CloseTemplateBracket
};

// A specifier with optional value
struct FPropertySpecifier
{
public:
	explicit FPropertySpecifier(FString&& InKey)
		: Key(MoveTemp(InKey))
	{
	}

	explicit FPropertySpecifier(const FString& InKey)
		: Key(InKey)
	{
	}

	FString Key;
	TArray<FString> Values;

	FString ConvertToString() const;
};

//
// Base class of header parsers.
//

class FBaseParser
{
protected:
	FBaseParser();

public:
	// Input text.
	const TCHAR* Input;

	// Length of input text.
	int32 InputLen;

	// Current position in text.
	int32 InputPos;

	// Current line in text.
	int32 InputLine;

	// Position previous to last GetChar() call.
	int32 PrevPos;

	// Line previous to last GetChar() call.
	int32 PrevLine;

	// Previous comment parsed by GetChar() call.
	FString PrevComment;

	// Number of statements parsed.
	int32 StatementsParsed;

	// Total number of lines parsed.
	int32 LinesParsed;

	void ResetParser(const TCHAR* SourceBuffer, int32 StartingLineNumber = 1);

	// Low-level parsing functions.
	TCHAR GetChar( bool Literal = false );
	TCHAR PeekChar();
	TCHAR GetLeadingChar();
	void UngetChar();

	/**
	 * Tests if a character is an end-of-line character.
	 *
	 * @param	c	The character to test.
	 *
	 * @return	true if c is an end-of-line character, false otherwise.
	 */
	static bool IsEOL( TCHAR c );

	/**
	 * Tests if a character is a whitespace character.
	 *
	 * @param	c	The character to test.
	 *
	 * @return	true if c is an whitespace character, false otherwise.
	 */
	static bool IsWhitespace( TCHAR c );

	/**
	 * Gets the next token from the input stream, advancing the variables which keep track of the current input position and line.
	 *
	 * @param	Token						receives the value of the parsed text; if Token is pre-initialized, special logic is performed
	 *										to attempt to evaluated Token in the context of that type.  Useful for distinguishing between ambigous symbols
	 *										like enum tags.
	 * @param	NoConsts					specify true to indicate that tokens representing literal const values are not allowed.
	 * @param	ParseTemplateClosingBracket	specify true to treat >> as two template closing brackets instead of shift operator.
	 *
	 * @return	true if a token was successfully processed, false otherwise.
	 */
	bool GetToken( FToken& Token, bool bNoConsts = false, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal );

	/**
	 * Put all text from the current position up to either EOL or the StopToken
	 * into Token.  Advances the compiler's current position.
	 *
	 * @param	Token	[out] will contain the text that was parsed
	 * @param	StopChar	stop processing when this character is reached
	 *
	 * @return	true if a token was parsed
	 */
	bool GetRawToken( FToken& Token, TCHAR StopChar = TCHAR('\n') );

	// Doesn't quit if StopChar is found inside a double-quoted string, but does not support quote escapes
	bool GetRawTokenRespectingQuotes( FToken& Token, TCHAR StopChar = TCHAR('\n') );

	void UngetToken( const FToken& Token );
	bool GetIdentifier( FToken& Token, bool bNoConsts = false );
	bool GetSymbol( FToken& Token );

	/**
	 * Get an int constant
	 * @return true on success, otherwise false.
	 */
	bool GetConstInt(int32& Result, const TCHAR* Tag = NULL);
	bool GetConstInt64(int64& Result, const TCHAR* Tag = NULL);

	// Matching predefined text.
	bool MatchIdentifier( FName Match );
	bool MatchIdentifier( const TCHAR* Match );
	bool MatchConstInt( const TCHAR* Match );
	bool PeekIdentifier( FName Match );
	bool PeekIdentifier( const TCHAR* Match );
	bool MatchSymbol( const TCHAR* Match, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal );
	void MatchSemi();
	bool PeekSymbol( const TCHAR* Match );

	// Requiring predefined text.
	void RequireIdentifier( FName Match, const TCHAR* Tag );
	void RequireIdentifier( const TCHAR* Match, const TCHAR* Tag );
	void RequireSymbol( const TCHAR* Match, const TCHAR* Tag, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal );

	/** Clears out the stored comment. */
	void ClearComment();

	// Reads a new-style value
	//@TODO: UCREMOVAL: Needs a better name
	FString ReadNewStyleValue(const FString& TypeOfSpecifier);

	// Reads ['(' Value [',' Value]* ')'] and places each value into the Items array
	bool ReadOptionalCommaSeparatedListInParens(TArray<FString>& Items, const FString& TypeOfSpecifier);

	//////////////
	// Complicated* parsing code that needs to be shared between the preparser and the parser
	// (* i.e., doesn't really belong in the base parser)

	// Expecting Name | (MODULE_API Name)
	//  Places Name into DeclaredName
	//  Places MODULE_API (where MODULE varies) into RequiredAPIMacroIfPresent
	// FailureMessage is printed out if the expectation is broken.
	void ParseNameWithPotentialAPIMacroPrefix(FString& DeclaredName, FString& RequiredAPIMacroIfPresent, const TCHAR* FailureMessage);

	// Reads a set of specifiers (with optional values) inside the () of a new-style metadata macro like UPROPERTY or UFUNCTION
	void ReadSpecifierSetInsideMacro(TArray<FPropertySpecifier>& SpecifiersFound, const FString& TypeOfSpecifier, TMap<FName, FString>& MetaData);

	// Validates and inserts one key-value pair into the meta data map
	static void InsertMetaDataPair(TMap<FName, FString>& MetaData, const FString& InKey, const FString& InValue);

	//////////////
};
