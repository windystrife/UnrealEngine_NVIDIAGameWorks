// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Char.h"
#include "GenericPlatform/GenericPlatformStricmp.h"
#include "GenericPlatform/GenericPlatformString.h"
#include <wchar.h>

#if PLATFORM_APPLE || PLATFORM_LINUX || PLATFORM_HTML5 || PLATFORM_PS4 || PLATFORM_SWITCH

/**
* Standard implementation
**/
struct FStandardPlatformString : public FGenericPlatformString
{
	template <typename CharType>
	static inline CharType* Strupr(CharType* Dest, SIZE_T DestCount)
	{
		for (CharType* Char = Dest; *Char && DestCount > 0; Char++, DestCount--)
		{
			*Char = TChar<CharType>::ToUpper(*Char);
		}
		return Dest;
	}

public:
	/**
	 * Compares two strings case-insensitive.
	 *
	 * @param String1 First string to compare.
	 * @param String2 Second string to compare.
	 *
	 * @returns Zero if both strings are equal. Greater than zero if first
	 *          string is greater than the second one. Less than zero
	 *          otherwise.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 Stricmp(const CharType1* String1, const CharType2* String2)
	{
		return FGenericPlatformStricmp::Stricmp(String1, String2);
	}

	template <typename CharType>
	static inline int32 Strnicmp( const CharType* String1, const CharType* String2, SIZE_T Count )
	{
		// walk the strings, comparing them case insensitively, up to a max size
		for (; (*String1 || *String2) && Count > 0; String1++, String2++, Count--)
		{
			if(*String1 != *String2)
			{
				CharType Char1 = TChar<CharType>::ToUpper(*String1), Char2 = TChar<CharType>::ToUpper(*String2);
				if (Char1 != Char2)
				{
					return Char1 - Char2;
				}
			}
		}
		return 0;
	}

	/**
	 * Unicode implementation
	 **/
	static FORCEINLINE WIDECHAR* Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		return wcscpy(Dest, Src);
	}

	static FORCEINLINE WIDECHAR* Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen)
	{
		wcsncpy(Dest, Src, MaxLen-1);
		Dest[MaxLen-1]=0;
		return Dest;
	}

	static FORCEINLINE WIDECHAR* Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		return (WIDECHAR*)wcscat( Dest, Src );
	}

	static FORCEINLINE int32 Strcmp( const WIDECHAR* String1, const WIDECHAR* String2 )
	{
		return wcscmp(String1, String2);
	}

	static FORCEINLINE int32 Strncmp( const WIDECHAR* String1, const WIDECHAR* String2, SIZE_T Count )
	{
		return wcsncmp( String1, String2, Count );
	}

	static FORCEINLINE int32 Strlen( const WIDECHAR* String )
	{
		return wcslen( String );
	}

	static FORCEINLINE const WIDECHAR* Strstr( const WIDECHAR* String, const WIDECHAR* Find)
	{
		return wcsstr( String, Find );
	}

	static FORCEINLINE const WIDECHAR* Strchr( const WIDECHAR* String, WIDECHAR C)
	{
		return wcschr( String, C );
	}

	static FORCEINLINE const WIDECHAR* Strrchr( const WIDECHAR* String, WIDECHAR C)
	{
		return wcsrchr( String, C );
	}

	static FORCEINLINE int32 Atoi(const WIDECHAR* String)
	{
		return wcstol( String, NULL, 10 );
	}

	static FORCEINLINE int64 Atoi64(const WIDECHAR* String)
	{
		return wcstoll( String, NULL, 10 );
	}

	static FORCEINLINE float Atof(const WIDECHAR* String)
	{
		return wcstof( String, NULL );
	}

	static FORCEINLINE double Atod(const WIDECHAR* String)
	{
		return wcstod( String, NULL );
	}

	static FORCEINLINE int32 Strtoi( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
	{
		return wcstol( Start, End, Base );
	}

	static FORCEINLINE int64 Strtoi64( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
	{
		return wcstoll( Start, End, Base );
	}

	static FORCEINLINE uint64 Strtoui64( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
	{
		return wcstoull( Start, End, Base );
	}

	static FORCEINLINE WIDECHAR* Strtok(WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context)
	{
		return wcstok(StrToken, Delim, Context);
	}

#if PLATFORM_USE_SYSTEM_VSWPRINTF
	static int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr )
	{
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
		// fix up the Fmt string, as fast as possible, without using an FString
		const WIDECHAR* OldFormat = Fmt;
		WIDECHAR* NewFormat = (WIDECHAR*)alloca((Strlen(Fmt) * 2 + 1) * sizeof(WIDECHAR));

		int32 NewIndex = 0;

		for (; *OldFormat != 0; NewIndex++, OldFormat++)
		{
			// fix up %s -> %ls and %c -> %lc
			if (OldFormat[0] == LITERAL(WIDECHAR, '%'))
			{
				NewFormat[NewIndex++] = *OldFormat++;

				if (*OldFormat == LITERAL(WIDECHAR, '%'))
				{
					NewFormat[NewIndex] = *OldFormat;
				}
				else
				{
					const WIDECHAR* NextChar = OldFormat;

					while(*NextChar != 0 && !FChar::IsAlpha(*NextChar))
					{
						NewFormat[NewIndex++] = *NextChar;
						++NextChar;
					};

					if (*NextChar == LITERAL(WIDECHAR, 's') || *NextChar == LITERAL(WIDECHAR, 'c'))
					{
						NewFormat[NewIndex++] = LITERAL(WIDECHAR, 'l');
						NewFormat[NewIndex] = *NextChar;
					}
					else
					{
						NewFormat[NewIndex] = *NextChar;
					}

					OldFormat = NextChar;
				}
			}
			else
			{
				NewFormat[NewIndex] = *OldFormat;
			}
		}
		NewFormat[NewIndex] = 0;
		int32 Result = vswprintf( Dest, Count, NewFormat, ArgPtr);
#else
		int32 Result = vswprintf( Dest, Count, Fmt, ArgPtr);
#endif
		va_end( ArgPtr );
		return Result;
	}
#else // PLATFORM_USE_SYSTEM_VSWPRINTF
	static int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr );
#endif // PLATFORM_USE_SYSTEM_VSWPRINTF

	/**
	 * Ansi implementation
	 **/
	static FORCEINLINE ANSICHAR* Strcpy(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return strcpy( Dest, Src );
	}

	static FORCEINLINE ANSICHAR* Strncpy(ANSICHAR* Dest, const ANSICHAR* Src, int32 MaxLen)
	{
		::strncpy(Dest, Src, MaxLen);
		Dest[MaxLen-1]=0;
		return Dest;
	}

	static FORCEINLINE ANSICHAR* Strcat(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return strcat( Dest, Src );
	}

	static FORCEINLINE int32 Strcmp( const ANSICHAR* String1, const ANSICHAR* String2 )
	{
		return strcmp(String1, String2);
	}

	static FORCEINLINE int32 Strncmp( const ANSICHAR* String1, const ANSICHAR* String2, SIZE_T Count )
	{
		return strncmp( String1, String2, Count );
	}

	static FORCEINLINE int32 Strlen( const ANSICHAR* String )
	{
		return strlen( String );
	}

	static FORCEINLINE const ANSICHAR* Strstr( const ANSICHAR* String, const ANSICHAR* Find)
	{
		return strstr(String, Find);
	}

	static FORCEINLINE const ANSICHAR* Strchr( const ANSICHAR* String, ANSICHAR C)
	{
		return strchr(String, C);
	}

	static FORCEINLINE const ANSICHAR* Strrchr( const ANSICHAR* String, ANSICHAR C)
	{
		return strrchr(String, C);
	}

	static FORCEINLINE int32 Atoi(const ANSICHAR* String)
	{
		return atoi( String );
	}

	static FORCEINLINE int64 Atoi64(const ANSICHAR* String)
	{
		return strtoll( String, NULL, 10 );
	}

	static FORCEINLINE float Atof(const ANSICHAR* String)
	{
		return (float)atof( String );
	}

	static FORCEINLINE double Atod(const ANSICHAR* String)
	{
		return atof( String );
	}

	static FORCEINLINE int32 Strtoi( const ANSICHAR* Start, ANSICHAR** End, int32 Base )
	{
		return strtol( Start, End, Base );
	}

	static FORCEINLINE int64 Strtoi64( const ANSICHAR* Start, ANSICHAR** End, int32 Base )
	{
		return strtoll(Start, End, Base);
	}

	static FORCEINLINE uint64 Strtoui64( const ANSICHAR* Start, ANSICHAR** End, int32 Base )
	{
		return strtoull(Start, End, Base);
	}

	static FORCEINLINE ANSICHAR* Strtok(ANSICHAR* StrToken, const ANSICHAR* Delim, ANSICHAR** Context)
	{
		return strtok(StrToken, Delim);
	}

	static int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr )
	{
		int32 Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
		va_end( ArgPtr );
		return Result;
	}

	/**
	 * UCS2 implementation
	 **/

	static FORCEINLINE int32 Strlen( const UCS2CHAR* String )
	{
		int32 Result = 0;
		while (*String++)
		{
			++Result;
		}

		return Result;
	}
};

#endif
