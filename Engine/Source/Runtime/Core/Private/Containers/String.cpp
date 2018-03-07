// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/VarArgs.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/ByteSwap.h"


/* FString implementation
 *****************************************************************************/

namespace UE4String_Private
{
	struct FCompareCharsCaseSensitive
	{
		static FORCEINLINE bool Compare(TCHAR Lhs, TCHAR Rhs)
		{
			return Lhs == Rhs;
		}
	};

	struct FCompareCharsCaseInsensitive
	{
		static FORCEINLINE bool Compare(TCHAR Lhs, TCHAR Rhs)
		{
			return FChar::ToLower(Lhs) == FChar::ToLower(Rhs);
		}
	};

	template <typename CompareType>
	bool MatchesWildcardRecursive(const TCHAR* Target, int32 TargetLength, const TCHAR* Wildcard, int32 WildcardLength)
	{
		// Skip over common initial non-wildcard-char sequence of Target and Wildcard
		for (;;)
		{
			TCHAR WCh = *Wildcard;
			if (WCh == TEXT('*') || WCh == TEXT('?'))
			{
				break;
			}

			if (!CompareType::Compare(*Target, WCh))
			{
				return false;
			}

			if (WCh == TEXT('\0'))
			{
				return true;
			}

			++Target;
			++Wildcard;
			--TargetLength;
			--WildcardLength;
		}

		// Test for common suffix
		const TCHAR* TPtr = Target   + TargetLength;
		const TCHAR* WPtr = Wildcard + WildcardLength;
		for (;;)
		{
			--TPtr;
			--WPtr;

			TCHAR WCh = *WPtr;
			if (WCh == TEXT('*') || WCh == TEXT('?'))
			{
				break;
			}

			if (!CompareType::Compare(*TPtr, WCh))
			{
				return false;
			}

			--TargetLength;
			if (TargetLength == 0)
			{
				return false;
			}

			--WildcardLength;
		}

		// Match * against anything and ? against single (and zero?) chars
		TCHAR FirstWild = *Wildcard;
		if (WildcardLength == 1 && (FirstWild == TEXT('*') || TargetLength < 2))
		{
			return true;
		}
		++Wildcard;
		--WildcardLength;

		// This routine is very slow, though it does ok with one wildcard
		int32 MaxNum = TargetLength;
		if (FirstWild == TEXT('?') && MaxNum > 1)
		{
			MaxNum = 1;
		}

		for (int32 Index = 0; Index <= MaxNum; ++Index)
		{
			if (MatchesWildcardRecursive<CompareType>(Target, TargetLength - Index, Wildcard, WildcardLength))
			{
				return true;
			}
		}
		return false;
	}
}

void FString::TrimToNullTerminator()
{
	if( Data.Num() )
	{
		int32 DataLen = FCString::Strlen(Data.GetData());
		check(DataLen == 0 || DataLen < Data.Num());
		int32 Len = DataLen > 0 ? DataLen+1 : 0;

		check(Len <= Data.Num());
		Data.RemoveAt(Len, Data.Num()-Len);
	}
}


int32 FString::Find(const TCHAR* SubStr, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir, int32 StartPosition) const
{
	if ( SubStr == nullptr )
	{
		return INDEX_NONE;
	}
	if( SearchDir == ESearchDir::FromStart)
	{
		const TCHAR* Start = **this;
		if ( StartPosition != INDEX_NONE )
		{
			Start += FMath::Clamp(StartPosition, 0, Len() - 1);
		}
		const TCHAR* Tmp = SearchCase == ESearchCase::IgnoreCase
			? FCString::Stristr(Start, SubStr)
			: FCString::Strstr(Start, SubStr);

		return Tmp ? (Tmp-**this) : INDEX_NONE;
	}
	else
	{
		// if ignoring, do a onetime ToUpper on both strings, to avoid ToUppering multiple
		// times in the loop below
		if ( SearchCase == ESearchCase::IgnoreCase)
		{
			return ToUpper().Find(FString(SubStr).ToUpper(), ESearchCase::CaseSensitive, SearchDir, StartPosition);
		}
		else
		{
			const int32 SearchStringLength=FMath::Max(1, FCString::Strlen(SubStr));
			
			if ( StartPosition == INDEX_NONE || StartPosition >= Len() )
			{
				StartPosition = Len();
			}
			
			for( int32 i = StartPosition - SearchStringLength; i >= 0; i-- )
			{
				int32 j;
				for( j=0; SubStr[j]; j++ )
				{
					if( (*this)[i+j]!=SubStr[j] )
					{
						break;
					}
				}
				
				if( !SubStr[j] )
				{
					return i;
				}
			}
			return INDEX_NONE;
		}
	}
}

FString FString::ToUpper() const &
{
	FString New = *this;
	New.ToUpperInline();
	return New;
}

FString FString::ToUpper() &&
{
	this->ToUpperInline();
	return MoveTemp(*this);
}

void FString::ToUpperInline()
{
	const int32 StringLength = Len();
	TCHAR* RawData = Data.GetData();
	for (int32 i = 0; i < StringLength; ++i)
	{
		RawData[i] = FChar::ToUpper(RawData[i]);
	}
}


FString FString::ToLower() const &
{
	FString New = *this;
	New.ToLowerInline();
	return New;
}

