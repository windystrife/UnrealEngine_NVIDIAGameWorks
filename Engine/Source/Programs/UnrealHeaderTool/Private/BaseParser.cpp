// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BaseParser.h"
#include "UnrealHeaderTool.h"
#include "UObject/NameTypes.h"
#include "UObject/ErrorException.h"

#include "ParserHelper.h"


namespace
{
	namespace EMetadataValueArgument
	{
		enum Type
		{
			None,
			Required,
			Optional
		};
	}

	namespace EMetadataValueAction
	{
		enum Type
		{
			Remove,
			Add
		};
	}

	struct FMetadataValueAction
	{
		FMetadataValueAction(const TCHAR* InMapping, const TCHAR* InDefaultValue, EMetadataValueAction::Type InValueAction)
		: Mapping         (InMapping)
		, DefaultValue    (InDefaultValue)
		, ValueAction     (InValueAction)
		{
		}

		FString                    Mapping;
		FString                    DefaultValue;
		EMetadataValueAction::Type ValueAction;
	};

	struct FMetadataKeyword
	{
		FMetadataKeyword(EMetadataValueArgument::Type InValueArgument)
		: ValueArgument(InValueArgument)
		{
		}

		void InsertAddAction(const TCHAR* InMapping, const TCHAR* InDefaultValue)
		{
			ValueActions.Add(FMetadataValueAction(InMapping, InDefaultValue, EMetadataValueAction::Add));
		}

		void InsertRemoveAction(const TCHAR* InMapping)
		{
			ValueActions.Add(FMetadataValueAction(InMapping, TEXT(""), EMetadataValueAction::Remove));
		}

		void ApplyToMetadata(TMap<FName, FString>& Metadata, const FString* Value = NULL)
		{
			for (auto It = ValueActions.CreateConstIterator(); It; ++It)
			{
				if (It->ValueAction == EMetadataValueAction::Add)
				{
					FBaseParser::InsertMetaDataPair(Metadata, It->Mapping, Value ? *Value : It->DefaultValue);
				}
				else
				{
					Metadata.Remove(*It->Mapping);
				}
			}
		}

		TArray<FMetadataValueAction> ValueActions;
		EMetadataValueArgument::Type ValueArgument;
	};

	FMetadataKeyword* GetMetadataKeyword(const TCHAR* Keyword)
	{
		static TMap<FString, FMetadataKeyword> Dictionary;
		if (!Dictionary.Num())
		{
			FMetadataKeyword& DisplayName = Dictionary.Add(TEXT("DisplayName"), EMetadataValueArgument::Required);
			DisplayName.InsertAddAction(TEXT("DisplayName"), TEXT(""));

			FMetadataKeyword& FriendlyName = Dictionary.Add(TEXT("FriendlyName"), EMetadataValueArgument::Required);
			FriendlyName.InsertAddAction(TEXT("FriendlyName"), TEXT(""));

			FMetadataKeyword& BlueprintInternalUseOnly = Dictionary.Add(TEXT("BlueprintInternalUseOnly"), EMetadataValueArgument::None);
			BlueprintInternalUseOnly.InsertAddAction(TEXT("BlueprintInternalUseOnly"), TEXT("true"));
			BlueprintInternalUseOnly.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));