FString FString::ToLower() &&
{
	this->ToLowerInline();
	return MoveTemp(*this);
}

void FString::ToLowerInline()
{
	const int32 StringLength = Len();
	TCHAR* RawData = Data.GetData();
	for (int32 i = 0; i < StringLength; ++i)
	{
		RawData[i] = FChar::ToLower(RawData[i]);
	}
}

bool FString::StartsWith(const TCHAR* InPrefix, ESearchCase::Type SearchCase) const
{
	if (SearchCase == ESearchCase::IgnoreCase)
	{
		return InPrefix && *InPrefix && !FCString::Strnicmp(**this, InPrefix, FCString::Strlen(InPrefix));
	}
	else
	{
		return InPrefix && *InPrefix && !FCString::Strncmp(**this, InPrefix, FCString::Strlen(InPrefix));
	}
}

bool FString::StartsWith(const FString& InPrefix, ESearchCase::Type SearchCase ) const
{
	if( SearchCase == ESearchCase::IgnoreCase )
	{
		return InPrefix.Len() > 0 && !FCString::Strnicmp(**this, *InPrefix, InPrefix.Len());
	}
	else
	{
		return InPrefix.Len() > 0 && !FCString::Strncmp(**this, *InPrefix, InPrefix.Len());
	}
}

bool FString::EndsWith(const TCHAR* InSuffix, ESearchCase::Type SearchCase) const
{
	if (!InSuffix || *InSuffix == TEXT('\0'))
	{
		return false;
	}

	int32 ThisLen   = this->Len();
	int32 SuffixLen = FCString::Strlen(InSuffix);
	if (SuffixLen > ThisLen)
	{
		return false;
	}

	const TCHAR* StrPtr = Data.GetData() + ThisLen - SuffixLen;
	if (SearchCase == ESearchCase::IgnoreCase)
	{
		return !FCString::Stricmp(StrPtr, InSuffix);
	}
	else
	{
		return !FCString::Strcmp(StrPtr, InSuffix);
	}
}

bool FString::EndsWith(const FString& InSuffix, ESearchCase::Type SearchCase ) const
{
	if( SearchCase == ESearchCase::IgnoreCase )
	{
		return InSuffix.Len() > 0 &&
			Len() >= InSuffix.Len() &&
			!FCString::Stricmp( &(*this)[ Len() - InSuffix.Len() ], *InSuffix );
	}
	else
	{
		return InSuffix.Len() > 0 &&
			Len() >= InSuffix.Len() &&
			!FCString::Strcmp( &(*this)[ Len() - InSuffix.Len() ], *InSuffix );
	}
}

bool FString::RemoveFromStart( const FString& InPrefix, ESearchCase::Type SearchCase )
{
	if ( InPrefix.IsEmpty() )
	{
		return false;
	}

	if ( StartsWith( InPrefix, SearchCase ) )
	{
		RemoveAt( 0, InPrefix.Len() );
		return true;
	}

	return false;
}

bool FString::RemoveFromEnd( const FString& InSuffix, ESearchCase::Type SearchCase )
{
	if ( InSuffix.IsEmpty() )
	{
		return false;
	}

	if ( EndsWith( InSuffix, SearchCase ) )
	{
		RemoveAt( Len() - InSuffix.Len(), InSuffix.Len() );
		return true;
	}

	return false;
}

/**
 * Concatenate this path with given path ensuring the / character is used between them
 *
 * @param Str       Pointer to an array of TCHARs (not necessarily null-terminated) to be concatenated onto the end of this.
 * @param StrLength Exact number of characters from Str to append.
 */
void FString::PathAppend(const TCHAR* Str, int32 StrLength)
{
	int32 DataNum = Data.Num();
	if (StrLength == 0)
	{
		if (DataNum > 1 && Data[DataNum - 2] != TEXT('/') && Data[DataNum - 2] != TEXT('\\'))
		{
			Data[DataNum - 1] = TEXT('/');
			Data.Add(TEXT('\0'));
		}
	}
	else
	{
		if (DataNum > 0)
		{
			if (DataNum > 1 && Data[DataNum - 2] != TEXT('/') && Data[DataNum - 2] != TEXT('\\') && *Str != TEXT('/'))
			{
				Data[DataNum - 1] = TEXT('/');
			}
			else
			{
				Data.Pop(false);
				--DataNum;
			}
		}

		Data.Reserve(DataNum + StrLength + 1);
		Data.Append(Str, StrLength);
		Data.Add(TEXT('\0'));
	}
}

FString FString::Trim()
{
	int32 Pos = 0;
	while(Pos < Len())
	{
		if( FChar::IsWhitespace( (*this)[Pos] ) )
		{
			Pos++;
		}
		else
		{
			break;
		}
	}

	*this = Right( Len()-Pos );

	return *this;
}

FString FString::TrimTrailing( void )
{
	int32 Pos = Len() - 1;
	while( Pos >= 0 )
	{
		if( !FChar::IsWhitespace( ( *this )[Pos] ) )
		{
			break;
		}

		Pos--;
	}

	*this = Left( Pos + 1 );

	return( *this );
}

void FString::TrimStartAndEndInline()
{
	TrimEndInline();
	TrimStartInline();
}

FString FString::TrimStartAndEnd() const &
{
	FString Result(*this);
	Result.TrimStartAndEndInline();
	return Result;
}

FString FString::TrimStartAndEnd() &&
{
	FString Result(MoveTemp(*this));
	Result.TrimStartAndEndInline();
	return Result;
}

void FString::TrimStartInline()
{
	int32 Pos = 0;
	while(Pos < Len() && FChar::IsWhitespace((*this)[Pos]))
	{
		Pos++;
	}
	RemoveAt(0, Pos);
}

FString FString::TrimStart() const &
{
	FString Result(*this);
	Result.TrimStartInline();
	return Result;
}

FString FString::TrimStart() &&
{
	FString Result(MoveTemp(*this));
	Result.TrimStartInline();
	return Result;
}

void FString::TrimEndInline()
{
	int32 End = Len();
	while(End > 0 && FChar::IsWhitespace((*this)[End - 1]))
	{
		End--;
	}
	RemoveAt(End, Len() - End);
}

FString FString::TrimEnd() const &
{
	FString Result(*this);
	Result.TrimEndInline();
	return Result;
}

FString FString::TrimEnd() &&
{
	FString Result(MoveTemp(*this));
	Result.TrimEndInline();
	return Result;
}

FString FString::TrimQuotes( bool* bQuotesRemoved ) const
{
	bool bQuotesWereRemoved=false;
	int32 Start = 0, Count = Len();
	if ( Count > 0 )
	{
		if ( (*this)[0] == TCHAR('"') )
		{
			Start++;
			Count--;
			bQuotesWereRemoved=true;
		}

		if ( Len() > 1 && (*this)[Len() - 1] == TCHAR('"') )
		{
			Count--;
			bQuotesWereRemoved=true;
		}
	}

	if ( bQuotesRemoved != nullptr )
	{
		*bQuotesRemoved = bQuotesWereRemoved;
	}
	return Mid(Start, Count);
}

int32 FString::CullArray( TArray<FString>* InArray )
{
	check(InArray);
	FString Empty;
	InArray->Remove(Empty);
	return InArray->Num();
}

FString FString::Reverse() const
{
	FString New(*this);
	New.ReverseString();
	return New;
}

void FString::ReverseString()
{
	if ( Len() > 0 )
	{
		TCHAR* StartChar = &(*this)[0];
		TCHAR* EndChar = &(*this)[Len()-1];
		TCHAR TempChar;
		do 
		{
			TempChar = *StartChar;	// store the current value of StartChar
			*StartChar = *EndChar;	// change the value of StartChar to the value of EndChar
			*EndChar = TempChar;	// change the value of EndChar to the character that was previously at StartChar

			StartChar++;
			EndChar--;

		} while( StartChar < EndChar );	// repeat until we've reached the midpoint of the string
	}
}

FString FString::FormatAsNumber( int32 InNumber )
{
	FString Number = FString::FromInt( InNumber ), Result;

	int32 dec = 0;
	for( int32 x = Number.Len()-1 ; x > -1 ; --x )
	{
		Result += Number.Mid(x,1);

		dec++;
		if( dec == 3 && x > 0 )
		{
			Result += TEXT(",");
			dec = 0;
		}
	}

	return Result.Reverse();
}

/**
 * Serializes a string as ANSI char array.
 *
 * @param	String			String to serialize
 * @param	Ar				Archive to serialize with
 * @param	MinCharacters	Minimum number of characters to serialize.
 */
void FString::SerializeAsANSICharArray( FArchive& Ar, int32 MinCharacters ) const
{
	int32	Length = FMath::Max( Len(), MinCharacters );
	Ar << Length;
	
	for( int32 CharIndex=0; CharIndex<Len(); CharIndex++ )
	{
		ANSICHAR AnsiChar = CharCast<ANSICHAR>( (*this)[CharIndex] );
		Ar << AnsiChar;
	}

	// Zero pad till minimum number of characters are written.
	for( int32 i=Len(); i<Length; i++ )
	{
		ANSICHAR NullChar = 0;
		Ar << NullChar;
	}
}

void FString::AppendInt( int32 InNum )
{
	int64 Num						= InNum; // This avoids having to deal with negating -MAX_int32-1
	const TCHAR* NumberChar[11]		= { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("-") };
	bool bIsNumberNegative			= false;
	TCHAR TempNum[16];				// 16 is big enough
	int32 TempAt					= 16; // fill the temp string from the top down.

	// Correctly handle negative numbers and convert to positive integer.
	if( Num < 0 )
	{
		bIsNumberNegative = true;
		Num = -Num;
	}

	TempNum[--TempAt] = 0; // NULL terminator

	// Convert to string assuming base ten and a positive integer.
	do 
	{
		TempNum[--TempAt] = *NumberChar[Num % 10];
		Num /= 10;
	} while( Num );

	// Append sign as we're going to reverse string afterwards.
	if( bIsNumberNegative )
	{
		TempNum[--TempAt] = *NumberChar[10];
	}

	*this += TempNum + TempAt;
}


bool FString::ToBool() const
{
	return FCString::ToBool(**this);
}

FString FString::FromBlob(const uint8* SrcBuffer,const uint32 SrcSize)
{
	FString Result;
	Result.Reserve( SrcSize * 3 );
	// Convert and append each byte in the buffer
	for (uint32 Count = 0; Count < SrcSize; Count++)
	{
		Result += FString::Printf(TEXT("%03d"),(uint8)SrcBuffer[Count]);
	}
	return Result;
}