			FMetadataKeyword& BlueprintType = Dictionary.Add(TEXT("BlueprintType"), EMetadataValueArgument::None);
			BlueprintType.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));

			FMetadataKeyword& NotBlueprintType = Dictionary.Add(TEXT("NotBlueprintType"), EMetadataValueArgument::None);
			NotBlueprintType.InsertAddAction(TEXT("NotBlueprintType"), TEXT("true"));
			NotBlueprintType.InsertRemoveAction(TEXT("BlueprintType"));

			FMetadataKeyword& Blueprintable = Dictionary.Add(TEXT("Blueprintable"), EMetadataValueArgument::None);
			Blueprintable.InsertAddAction(TEXT("IsBlueprintBase"), TEXT("true"));
			Blueprintable.InsertAddAction(TEXT("BlueprintType"),   TEXT("true"));

			FMetadataKeyword& CallInEditor = Dictionary.Add(TEXT("CallInEditor"), EMetadataValueArgument::None);
			CallInEditor.InsertAddAction(TEXT("CallInEditor"), TEXT("true"));

			FMetadataKeyword& NotBlueprintable = Dictionary.Add(TEXT("NotBlueprintable"), EMetadataValueArgument::None);
			NotBlueprintable.InsertAddAction   (TEXT("IsBlueprintBase"), TEXT("false"));
			NotBlueprintable.InsertRemoveAction(TEXT("BlueprintType"));

			FMetadataKeyword& Category = Dictionary.Add(TEXT("Category"), EMetadataValueArgument::Required);
			Category.InsertAddAction(TEXT("Category"), TEXT(""));

			FMetadataKeyword& ExperimentalFeature = Dictionary.Add(TEXT("Experimental"), EMetadataValueArgument::None);
			ExperimentalFeature.InsertAddAction(TEXT("DevelopmentStatus"), TEXT("Experimental"));

			FMetadataKeyword& EarlyAccessFeature = Dictionary.Add(TEXT("EarlyAccessPreview"), EMetadataValueArgument::None);
			EarlyAccessFeature.InsertAddAction(TEXT("DevelopmentStatus"), TEXT("EarlyAccess"));
		}

		return Dictionary.Find(Keyword);
	}
}

//////////////////////////////////////////////////////////////////////////
// FPropertySpecifier

FString FPropertySpecifier::ConvertToString() const
{
	FString Result;

	// Emit the specifier key
	Result += Key;

	// Emit the values if there are any
	if (Values.Num())
	{
		Result += TEXT("=");

		if (Values.Num() == 1)
		{
			// One value goes on it's own
			Result += Values[0];
		}
		else
		{
			// More than one value goes in parens, separated by commas
			Result += TEXT("(");
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				if (ValueIndex > 0)
				{
					Result += TEXT(", ");
				}
				Result += Values[ValueIndex];
			}
			Result += TEXT(")");
		}
	}

	return Result;
}

/////////////////////////////////////////////////////
// FBaseParser

FBaseParser::FBaseParser()
	: StatementsParsed(0)
	, LinesParsed(0)
{
}

void FBaseParser::ResetParser(const TCHAR* SourceBuffer, int32 StartingLineNumber)
{
	Input = SourceBuffer;
	InputLen = FCString::Strlen(Input);
	InputPos = 0;
	PrevPos = 0;
	PrevLine = 1;
	InputLine = StartingLineNumber;
}

/*-----------------------------------------------------------------------------
	Single-character processing.
-----------------------------------------------------------------------------*/

//
// Get a single character from the input stream and return it, or 0=end.
//
TCHAR FBaseParser::GetChar(bool bLiteral)
{
	bool bInsideComment = false;

	PrevPos = InputPos;
	PrevLine = InputLine;

Loop:
	const TCHAR c = Input[InputPos++];
	if (bInsideComment)
	{
		// Record the character as a comment.
		PrevComment += c;
	}

	if (c == TEXT('\n'))
	{
		InputLine++;
	}
	else if (!bLiteral)
	{
		const TCHAR NextChar = PeekChar();
		if ( c==TEXT('/') && NextChar==TEXT('*') )
		{
			if (!bInsideComment)
			{
				ClearComment();
				// Record the slash and star.
				PrevComment += c;
				PrevComment += NextChar;
				bInsideComment = true;

				// Move past the star. Do it only when not in comment,
				// otherwise end of comment might be missed e.g.
				// /*/ Comment /*/
				// ~~~~~~~~~~~~~^ Will report second /* as beginning of comment
				// And throw error that end of file is found in comment.
				InputPos++;
			}

			goto Loop;
		}
		else if( c==TEXT('*') && NextChar==TEXT('/') )
		{
			if (!bInsideComment)
			{
				ClearComment();
				FError::Throwf(TEXT("Unexpected '*/' outside of comment") );
			}

			/** Asterisk and slash always end comment. */
			bInsideComment = false;

			// Star already recorded; record the slash.
			PrevComment += Input[InputPos];

			InputPos++;
			goto Loop;
		}
	}

	if (bInsideComment)
	{
		if (c == 0)
		{
			ClearComment();
			FError::Throwf(TEXT("End of class header encountered inside comment") );
		}
		goto Loop;
	}
	return c;
}

//
// Unget the previous character retrieved with GetChar().
//
void FBaseParser::UngetChar()
{
	InputPos = PrevPos;
	InputLine = PrevLine;
}

//
// Look at a single character from the input stream and return it, or 0=end.
// Has no effect on the input stream.
//
TCHAR FBaseParser::PeekChar()
{
	return (InputPos < InputLen) ? Input[InputPos] : 0;
}

//
// Skip past all spaces and tabs in the input stream.
//
TCHAR FBaseParser::GetLeadingChar()
{
	TCHAR TrailingCommentNewline = 0;

	for (;;)
	{
		bool MultipleNewlines = false;

		TCHAR c;

		// Skip blanks.
		do
		{
			c = GetChar();

			// Check if we've encountered another newline since the last one
			if (c == TrailingCommentNewline)
			{
				MultipleNewlines = true;
			}
		} while (IsWhitespace(c));

		if (c != TEXT('/') || PeekChar() != TEXT('/'))
		{
			return c;
		}

		// Clear the comment if we've encountered newlines since the last comment
		if (MultipleNewlines)
		{
			ClearComment();
		}

		// Record the first slash.  The first iteration of the loop will get the second slash.
		PrevComment += c;

		do
		{
			c = GetChar(true);
			if (c == 0)
				return c;
			PrevComment += c;
		} while (!IsEOL(c));

		TrailingCommentNewline = c;

		for (;;)
		{
			c = GetChar();
			if (c == 0)
				return c;
			if (c == TrailingCommentNewline || !IsEOL(c))
			{
				UngetChar();
				break;
			}

			PrevComment += c;
		}
	}
}

bool FBaseParser::IsEOL( TCHAR c )
{
	return c==TEXT('\n') || c==TEXT('\r') || c==0;
}

bool FBaseParser::IsWhitespace( TCHAR c )
{
	return c==TEXT(' ') || c==TEXT('\t') || c==TEXT('\r') || c==TEXT('\n');
}

/*-----------------------------------------------------------------------------
	Tokenizing.
-----------------------------------------------------------------------------*/