bool FString::ToBlob(const FString& Source,uint8* DestBuffer,const uint32 DestSize)
{
	// Make sure the buffer is at least half the size and that the string is an
	// even number of characters long
	if (DestSize >= (uint32)(Source.Len() / 3) &&
		(Source.Len() % 3) == 0)
	{
		TCHAR ConvBuffer[4];
		ConvBuffer[3] = TEXT('\0');
		int32 WriteIndex = 0;
		// Walk the string 3 chars at a time
		for (int32 Index = 0; Index < Source.Len(); Index += 3, WriteIndex++)
		{
			ConvBuffer[0] = Source[Index];
			ConvBuffer[1] = Source[Index + 1];
			ConvBuffer[2] = Source[Index + 2];
			DestBuffer[WriteIndex] = FCString::Atoi(ConvBuffer);
		}
		return true;
	}
	return false;
}

FString FString::FromHexBlob( const uint8* SrcBuffer, const uint32 SrcSize )
{
	FString Result;
	Result.Reserve( SrcSize * 2 );
	// Convert and append each byte in the buffer
	for (uint32 Count = 0; Count < SrcSize; Count++)
	{
		Result += FString::Printf( TEXT( "%02X" ), (uint8)SrcBuffer[Count] );
	}
	return Result;
}

bool FString::ToHexBlob( const FString& Source, uint8* DestBuffer, const uint32 DestSize )
{
	// Make sure the buffer is at least half the size and that the string is an
	// even number of characters long
	if (DestSize >= (uint32)(Source.Len() / 2) &&
		 (Source.Len() % 2) == 0)
	{
		TCHAR ConvBuffer[3];
		ConvBuffer[2] = TEXT( '\0' );
		int32 WriteIndex = 0;
		// Walk the string 2 chars at a time
		TCHAR* End = nullptr;
		for (int32 Index = 0; Index < Source.Len(); Index += 2, WriteIndex++)
		{
			ConvBuffer[0] = Source[Index];
			ConvBuffer[1] = Source[Index + 1];
			DestBuffer[WriteIndex] = FCString::Strtoi( ConvBuffer, &End, 16 );
		}
		return true;
	}
	return false;
}

FString FString::SanitizeFloat( double InFloat )
{
	// Avoids negative zero
	if( InFloat == 0 )
	{
		InFloat = 0;
	}

	FString TempString;
	// First create the string
	TempString = FString::Printf(TEXT("%f"), InFloat );
	const TArray< TCHAR >& Chars = TempString.GetCharArray();	
	const TCHAR Zero = '0';
	const TCHAR Period = '.';
	int32 TrimIndex = 0;
	// Find the first non-zero char in the array
	for (int32 Index = Chars.Num()-2; Index >= 2; --Index )
	{
		const TCHAR EachChar = Chars[Index];
		const TCHAR NextChar = Chars[Index-1];
		if( ( EachChar != Zero ) || (NextChar == Period ) )
		{			
			TrimIndex = Index;
			break;
		}
	}	
	// If we changed something trim the string
	if( TrimIndex != 0 )
	{
		TempString = TempString.Left( TrimIndex + 1 );
	}
	return TempString;
}

FString FString::Chr( TCHAR Ch )
{
	TCHAR Temp[2]={Ch,0};
	return FString(Temp);
}


FString FString::ChrN( int32 NumCharacters, TCHAR Char )
{
	check( NumCharacters >= 0 );

	FString Temp;
	Temp.Data.AddUninitialized(NumCharacters+1);
	for( int32 Cx = 0; Cx < NumCharacters; ++Cx )
	{
		Temp[Cx] = Char;
	}
	Temp.Data[NumCharacters]=0;
	return Temp;
}

FString FString::LeftPad( int32 ChCount ) const
{
	int32 Pad = ChCount - Len();

	if (Pad > 0)
	{
		return ChrN(Pad, ' ') + *this;
	}
	else
	{
		return *this;
	}
}
FString FString::RightPad( int32 ChCount ) const
{
	int32 Pad = ChCount - Len();

	if (Pad > 0)
	{
		return *this + ChrN(Pad, ' ');
	}
	else
	{
		return *this;
	}
}

bool FString::IsNumeric() const
{
	if (IsEmpty())
	{
		return 0;
	}

	return FCString::IsNumeric(Data.GetData());
}

/**
 * Breaks up a delimited string into elements of a string array.
 *
 * @param	InArray		The array to fill with the string pieces
 * @param	pchDelim	The string to delimit on
 * @param	InCullEmpty	If 1, empty strings are not added to the array
 *
 * @return	The number of elements in InArray
 */
int32 FString::ParseIntoArray( TArray<FString>& OutArray, const TCHAR* pchDelim, const bool InCullEmpty ) const
{
	// Make sure the delimit string is not null or empty
	check(pchDelim);
	OutArray.Reset();
	const TCHAR *Start = Data.GetData();
	const int32 DelimLength = FCString::Strlen(pchDelim);
	if (Start && DelimLength)
	{
		while( const TCHAR *At = FCString::Strstr(Start,pchDelim) )
		{
			if (!InCullEmpty || At-Start)
			{
				OutArray.Emplace(At-Start,Start);
			}
			Start = At + DelimLength;
		}
		if (!InCullEmpty || *Start)
		{
			OutArray.Emplace(Start);
		}

	}
	return OutArray.Num();
}