// Gets the next token from the input stream, advancing the variables which keep track of the current input position and line.
bool FBaseParser::GetToken( FToken& Token, bool bNoConsts/*=false*/, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	Token.TokenName	= NAME_None;
	TCHAR c = GetLeadingChar();
	TCHAR p = PeekChar();
	if( c == 0 )
	{
		UngetChar();
		return 0;
	}
	Token.StartPos		= PrevPos;
	Token.StartLine		= PrevLine;
	if( (c>='A' && c<='Z') || (c>='a' && c<='z') || (c=='_') )
	{
		// Alphanumeric token.
		int32 Length=0;
		do
		{
			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
			{
				FError::Throwf(TEXT("Identifer length exceeds maximum of %i"), (int32)NAME_SIZE);
				Length = ((int32)NAME_SIZE) - 1;
				break;
			}
			c = GetChar();
		} while( ((c>='A')&&(c<='Z')) || ((c>='a')&&(c<='z')) || ((c>='0')&&(c<='9')) || (c=='_') );
		UngetChar();
		Token.Identifier[Length]=0;

		// Assume this is an identifier unless we find otherwise.
		Token.TokenType = TOKEN_Identifier;

		// Lookup the token's global name.
		Token.TokenName = FName(Token.Identifier, FNAME_Find);

		// If const values are allowed, determine whether the identifier represents a constant
		if ( !bNoConsts )
		{
			// See if the identifier is part of a vector, rotation or other struct constant.
			// boolean true/false
			if( Token.Matches(TEXT("true")) )
			{
				Token.SetConstBool(true);
				return true;
			}
			else if( Token.Matches(TEXT("false")) )
			{
				Token.SetConstBool(false);
				return true;
			}
		}
		return true;
	}

	// if const values are allowed, determine whether the non-identifier token represents a const
	else if ( !bNoConsts && ((c>='0' && c<='9') || ((c=='+' || c=='-') && (p>='0' && p<='9'))) )
	{
		// Integer or floating point constant.
		bool  bIsFloat = 0;
		int32 Length   = 0;
		bool  bIsHex   = 0;
		do
		{
			if( c==TEXT('.') )
			{
				bIsFloat = true;
			}
			if( c==TEXT('X') || c == TEXT('x') )
			{
				bIsHex = true;
			}

			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
			{
				FError::Throwf(TEXT("Number length exceeds maximum of %i "), (int32)NAME_SIZE );
				Length = ((int32)NAME_SIZE) - 1;
				break;
			}
			c = FChar::ToUpper(GetChar());
		} while ((c >= TEXT('0') && c <= TEXT('9')) || (!bIsFloat && c == TEXT('.')) || (!bIsHex && c == TEXT('X')) || (bIsHex && c >= TEXT('A') && c <= TEXT('F')));

		Token.Identifier[Length]=0;
		if (!bIsFloat || c != 'F')
		{
			UngetChar();
		}

		if (bIsFloat)
		{
			Token.SetConstFloat( FCString::Atof(Token.Identifier) );
		}
		else if (bIsHex)
		{
			TCHAR* End = Token.Identifier + FCString::Strlen(Token.Identifier);
			Token.SetConstInt64( FCString::Strtoi64(Token.Identifier,&End,0) );
		}
		else
		{
			Token.SetConstInt64( FCString::Atoi64(Token.Identifier) );
		}
		return true;
	}
	else if (c == '\'')
	{
		TCHAR ActualCharLiteral = GetChar(/*bLiteral=*/ true);

		if (ActualCharLiteral == '\\')
		{
			ActualCharLiteral = GetChar(/*bLiteral=*/ true);
			switch (ActualCharLiteral)
			{
			case TCHAR('t'):
				ActualCharLiteral = '\t';
				break;
			case TCHAR('n'):
				ActualCharLiteral = '\n';
				break;
			case TCHAR('r'):
				ActualCharLiteral = '\r';
				break;
			}
		}

		c = GetChar(/*bLiteral=*/ true);
		if (c != '\'')
		{
			FError::Throwf(TEXT("Unterminated character constant"));
			UngetChar();
		}

		Token.SetConstChar(ActualCharLiteral);
		return true;
	}
	else if (c == '"')
	{
		// String constant.
		TCHAR Temp[MAX_STRING_CONST_SIZE];
		int32 Length=0;
		c = GetChar(/*bLiteral=*/ true);
		while( (c!='"') && !IsEOL(c) )
		{
			if( c=='\\' )
			{
				c = GetChar(/*bLiteral=*/ true);
				if( IsEOL(c) )
				{
					break;
				}
				else if(c == 'n')
				{
					// Newline escape sequence.
					c = '\n';
				}
			}
			Temp[Length++] = c;
			if( Length >= MAX_STRING_CONST_SIZE )
			{
				FError::Throwf(TEXT("String constant exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE );
				c = TEXT('\"');
				Length = ((int32)MAX_STRING_CONST_SIZE) - 1;
				break;
			}
			c = GetChar(/*bLiteral=*/ true);
		}
		Temp[Length]=0;

		if( c != '"' )
		{
			FError::Throwf(TEXT("Unterminated string constant: %s"), Temp);
			UngetChar();
		}

		Token.SetConstString(Temp);
		return true;
	}
	else
	{
		// Symbol.
		int32 Length=0;
		Token.Identifier[Length++] = c;

		// Handle special 2-character symbols.
		#define PAIR(cc,dd) ((c==cc)&&(d==dd)) /* Comparison macro for convenience */
		TCHAR d = GetChar();
		if
		(	PAIR('<','<')
		||	(PAIR('>','>') && (bParseTemplateClosingBracket != ESymbolParseOption::CloseTemplateBracket))
		||	PAIR('!','=')
		||	PAIR('<','=')
		||	PAIR('>','=')
		||	PAIR('+','+')
		||	PAIR('-','-')
		||	PAIR('+','=')
		||	PAIR('-','=')
		||	PAIR('*','=')
		||	PAIR('/','=')
		||	PAIR('&','&')
		||	PAIR('|','|')
		||	PAIR('^','^')
		||	PAIR('=','=')
		||	PAIR('*','*')
		||	PAIR('~','=')
		||	PAIR(':',':')
		)
		{
			Token.Identifier[Length++] = d;
			if( c=='>' && d=='>' )
			{
				if( GetChar()=='>' )
					Token.Identifier[Length++] = '>';
				else
					UngetChar();
			}
		}
		else UngetChar();
		#undef PAIR

		Token.Identifier[Length] = 0;
		Token.TokenType = TOKEN_Symbol;

		// Lookup the token's global name.
		Token.TokenName = FName(Token.Identifier, FNAME_Find);

		return true;
	}
}

bool FBaseParser::GetRawTokenRespectingQuotes( FToken& Token, TCHAR StopChar /* = TCHAR('\n') */ )
{
	// Get token after whitespace.
	TCHAR Temp[MAX_STRING_CONST_SIZE];
	int32 Length=0;
	TCHAR c = GetLeadingChar();

	bool bInQuote = false;

	while( !IsEOL(c) && ((c != StopChar) || bInQuote) )
	{
		if( (c=='/' && PeekChar()=='/') || (c=='/' && PeekChar()=='*') )
		{
			break;
		}

		if (c == '"')
		{
			bInQuote = !bInQuote;
		}

		Temp[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE )
		{
			FError::Throwf(TEXT("Identifier exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE );
			c = GetChar(true);
			Length = ((int32)MAX_STRING_CONST_SIZE) - 1;
			break;
		}
		c = GetChar(true);
	}
	UngetChar();

	if (bInQuote)
	{
		FError::Throwf(TEXT("Unterminated quoted string"));
	}

	// Get rid of trailing whitespace.
	while( Length>0 && (Temp[Length-1]==' ' || Temp[Length-1]==9 ) )
	{
		Length--;
	}
	Temp[Length]=0;

	Token.SetConstString(Temp);

	return Length>0;
}

/**
 * Put all text from the current position up to either EOL or the StopToken
 * into Token.  Advances the compiler's current position.
 *
 * @param	Token	[out] will contain the text that was parsed
 * @param	StopChar	stop processing when this character is reached
 *
 * @return	the number of character parsed
 */
bool FBaseParser::GetRawToken( FToken& Token, TCHAR StopChar /* = TCHAR('\n') */ )
{
	// Get token after whitespace.
	TCHAR Temp[MAX_STRING_CONST_SIZE];
	int32 Length=0;
	TCHAR c = GetLeadingChar();
	while( !IsEOL(c) && c != StopChar )
	{
		if( (c=='/' && PeekChar()=='/') || (c=='/' && PeekChar()=='*') )
		{
			break;
		}
		Temp[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE )
		{
			FError::Throwf(TEXT("Identifier exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE );
		}
		c = GetChar(true);
	}
	UngetChar();

	// Get rid of trailing whitespace.
	while( Length>0 && (Temp[Length-1]==' ' || Temp[Length-1]==9 ) )
	{
		Length--;
	}
	Temp[Length]=0;

	Token.SetConstString(Temp);

	return Length>0;
}

//
// Get an identifier token, return 1 if gotten, 0 if not.
//
bool FBaseParser::GetIdentifier( FToken& Token, bool bNoConsts )
{
	if (!GetToken(Token, bNoConsts))
	{
		return false;
	}

	if (Token.TokenType == TOKEN_Identifier)
	{
		return true;
	}

	UngetToken(Token);
	return false;
}

//
// Get a symbol token, return 1 if gotten, 0 if not.
//
bool FBaseParser::GetSymbol( FToken& Token )
{
	if (!GetToken(Token))
	{
		return false;
	}

	if( Token.TokenType == TOKEN_Symbol )
	{
		return true;
	}

	UngetToken(Token);
	return false;
}

bool FBaseParser::GetConstInt(int32& Result, const TCHAR* Tag)
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.GetConstInt(Result))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	if (Tag != NULL)
	{
		FError::Throwf(TEXT("%s: Missing constant integer"), Tag );
	}

	return false;
}

bool FBaseParser::GetConstInt64(int64& Result, const TCHAR* Tag)
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.GetConstInt64(Result))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	if (Tag != NULL)
	{
		FError::Throwf(TEXT("%s: Missing constant integer"), Tag );
	}

	return false;
}

bool FBaseParser::MatchSymbol( const TCHAR* Match, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	FToken Token;

	if (GetToken(Token, /*bNoConsts=*/ true, bParseTemplateClosingBracket))
	{
		if (Token.TokenType==TOKEN_Symbol && !FCString::Stricmp(Token.Identifier, Match))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	return false;
}

//
// Get a specific identifier and return 1 if gotten, 0 if not.
// This is used primarily for checking for required symbols during compilation.
//
bool FBaseParser::MatchIdentifier( FName Match )
{
	FToken Token;
	if (!GetToken(Token))
	{
		return false;
	}

	if ((Token.TokenType == TOKEN_Identifier) && (Token.TokenName == Match))
	{
		return true;
	}

	UngetToken(Token);
	return false;
}

bool FBaseParser::MatchIdentifier( const TCHAR* Match )
{
	FToken Token;
	if (GetToken(Token))
	{
		if( Token.TokenType==TOKEN_Identifier && FCString::Stricmp(Token.Identifier,Match)==0 )
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}
	
	return false;
}

bool FBaseParser::MatchConstInt( const TCHAR* Match )
{
	FToken Token;
	if (GetToken(Token))
	{
		if( Token.TokenType==TOKEN_Const && (Token.Type == CPT_Int || Token.Type == CPT_Int64) && FCString::Stricmp(Token.Identifier,Match)==0 )
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}
	
	return false;
}


void FBaseParser::MatchSemi()
{
	if( !MatchSymbol(TEXT(";")) )
	{
		FToken Token;
		if( GetToken(Token) )
		{
			FError::Throwf(TEXT("Missing ';' before '%s'"), Token.Identifier );
		}
		else
		{
			FError::Throwf(TEXT("Missing ';'") );
		}
	}
}


//
// Peek ahead and see if a symbol follows in the stream.
//
bool FBaseParser::PeekSymbol( const TCHAR* Match )
{
	FToken Token;
	if (!GetToken(Token, true))
	{
		return false;
	}
	UngetToken(Token);

	return Token.TokenType==TOKEN_Symbol && FCString::Stricmp(Token.Identifier,Match)==0;
}

//
// Peek ahead and see if an identifier follows in the stream.
//
bool FBaseParser::PeekIdentifier( FName Match )
{
	FToken Token;
	if (!GetToken(Token, true))
	{
		return false;
	}
	UngetToken(Token);
	return Token.TokenType==TOKEN_Identifier && Token.TokenName==Match;
}

bool FBaseParser::PeekIdentifier( const TCHAR* Match )
{
	FToken Token;
	if (!GetToken(Token, true))
	{
		return false;
	}
	UngetToken(Token);
	return Token.TokenType==TOKEN_Identifier && FCString::Stricmp(Token.Identifier,Match)==0;
}

//
// Unget the most recently gotten token.
//
void FBaseParser::UngetToken( const FToken& Token )
{
	InputPos = Token.StartPos;
	InputLine = Token.StartLine;
}

//
// Require a symbol.
//
void FBaseParser::RequireSymbol( const TCHAR* Match, const TCHAR* Tag, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	if (!MatchSymbol(Match, bParseTemplateClosingBracket))
	{
		FError::Throwf(TEXT("Missing '%s' in %s"), Match, Tag );
	}
}

//
// Require an identifier.
//
void FBaseParser::RequireIdentifier( FName Match, const TCHAR* Tag )
{
	if (!MatchIdentifier(Match))
	{
		FError::Throwf(TEXT("Missing '%s' in %s"), *Match.ToString(), Tag );
	}
}

void FBaseParser::RequireIdentifier( const TCHAR* Match, const TCHAR* Tag )
{
	if (!MatchIdentifier(Match))
	{
		FError::Throwf(TEXT("Missing '%s' in %s"), Match, Tag );
	}
}

// Clears out the stored comment.
void FBaseParser::ClearComment()
{
	// Can't call Reset as FString uses protected inheritance
	PrevComment.Empty( PrevComment.Len() );
}

// Reads a new-style value
//@TODO: UCREMOVAL: Needs a better name
FString FBaseParser::ReadNewStyleValue(const FString& TypeOfSpecifier)
{
	FToken ValueToken;
	if (!GetToken( ValueToken, false ))
		FError::Throwf(TEXT("Expected a value when handling a "), *TypeOfSpecifier);

	switch (ValueToken.TokenType)
	{
		case TOKEN_Identifier:
		case TOKEN_Symbol:
		{
			FString Result = ValueToken.Identifier;

			if (MatchSymbol(TEXT("=")))
			{
				Result += TEXT("=");
				Result += ReadNewStyleValue(TypeOfSpecifier);
			}

			return Result;
		}

		case TOKEN_Const:
			return ValueToken.GetConstantValue();

		default:
			return TEXT("");
	}
}

// Reads [ Value | ['(' Value [',' Value]* ')'] ] and places each value into the Items array
bool FBaseParser::ReadOptionalCommaSeparatedListInParens(TArray<FString>& Items, const FString& TypeOfSpecifier)
{
	if (MatchSymbol(TEXT("(")))
	{
		do 
		{
			FString Value = ReadNewStyleValue(TypeOfSpecifier);
			Items.Add(Value);
		} while ( MatchSymbol(TEXT(",")) );

		RequireSymbol(TEXT(")"), *TypeOfSpecifier);

		return true;
	}

	return false;
}

void FBaseParser::ParseNameWithPotentialAPIMacroPrefix(FString& DeclaredName, FString& RequiredAPIMacroIfPresent, const TCHAR* FailureMessage)
{
	// Expecting Name | (MODULE_API Name)
	FToken NameToken;

	// Read an identifier
	if (!GetIdentifier(NameToken))
	{
		FError::Throwf(TEXT("Missing %s name"), FailureMessage);
	}

	// Is the identifier the name or an DLL import/export API macro?
	FString NameTokenStr = NameToken.Identifier;
	if (NameTokenStr.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		RequiredAPIMacroIfPresent = NameTokenStr;

		// Read the real name
		if (!GetIdentifier(NameToken))
		{
			FError::Throwf(TEXT("Missing %s name"), FailureMessage);
		}
		DeclaredName = NameToken.Identifier;
	}
	else
	{
		DeclaredName = NameTokenStr;
		RequiredAPIMacroIfPresent.Empty();
	}
}

// Reads a set of specifiers (with optional values) inside the () of a new-style metadata macro like UPROPERTY or UFUNCTION
void FBaseParser::ReadSpecifierSetInsideMacro(TArray<FPropertySpecifier>& SpecifiersFound, const FString& TypeOfSpecifier, TMap<FName, FString>& MetaData)
{
	int32 FoundSpecifierCount = 0;
	FString ErrorMessage = FString::Printf(TEXT("%s declaration specifier"), *TypeOfSpecifier);

	RequireSymbol(TEXT("("), *ErrorMessage);

	while (!MatchSymbol(TEXT(")")))
	{
		if (FoundSpecifierCount > 0)
		{
			RequireSymbol(TEXT(","), *ErrorMessage);
		}
		++FoundSpecifierCount;

		// Read the specifier key
		FToken Specifier;
		if (!GetToken(Specifier))
		{
			FError::Throwf(TEXT("Expected %s"), *ErrorMessage);
		}

		if (Specifier.Matches(TEXT("meta")))
		{
			RequireSymbol(TEXT("="), *ErrorMessage);
			RequireSymbol(TEXT("("), *ErrorMessage);

			// Keep reading comma-separated metadata pairs
			do 
			{
				// Read a key
				FToken MetaKeyToken;
				if (!GetIdentifier(MetaKeyToken))
				{
					FError::Throwf(TEXT("Expected a metadata key"));
				}

				FString Key = MetaKeyToken.Identifier;

				// Potentially read a value
				FString Value;
				if (MatchSymbol(TEXT("=")))
				{
					Value = ReadNewStyleValue(TypeOfSpecifier);
				}

				// Validate the value is a valid type for the key and insert it into the map
				InsertMetaDataPair(MetaData, Key, Value);
			} while ( MatchSymbol(TEXT(",")) );

			RequireSymbol(TEXT(")"), *ErrorMessage);
		}
		// Look up specifier in metadata dictionary
		else if (FMetadataKeyword* MetadataKeyword = GetMetadataKeyword(Specifier.Identifier))
		{
			if (MatchSymbol(TEXT("=")))
			{
				if (MetadataKeyword->ValueArgument == EMetadataValueArgument::None)
				{
					FError::Throwf(TEXT("Incorrect = after metadata specifier '%s'"), Specifier.Identifier);
				}

				FString Value = ReadNewStyleValue(TypeOfSpecifier);
				MetadataKeyword->ApplyToMetadata(MetaData, &Value);
			}
			else
			{
				if (MetadataKeyword->ValueArgument == EMetadataValueArgument::Required)
				{
					FError::Throwf(TEXT("Missing = after metadata specifier '%s'"), Specifier.Identifier);
				}

				MetadataKeyword->ApplyToMetadata(MetaData);
			}
		}
		else
		{
			// Creating a new specifier
			SpecifiersFound.Emplace(Specifier.Identifier);

			// Look for a value for this specifier
			if (MatchSymbol(TEXT("=")) || PeekSymbol(TEXT("(")))
			{
				TArray<FString>& NewPairValues = SpecifiersFound.Last().Values;
				if (!ReadOptionalCommaSeparatedListInParens(NewPairValues, TypeOfSpecifier))
				{
					FString Value = ReadNewStyleValue(TypeOfSpecifier);
					NewPairValues.Add(Value);
				}
			}
		}
	}
}

void FBaseParser::InsertMetaDataPair(TMap<FName, FString>& MetaData, const FString& InKey, const FString& InValue)
{
	FString Key = InKey;
	FString Value = InValue;

	// trim extra white space and quotes
	Key.TrimStartAndEndInline();
	Value.TrimStartAndEndInline();
	Value = Value.TrimQuotes();

	// make sure the key is valid
	if (InKey.Len() == 0)
	{
		FError::Throwf(TEXT("Invalid metadata"));
	}

	FName KeyName(*Key);

	FString* ExistingValue = MetaData.Find(KeyName);
	if (ExistingValue && Value != *ExistingValue)
	{
		FError::Throwf(TEXT("Metadata key '%s' first seen with value '%s' then '%s'"), *Key, **ExistingValue, *Value);
	}

	// finally we have enough to put it into our metadata
	MetaData.Add(FName(*Key), *Value);
}