bool FString::MatchesWildcard(const FString& InWildcard, ESearchCase::Type SearchCase) const
{
	FString Wildcard(InWildcard);
	FString Target(*this);
	int32 IndexOfStar = Wildcard.Find(TEXT("*"), ESearchCase::CaseSensitive, ESearchDir::FromEnd); // last occurance
	int32 IndexOfQuestion = Wildcard.Find( TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromEnd); // last occurance
	int32 Suffix = FMath::Max<int32>(IndexOfStar, IndexOfQuestion);
	if (Suffix == INDEX_NONE)
	{
		// no wildcards
		if (SearchCase == ESearchCase::IgnoreCase)
		{
			return FCString::Stricmp( *Target, *Wildcard )==0;
		}
		else
		{
			return FCString::Strcmp( *Target, *Wildcard )==0;
		}
	}
	else
	{
		if (Suffix + 1 < Wildcard.Len())
		{
			FString SuffixString = Wildcard.Mid(Suffix + 1);
			if (!Target.EndsWith(SuffixString, SearchCase))
			{
				return false;
			}
			Wildcard = Wildcard.Left(Suffix + 1);
			Target = Target.Left(Target.Len() - SuffixString.Len());
		}
		int32 PrefixIndexOfStar = Wildcard.Find(TEXT("*"), ESearchCase::CaseSensitive); 
		int32 PrefixIndexOfQuestion = Wildcard.Find(TEXT("?"), ESearchCase::CaseSensitive);
		int32 Prefix = FMath::Min<int32>(PrefixIndexOfStar < 0 ? MAX_int32 : PrefixIndexOfStar, PrefixIndexOfQuestion < 0 ? MAX_int32 : PrefixIndexOfQuestion);
		check(Prefix >= 0 && Prefix < Wildcard.Len());
		if (Prefix > 0)
		{
			FString PrefixString = Wildcard.Left(Prefix);
			if (!Target.StartsWith(PrefixString, SearchCase))
			{
				return false;
			}
			Wildcard = Wildcard.Mid(Prefix);
			Target = Target.Mid(Prefix);
		}
	}
	// This routine is very slow, though it does ok with one wildcard
	check(Wildcard.Len());
	TCHAR FirstWild = Wildcard[0];
	Wildcard = Wildcard.Right(Wildcard.Len() - 1);
	if (FirstWild == TEXT('*') || FirstWild == TEXT('?'))
	{
		if (!Wildcard.Len())
		{
			if (FirstWild == TEXT('*') || Target.Len() < 2)
			{
				return true;
			}
		}
		int32 MaxNum = FMath::Min<int32>(Target.Len(), FirstWild == TEXT('?') ? 1 : MAX_int32);
		for (int32 Index = 0; Index <= MaxNum; Index++)
		{
			if (Target.Right(Target.Len() - Index).MatchesWildcard(Wildcard, SearchCase))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		check(0); // we should have dealt with prefix comparison above
		return false;
	}
}


/** Caution!! this routine is O(N^2) allocations...use it for parsing very short text or not at all */
int32 FString::ParseIntoArrayWS( TArray<FString>& OutArray, const TCHAR* pchExtraDelim, bool InCullEmpty ) const
{
	// default array of White Spaces, the last entry can be replaced with the optional pchExtraDelim string
	// (if you want to split on white space and another character)
	static const TCHAR* WhiteSpace[] = 
	{
		TEXT(" "),
		TEXT("\t"),
		TEXT("\r"),
		TEXT("\n"),
		TEXT(""),
	};

	// start with just the standard whitespaces
	int32 NumWhiteSpaces = ARRAY_COUNT(WhiteSpace) - 1;
	// if we got one passed in, use that in addition
	if (pchExtraDelim && *pchExtraDelim)
	{
		WhiteSpace[NumWhiteSpaces++] = pchExtraDelim;
	}

	return ParseIntoArray(OutArray, WhiteSpace, NumWhiteSpaces, InCullEmpty);
}

int32 FString::ParseIntoArrayLines(TArray<FString>& OutArray, bool InCullEmpty) const
{
	// default array of LineEndings
	static const TCHAR* LineEndings[] =
	{				
		TEXT("\r\n"),
		TEXT("\r"),
		TEXT("\n"),	
	};

	// start with just the standard line endings
	int32 NumLineEndings = ARRAY_COUNT(LineEndings);	
	return ParseIntoArray(OutArray, LineEndings, NumLineEndings, InCullEmpty);
}

int32 FString::ParseIntoArray(TArray<FString>& OutArray, const TCHAR** DelimArray, int32 NumDelims, bool InCullEmpty) const
{
	// Make sure the delimit string is not null or empty
	check(DelimArray);
	OutArray.Empty();
	const TCHAR *Start = Data.GetData();
	const int32 Length = Len();
	if (Start)
	{
		int32 SubstringBeginIndex = 0;

		// Iterate through string.
		for(int32 i = 0; i < Len();)
		{
			int32 SubstringEndIndex = INDEX_NONE;
			int32 DelimiterLength = 0;

			// Attempt each delimiter.
			for(int32 DelimIndex = 0; DelimIndex < NumDelims; ++DelimIndex)
			{
				DelimiterLength = FCString::Strlen(DelimArray[DelimIndex]);

				// If we found a delimiter...
				if (FCString::Strncmp(Start + i, DelimArray[DelimIndex], DelimiterLength) == 0)
				{
					// Mark the end of the substring.
					SubstringEndIndex = i;
					break;
				}
			}

			if (SubstringEndIndex != INDEX_NONE)
			{
				const int32 SubstringLength = SubstringEndIndex - SubstringBeginIndex;
				// If we're not culling empty strings or if we are but the string isn't empty anyways...
				if(!InCullEmpty || SubstringLength != 0)
				{
					// ... add new string from substring beginning up to the beginning of this delimiter.
					new (OutArray) FString(SubstringEndIndex - SubstringBeginIndex, Start + SubstringBeginIndex);
				}
				// Next substring begins at the end of the discovered delimiter.
				SubstringBeginIndex = SubstringEndIndex + DelimiterLength;
				i = SubstringBeginIndex;
			}
			else
			{
				++i;
			}
		}

		// Add any remaining characters after the last delimiter.
		const int32 SubstringLength = Length - SubstringBeginIndex;
		// If we're not culling empty strings or if we are but the string isn't empty anyways...
		if(!InCullEmpty || SubstringLength != 0)
		{
			// ... add new string from substring beginning up to the beginning of this delimiter.
			new (OutArray) FString(Start + SubstringBeginIndex);
		}
	}

	return OutArray.Num();
}

FString FString::Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase) const
{
	// Previous code used to accidentally accept a nullptr replacement string - this is no longer accepted.
	check(To);

	if (IsEmpty() || !From || !*From)
	{
		return *this;
	}

	// get a pointer into the character data
	const TCHAR* Travel = Data.GetData();

	// precalc the lengths of the replacement strings
	int32 FromLength = FCString::Strlen(From);
	int32 ToLength   = FCString::Strlen(To);

	FString Result;
	while (true)
	{
		// look for From in the remaining string
		const TCHAR* FromLocation = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(Travel, From) : FCString::Strstr(Travel, From);
		if (!FromLocation)
			break;

		// copy everything up to FromLocation
		Result.AppendChars(Travel, FromLocation - Travel);

		// copy over the To
		Result.AppendChars(To, ToLength);

		Travel = FromLocation + FromLength;
	}

	// copy anything left over
	Result += Travel;

	return Result;
}

int32 FString::ReplaceInline(const TCHAR* SearchText, const TCHAR* ReplacementText, ESearchCase::Type SearchCase)
{
	int32 ReplacementCount = 0;

	if (Len() > 0
		&& SearchText != nullptr && *SearchText != 0
		&& ReplacementText != nullptr && (SearchCase == ESearchCase::IgnoreCase || FCString::Strcmp(SearchText, ReplacementText) != 0))
	{
		const int32 NumCharsToReplace = FCString::Strlen(SearchText);
		const int32 NumCharsToInsert = FCString::Strlen(ReplacementText);

		if (NumCharsToInsert == NumCharsToReplace)
		{
			TCHAR* Pos = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(&(*this)[0], SearchText) : FCString::Strstr(&(*this)[0], SearchText);
			while (Pos != nullptr)
			{
				ReplacementCount++;

				// FCString::Strcpy now inserts a terminating zero so can't use that
				for (int32 i = 0; i < NumCharsToInsert; i++)
				{
					Pos[i] = ReplacementText[i];
				}

				if (Pos + NumCharsToReplace - **this < Len())
				{
					Pos = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(Pos + NumCharsToReplace, SearchText) : FCString::Strstr(Pos + NumCharsToReplace, SearchText);
				}
				else
				{
					break;
				}
			}
		}
		else if (Contains(SearchText, SearchCase))
		{
			FString Copy(*this);
			Empty(Len());

			// get a pointer into the character data
			TCHAR* WritePosition = (TCHAR*)Copy.Data.GetData();
			// look for From in the remaining string
			TCHAR* SearchPosition = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(WritePosition, SearchText) : FCString::Strstr(WritePosition, SearchText);
			while (SearchPosition != nullptr)
			{
				ReplacementCount++;

				// replace the first letter of the From with 0 so we can do a strcpy (FString +=)
				*SearchPosition = 0;

				// copy everything up to the SearchPosition
				(*this) += WritePosition;

				// copy over the ReplacementText
				(*this) += ReplacementText;

				// restore the letter, just so we don't have 0's in the string
				*SearchPosition = *SearchText;

				WritePosition = SearchPosition + NumCharsToReplace;
				SearchPosition = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(WritePosition, SearchText) : FCString::Strstr(WritePosition, SearchText);
			}

			// copy anything left over
			(*this) += WritePosition;
		}
	}

	return ReplacementCount;
}


/**
 * Returns a copy of this string with all quote marks escaped (unless the quote is already escaped)
 */
FString FString::ReplaceQuotesWithEscapedQuotes() const
{
	if (Contains(TEXT("\""), ESearchCase::CaseSensitive))
	{
		FString Result;

		const TCHAR* pChar = **this;

		bool bEscaped = false;
		while ( *pChar != 0 )
		{
			if ( bEscaped )
			{
				bEscaped = false;
			}
			else if ( *pChar == TCHAR('\\') )
			{
				bEscaped = true;
			}
			else if ( *pChar == TCHAR('"') )
			{
				Result += TCHAR('\\');
			}

			Result += *pChar++;
		}
		
		return Result;
	}

	return *this;
}

static const TCHAR* CharToEscapeSeqMap[][2] =
{
	// Always replace \\ first to avoid double-escaping characters
	{ TEXT("\\"), TEXT("\\\\") },
	{ TEXT("\n"), TEXT("\\n")  },
	{ TEXT("\r"), TEXT("\\r")  },
	{ TEXT("\t"), TEXT("\\t")  },
	{ TEXT("\'"), TEXT("\\'")  },
	{ TEXT("\""), TEXT("\\\"") }
};

static const uint32 MaxSupportedEscapeChars = ARRAY_COUNT(CharToEscapeSeqMap);

/**
 * Replaces certain characters with the "escaped" version of that character (i.e. replaces "\n" with "\\n").
 * The characters supported are: { \n, \r, \t, \', \", \\ }.
 *
 * @param	Chars	by default, replaces all supported characters; this parameter allows you to limit the replacement to a subset.
 *
 * @return	a string with all control characters replaced by the escaped version.
 */
FString FString::ReplaceCharWithEscapedChar( const TArray<TCHAR>* Chars/*=nullptr*/ ) const
{
	if ( Len() > 0 && (Chars == nullptr || Chars->Num() > 0) )
	{
		FString Result(*this);
		for ( int32 ChIdx = 0; ChIdx < MaxSupportedEscapeChars; ChIdx++ )
		{
			if ( Chars == nullptr || Chars->Contains(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline(CharToEscapeSeqMap[ChIdx][0], CharToEscapeSeqMap[ChIdx][1]);
			}
		}
		return Result;
	}

	return *this;
}
/**
 * Removes the escape backslash for all supported characters, replacing the escape and character with the non-escaped version.  (i.e.
 * replaces "\\n" with "\n".  Counterpart to ReplaceCharWithEscapedChar().
 */
FString FString::ReplaceEscapedCharWithChar( const TArray<TCHAR>* Chars/*=nullptr*/ ) const
{
	if ( Len() > 0 && (Chars == nullptr || Chars->Num() > 0) )
	{
		FString Result(*this);
		// Spin CharToEscapeSeqMap backwards to ensure we're doing the inverse of ReplaceCharWithEscapedChar
		for ( int32 ChIdx = MaxSupportedEscapeChars - 1; ChIdx >= 0; ChIdx-- )
		{
			if ( Chars == nullptr || Chars->Contains(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline( CharToEscapeSeqMap[ChIdx][1], CharToEscapeSeqMap[ChIdx][0] );
			}
		}
		return Result;
	}

	return *this;
}

/** 
 * Replaces all instances of '\t' with TabWidth number of spaces
 * @param InSpacesPerTab - Number of spaces that a tab represents
 */
FString FString::ConvertTabsToSpaces (const int32 InSpacesPerTab)
{
	//must call this with at least 1 space so the modulus operation works
	check(InSpacesPerTab > 0);

	FString FinalString = *this;
	int32 TabIndex;
	while ((TabIndex = FinalString.Find(TEXT("\t"))) != INDEX_NONE )
	{
		FString LeftSide = FinalString.Left(TabIndex);
		FString RightSide = FinalString.Mid(TabIndex+1);

		FinalString = LeftSide;
		//for a tab size of 4, 
		int32 LineBegin = LeftSide.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, TabIndex);
		if (LineBegin == INDEX_NONE)
		{
			LineBegin = 0;
		}
		int32 CharactersOnLine = (LeftSide.Len()-LineBegin);

		int32 NumSpacesForTab = InSpacesPerTab - (CharactersOnLine % InSpacesPerTab);
		for (int32 i = 0; i < NumSpacesForTab; ++i)
		{
			FinalString.AppendChar(' ');
		}
		FinalString += RightSide;
	}

	return FinalString;
}

// This starting size catches 99.97% of printf calls - there are about 700k printf calls per level
#define STARTING_BUFFER_SIZE		512

VARARG_BODY( FString, FString::Printf, const TCHAR*, VARARG_NONE )
{
	int32		BufferSize	= STARTING_BUFFER_SIZE;
	TCHAR	StartingBuffer[STARTING_BUFFER_SIZE];
	TCHAR*	Buffer		= StartingBuffer;
	int32		Result		= -1;

	// First try to print to a stack allocated location 
	GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );

	// If that fails, start allocating regular memory
	if( Result == -1 )
	{
		Buffer = nullptr;
		while(Result == -1)
		{
			BufferSize *= 2;
			Buffer = (TCHAR*) FMemory::Realloc( Buffer, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		};
	}

	Buffer[Result] = 0;

	FString ResultString(Buffer);

	if( BufferSize != STARTING_BUFFER_SIZE )
	{
		FMemory::Free( Buffer );
	}

	return ResultString;
}

FArchive& operator<<( FArchive& Ar, FString& A )
{
	// > 0 for ANSICHAR, < 0 for UCS2CHAR serialization

	if (Ar.IsLoading())
	{
		int32 SaveNum;
		Ar << SaveNum;

		bool LoadUCS2Char = SaveNum < 0;
		if (LoadUCS2Char)
		{
			SaveNum = -SaveNum;
		}

		// If SaveNum is still less than 0, they must have passed in MIN_INT. Archive is corrupted.
		if (SaveNum < 0)
		{
			Ar.ArIsError = 1;
			Ar.ArIsCriticalError = 1;
			UE_LOG(LogNetSerialization, Error, TEXT("Archive is corrupted"));
			return Ar;
		}

		auto MaxSerializeSize = Ar.GetMaxSerializeSize();
		// Protect against network packets allocating too much memory
		if ((MaxSerializeSize > 0) && (SaveNum > MaxSerializeSize))
		{
			Ar.ArIsError         = 1;
			Ar.ArIsCriticalError = 1;
			UE_LOG( LogNetSerialization, Error, TEXT( "String is too large" ) );
			return Ar;
		}

		// Resize the array only if it passes the above tests to prevent rogue packets from crashing
		A.Data.Empty           (SaveNum);
		A.Data.AddUninitialized(SaveNum);

		if (SaveNum)
		{
			if (LoadUCS2Char)
			{
				// read in the unicode string and byteswap it, etc
				auto Passthru = StringMemoryPassthru<UCS2CHAR>(A.Data.GetData(), SaveNum, SaveNum);
				Ar.Serialize(Passthru.Get(), SaveNum * sizeof(UCS2CHAR));
				// Ensure the string has a null terminator
				Passthru.Get()[SaveNum-1] = '\0';
				Passthru.Apply();

				INTEL_ORDER_TCHARARRAY(A.Data.GetData())

				// Since Microsoft's vsnwprintf implementation raises an invalid parameter warning
				// with a character of 0xffff, scan for it and terminate the string there.
				// 0xffff isn't an actual Unicode character anyway.
				int Index = 0;
				if(A.FindChar(0xffff, Index))
				{
					A[Index] = '\0';
					A.TrimToNullTerminator();		
				}
			}
			else
			{
				auto Passthru = StringMemoryPassthru<ANSICHAR>(A.Data.GetData(), SaveNum, SaveNum);
				Ar.Serialize(Passthru.Get(), SaveNum * sizeof(ANSICHAR));
				// Ensure the string has a null terminator
				Passthru.Get()[SaveNum-1] = '\0';
				Passthru.Apply();
			}

			// Throw away empty string.
			if (SaveNum == 1)
			{
				A.Data.Empty();
			}
		}
	}
	else
	{
		bool  SaveUCS2Char = Ar.IsForcingUnicode() || !FCString::IsPureAnsi(*A);
		int32 Num        = A.Data.Num();
		int32 SaveNum    = SaveUCS2Char ? -Num : Num;

		Ar << SaveNum;

		A.Data.CountBytes( Ar );

		if (SaveNum)
		{
			if (SaveUCS2Char)
			{
				// TODO - This is creating a temporary in order to byte-swap.  Need to think about how to make this not necessary.
				#if !PLATFORM_LITTLE_ENDIAN
					FString  ATemp  = A;
					FString& A      = ATemp;
					INTEL_ORDER_TCHARARRAY(A.Data.GetData());
				#endif

				Ar.Serialize((void*)StringCast<UCS2CHAR>(A.Data.GetData(), Num).Get(), sizeof(UCS2CHAR)* Num);
			}
			else
			{
				Ar.Serialize((void*)StringCast<ANSICHAR>(A.Data.GetData(), Num).Get(), sizeof(ANSICHAR)* Num);
			}
		}
	}

	return Ar;
}

int32 FindMatchingClosingParenthesis(const FString& TargetString, const int32 StartSearch)
{
	check(StartSearch >= 0 && StartSearch <= TargetString.Len());// Check for usage, we do not accept INDEX_NONE like other string functions

	const TCHAR* const StartPosition = (*TargetString) + StartSearch;
	const TCHAR* CurrPosition = StartPosition;
	int32 ParenthesisCount = 0;

	// Move to first open parenthesis
	while (*CurrPosition != 0 && *CurrPosition != TEXT('('))
	{
		++CurrPosition;
	}

	// Did we find the open parenthesis
	if (*CurrPosition == TEXT('('))
	{
		++ParenthesisCount;
		++CurrPosition;

		while (*CurrPosition != 0 && ParenthesisCount > 0)
		{
			if (*CurrPosition == TEXT('('))
			{
				++ParenthesisCount;
			}
			else if (*CurrPosition == TEXT(')'))
			{
				--ParenthesisCount;
			}
			++CurrPosition;
		}

		// Did we find the matching close parenthesis
		if (ParenthesisCount == 0 && *(CurrPosition - 1) == TEXT(')'))
		{
			return StartSearch + ((CurrPosition - 1) - StartPosition);
		}
	}

	return INDEX_NONE;
}

FString SlugStringForValidName(const FString& DisplayString)
{
	FString GeneratedName = DisplayString;

	// Convert the display label, which may consist of just about any possible character, into a
	// suitable name for a UObject (remove whitespace, certain symbols, etc.)
	{
		for ( int32 BadCharacterIndex = 0; BadCharacterIndex < ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++BadCharacterIndex )
		{
			const TCHAR TestChar[2] = { INVALID_OBJECTNAME_CHARACTERS[BadCharacterIndex], 0 };
			const int32 NumReplacedChars = GeneratedName.ReplaceInline(TestChar, TEXT(""));
		}
	}

	return GeneratedName;
}