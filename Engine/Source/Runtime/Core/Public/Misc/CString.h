// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/VarArgs.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Char.h"
#include "HAL/PlatformString.h"

#define MAX_SPRINTF 1024

/** Helper class used to convert CString into a boolean value. */
struct CORE_API FToBoolHelper
{
	static bool FromCStringAnsi( const ANSICHAR* String );
	static bool FromCStringWide( const WIDECHAR* String );
};

/**
 *	Set of basic string utility functions operating on plain C strings. In addition to the 
 *	wrapped C string API,this struct also contains a set of widely used utility functions that
 *  operate on c strings.
 *	There is a specialized implementation for ANSICHAR and WIDECHAR strings provided. To access these
 *	functionality, the convenience typedefs FCString and FCStringAnsi are provided.
 **/
template <typename T>
struct TCString
{
	typedef T CharType;

	/**
	 * Returns whether this string contains only pure ansi characters 
	 * @param Str - string that will be checked
	 **/
	static FORCEINLINE bool IsPureAnsi(const CharType* Str);

	/**
	 * Returns whether this string contains only numeric characters 
	 * @param Str - string that will be checked
	 **/
	static bool IsNumeric(const CharType* Str)
	{
		if (*Str == '-' || *Str == '+')
		{
			Str++;
		}

		bool bHasDot = false;
		while (*Str != '\0')
		{
			if (*Str == '.')
			{
				if (bHasDot)
				{
					return false;
				}
				bHasDot = true;
			}
			else if (!FChar::IsDigit(*Str))
			{
				return false;
			}
			
			++Str;
		}

		return true;
	}
	
	/**
	 * strcpy wrapper
	 *
	 * @param Dest - destination string to copy to
	 * @param Destcount - size of Dest in characters
	 * @param Src - source string
	 * @return destination string
	 */
	static FORCEINLINE CharType* Strcpy( CharType* Dest, SIZE_T DestCount, const CharType* Src );

	/** 
	 * Copy a string with length checking. Behavior differs from strncpy in that last character is zeroed. 
	 *
	 * @param Dest - destination buffer to copy to
	 * @param Src - source buffer to copy from
	 * @param MaxLen - max length of the buffer (including null-terminator)
	 * @return pointer to resulting string buffer
	 */
	static FORCEINLINE CharType* Strncpy( CharType* Dest, const CharType* Src, int32 MaxLen );

	/**
	 * strcpy wrapper
	 * (templated version to automatically handle static destination array case)
	 *
	 * @param Dest - destination string to copy to
	 * @param Src - source string
	 * @return destination string
	 */
	template<SIZE_T DestCount>
	static FORCEINLINE CharType* Strcpy( CharType (&Dest)[DestCount], const CharType* Src ) 
	{
		return Strcpy( Dest, DestCount, Src );
	}

	/**
	 * strcat wrapper
	 *
	 * @param Dest - destination string to copy to
	 * @param Destcount - size of Dest in characters
	 * @param Src - source string
	 * @return destination string
	 */
	static FORCEINLINE CharType* Strcat( CharType* Dest, SIZE_T DestCount, const CharType* Src );

	/**
	 * strcat wrapper
	 * (templated version to automatically handle static destination array case)
	 *
	 * @param Dest - destination string to copy to
	 * @param Src - source string
	 * @return destination string
	 */
	template<SIZE_T DestCount>
	static FORCEINLINE CharType* Strcat( CharType (&Dest)[DestCount], const CharType* Src ) 
	{ 
		return Strcat( Dest, DestCount, Src );
	}

	/** 
	 * Concatenate a string with length checking.
	 *
	 * @param Dest - destination buffer to append to
	 * @param Src - source buffer to copy from
	 * @param MaxLen - max length of the buffer
	 * @return pointer to resulting string buffer
	 */
	static inline CharType* Strncat( CharType* Dest, const CharType* Src, int32 MaxLen )
	{
		int32 Len = Strlen(Dest);
		CharType* NewDest = Dest + Len;
		if( (MaxLen-=Len) > 0 )
		{
			Strncpy( NewDest, Src, MaxLen );
		}
		return Dest;
	}

	/**
	 * strupr wrapper
	 *
	 * @param Dest - destination string to convert
	 * @param Destcount - size of Dest in characters
	 * @return destination string
	 */
	static FORCEINLINE CharType* Strupr( CharType* Dest, SIZE_T DestCount );

	
	/**
	 * strupr wrapper
	 * (templated version to automatically handle static destination array case)
	 *
	 * @param Dest - destination string to convert
	 * @return destination string
	 */
	template<SIZE_T DestCount>
	static FORCEINLINE CharType* Strupr( CharType (&Dest)[DestCount] ) 
	{
		return Strupr( Dest, DestCount );
	}

	/**
	 * strcmp wrapper
	 **/
	static FORCEINLINE int32 Strcmp( const CharType* String1, const CharType* String2 );

	/**
	 * strncmp wrapper
	 */
	static FORCEINLINE int32 Strncmp( const CharType* String1, const CharType* String2, SIZE_T Count);

	/**
	 * stricmp wrapper
	 */
	static FORCEINLINE int32 Stricmp( const CharType* String1, const CharType* String2 );
	
	/**
	 * strnicmp wrapper
	 */
	static FORCEINLINE int32 Strnicmp( const CharType* String1, const CharType* String2, SIZE_T Count );

	/**
	 * Returns a static string that is filled with a variable number of spaces
	 *
	 * @param NumSpaces Number of spaces to put into the string, max of 255
	 * 
	 * @return The string of NumSpaces spaces.
	 */
	static const CharType* Spc( int32 NumSpaces );

	/**
	 * Returns a static string that is filled with a variable number of tabs
	 *
	 * @param NumTabs Number of tabs to put into the string, max of 255
	 * 
	 * @return The string of NumTabs tabs.
	 */
	static const CharType* Tab( int32 NumTabs );

	/**
	 * Find string in string, case sensitive, requires non-alphanumeric lead-in.
	 */
	static const CharType* Strfind( const CharType* Str, const CharType* Find );

	/**
	 * Find string in string, case insensitive, requires non-alphanumeric lead-in.
	 */
	static const CharType* Strifind( const CharType* Str, const CharType* Find );

	/**
	 * Finds string in string, case insensitive, requires the string be surrounded by one the specified
	 * delimiters, or the start or end of the string.
	 */
	static const CharType* StrfindDelim(const CharType* Str, const CharType* Find, const CharType* Delim=LITERAL(CharType, " \t,"));

	/** 
	 * Finds string in string, case insensitive 
	 * @param Str The string to look through
	 * @param Find The string to find inside Str
	 * @return Position in Str if Find was found, otherwise, NULL
	 */
	static const CharType* Stristr(const CharType* Str, const CharType* Find);

	/** 
	 * Finds string in string, case insensitive (non-const version)
	 * @param Str The string to look through
	 * @param Find The string to find inside Str
	 * @return Position in Str if Find was found, otherwise, NULL
	 */
	static CharType* Stristr(CharType* Str, const CharType* Find) { return (CharType*)Stristr((const CharType*)Str, Find); }

	/**
	 * strlen wrapper
	 */
	static FORCEINLINE int32 Strlen( const CharType* String );
	
	/**
	 * strstr wrapper
	 */
	static FORCEINLINE const CharType* Strstr( const CharType* String, const CharType* Find );
	static FORCEINLINE CharType* Strstr( CharType* String, const CharType* Find );

	/**
	 * strchr wrapper
	 */
	static FORCEINLINE const CharType* Strchr( const CharType* String, CharType c );
	static FORCEINLINE CharType* Strchr( CharType* String, CharType c );

	/**
	 * strrchr wrapper
	 */
	static FORCEINLINE const CharType* Strrchr( const CharType* String, CharType c );
	static FORCEINLINE CharType* Strrchr( CharType* String, CharType c );

	/**
	 * strrstr wrapper
	 */
	static FORCEINLINE const CharType* Strrstr( const CharType* String, const CharType* Find );
	static FORCEINLINE CharType* Strrstr( CharType* String, const CharType* Find );

	/**
	 * strspn wrapper
	 */
	static FORCEINLINE int32 Strspn( const CharType* String, const CharType* Mask );

	/**
	 * strcspn wrapper
	 */
	static FORCEINLINE int32 Strcspn( const CharType* String, const CharType* Mask );

	/**
	 * atoi wrapper
	 */
	static FORCEINLINE int32 Atoi( const CharType* String );

	/**
	 * atoi64 wrapper
	 */
	static FORCEINLINE int64 Atoi64( const CharType* String );
	
	/**
	 * atof wrapper
	 */
	static FORCEINLINE float Atof( const CharType* String );

	/**
	 * atod wrapper
	 */
	static FORCEINLINE double Atod( const CharType* String );

	/**
	 * Converts a string into a boolean value
	 *   1, "True", "Yes", GTrue, GYes, and non-zero integers become true
	 *   0, "False", "No", GFalse, GNo, and unparsable values become false
	 *
	 * @return The boolean value
	 */
	static FORCEINLINE bool ToBool( const CharType* String );

	/**
	 * strtoi wrapper
	 */
	static FORCEINLINE int32 Strtoi( const CharType* Start, CharType** End, int32 Base );

	/**
	 * strtoi wrapper
	 */
	static FORCEINLINE int64 Strtoi64( const CharType* Start, CharType** End, int32 Base );

	/**
	 * strtoui wrapper
	 */
	static FORCEINLINE uint64 Strtoui64( const CharType* Start, CharType** End, int32 Base );

	/**
	 * strtok wrapper
	 */
	static FORCEINLINE CharType* Strtok( CharType* TokenString, const CharType* Delim, CharType** Context );

	/** 
	* Standard string formatted print. 
	* @warning: make sure code using FCString::Sprintf allocates enough (>= MAX_SPRINTF) memory for the destination buffer
	*/
	VARARG_DECL( static inline int32, static inline int32, return, Sprintf, VARARG_NONE, const CharType*, VARARG_EXTRA(CharType* Dest), VARARG_EXTRA(Dest) );

	/** 
	 * Safe string formatted print. 
	 */
	VARARG_DECL( static inline int32, static inline int32, return, Snprintf, VARARG_NONE, const CharType*, VARARG_EXTRA(CharType* Dest) VARARG_EXTRA(int32 DestSize), VARARG_EXTRA(Dest) VARARG_EXTRA(DestSize) );

	/**
	* Helper function to write formatted output using an argument list
	*
	* @param Dest - destination string buffer
	* @param DestSize - size of destination buffer
	* @param Count - number of characters to write (not including null terminating character)
	* @param Fmt - string to print
	* @param Args - argument list
	* @return number of characters written or -1 if truncated
	*/
	static FORCEINLINE int32 GetVarArgs( CharType* Dest, SIZE_T DestSize, int32 Count, const CharType*& Fmt, va_list ArgPtr );
};

typedef TCString<TCHAR>    FCString;
typedef TCString<ANSICHAR> FCStringAnsi;
typedef TCString<WIDECHAR> FCStringWide;

/*-----------------------------------------------------------------------------
	generic TCString implementations
-----------------------------------------------------------------------------*/

template <typename CharType = TCHAR>
struct TCStringSpcHelper
{
	/** Number of characters to be stored in string. */
	static const int32 MAX_SPACES = 255;

	/** Number of tabs to be stored in string. */
	static const int32 MAX_TABS = 255;

	static CORE_API const CharType SpcArray[MAX_SPACES + 1];
	static CORE_API const CharType TabArray[MAX_TABS + 1];
};

template <typename T>
const typename TCString<T>::CharType* TCString<T>::Spc( int32 NumSpaces )
{
	check(NumSpaces >= 0 && NumSpaces <= TCStringSpcHelper<T>::MAX_SPACES);
	return TCStringSpcHelper<T>::SpcArray + TCStringSpcHelper<T>::MAX_SPACES - NumSpaces;
}

template <typename T>
const typename TCString<T>::CharType* TCString<T>::Tab( int32 NumTabs )
{
	check(NumTabs >= 0 && NumTabs <= TCStringSpcHelper<T>::MAX_TABS);
	return TCStringSpcHelper<T>::TabArray + TCStringSpcHelper<T>::MAX_TABS - NumTabs;
}

//
// Find string in string, case sensitive, requires non-alphanumeric lead-in.
//
template <typename T>
const typename TCString<T>::CharType* TCString<T>::Strfind(const CharType* Str, const CharType* Find)
{
	if (Find == NULL || Str == NULL)
	{
		return NULL;
	}

	bool Alnum = 0;
	CharType f = *Find;
	int32 Length = Strlen(Find++) - 1;
	CharType c = *Str++;
	while (c)
	{
		if (!Alnum && c == f && !Strncmp(Str, Find, Length))
		{
			return Str - 1;
		}
		Alnum = (c >= LITERAL(CharType, 'A') && c <= LITERAL(CharType, 'Z')) ||
			(c >= LITERAL(CharType, 'a') && c <= LITERAL(CharType, 'z')) ||
			(c >= LITERAL(CharType, '0') && c <= LITERAL(CharType, '9'));
		c = *Str++;
	}
	return NULL;
}

//
// Find string in string, case insensitive, requires non-alphanumeric lead-in.
//
template <typename T>
const typename TCString<T>::CharType* TCString<T>::Strifind( const CharType* Str, const CharType* Find )
{
	if( Find == NULL || Str == NULL )
	{
		return NULL;
	}
	
	bool Alnum  = 0;
	CharType f = ( *Find < LITERAL(CharType, 'a') || *Find > LITERAL(CharType, 'z') ) ? (*Find) : (*Find + LITERAL(CharType,'A') - LITERAL(CharType,'a'));
	int32 Length = Strlen(Find++)-1;
	CharType c = *Str++;
	while( c )
	{
		if( c >= LITERAL(CharType, 'a') && c <= LITERAL(CharType, 'z') )
		{
			c += LITERAL(CharType, 'A') - LITERAL(CharType, 'a');
		}
		if( !Alnum && c==f && !Strnicmp(Str,Find,Length) )
		{
			return Str-1;
		}
		Alnum = (c>=LITERAL(CharType,'A') && c<=LITERAL(CharType,'Z')) || 
				(c>=LITERAL(CharType,'0') && c<=LITERAL(CharType,'9'));
		c = *Str++;
	}
	return NULL;
}

//
// Finds string in string, case insensitive, requires the string be surrounded by one the specified
// delimiters, or the start or end of the string.
//
template <typename T>
const typename TCString<T>::CharType* TCString<T>::StrfindDelim(const CharType* Str, const CharType* Find, const CharType* Delim)
{
	if( Find == NULL || Str == NULL )
	{
		return NULL;
	}

	int32 Length = Strlen(Find);
	const T* Found = Stristr(Str, Find);
	if( Found )
	{
		// check if this occurrence is delimited correctly
		if( (Found==Str || Strchr(Delim, Found[-1])!=NULL) &&								// either first char, or following a delim
			(Found[Length]==LITERAL(CharType,'\0') || Strchr(Delim, Found[Length])!=NULL) )	// either last or with a delim following
		{
			return Found;
		}

		// start searching again after the first matched character
		for(;;)
		{
			Str = Found+1;
			Found = Stristr(Str, Find);

			if( Found == NULL )
			{
				return NULL;
			}

			// check if the next occurrence is delimited correctly
			if( (Strchr(Delim, Found[-1])!=NULL) &&												// match is following a delim
				(Found[Length]==LITERAL(CharType,'\0') || Strchr(Delim, Found[Length])!=NULL) )	// either last or with a delim following
			{
				return Found;
			}
		}
	}

	return NULL;
}

/** 
 * Finds string in string, case insensitive 
 * @param Str The string to look through
 * @param Find The string to find inside Str
 * @return Position in Str if Find was found, otherwise, NULL
 */
template <typename T>
const typename TCString<T>::CharType* TCString<T>::Stristr(const CharType* Str, const CharType* Find)
{
	// both strings must be valid
	if( Find == NULL || Str == NULL )
	{
		return NULL;
	}

	// get upper-case first letter of the find string (to reduce the number of full strnicmps)
	CharType FindInitial = TChar<CharType>::ToUpper(*Find);
	// get length of find string, and increment past first letter
	int32   Length = Strlen(Find++) - 1;
	// get the first letter of the search string, and increment past it
	CharType StrChar = *Str++;
	// while we aren't at end of string...
	while (StrChar)
	{
		// make sure it's upper-case
		StrChar = TChar<CharType>::ToUpper(StrChar);
		// if it matches the first letter of the find string, do a case-insensitive string compare for the length of the find string
		if (StrChar == FindInitial && !Strnicmp(Str, Find, Length))
		{
			// if we found the string, then return a pointer to the beginning of it in the search string
			return Str-1;
		}
		// go to next letter
		StrChar = *Str++;
	}

	// if nothing was found, return NULL
	return NULL;
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strcpy(CharType* Dest, SIZE_T DestCount, const CharType* Src)
{
	return FPlatformString::Strcpy(Dest, DestCount, Src);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strncpy( CharType* Dest, const CharType* Src, int32 MaxLen )
{
	check(MaxLen > 0);
	FPlatformString::Strncpy(Dest, Src, MaxLen);
	return Dest;
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strcat(CharType* Dest, SIZE_T DestCount, const CharType* Src)
{
	return FPlatformString::Strcat(Dest, DestCount, Src);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strupr(CharType* Dest, SIZE_T DestCount)
{
	return FPlatformString::Strupr(Dest, DestCount);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strcmp( const CharType* String1, const CharType* String2 )
{
	return FPlatformString::Strcmp(String1, String2);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strncmp( const CharType* String1, const CharType* String2, SIZE_T Count)
{
	return FPlatformString::Strncmp(String1, String2, Count);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Stricmp( const CharType* String1, const CharType* String2 )
{
	return FPlatformString::Stricmp(String1, String2);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strnicmp( const CharType* String1, const CharType* String2, SIZE_T Count )
{
	return FPlatformString::Strnicmp(String1, String2, Count);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strlen( const CharType* String ) 
{
	return FPlatformString::Strlen(String);
}

template <typename T> FORCEINLINE
const typename TCString<T>::CharType* TCString<T>::Strstr( const CharType* String, const CharType* Find )
{
	return FPlatformString::Strstr(String, Find);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strstr( CharType* String, const CharType* Find )
{
	return (CharType*)FPlatformString::Strstr(String, Find);
}

template <typename T> FORCEINLINE
const typename TCString<T>::CharType* TCString<T>::Strchr( const CharType* String, CharType c ) 
{ 
	return FPlatformString::Strchr(String, c);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strchr( CharType* String, CharType c ) 
{ 
	return (CharType*)FPlatformString::Strchr(String, c);
}

template <typename T> FORCEINLINE
const typename TCString<T>::CharType* TCString<T>::Strrchr( const CharType* String, CharType c )
{ 
	return FPlatformString::Strrchr(String, c);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strrchr( CharType* String, CharType c )
{ 
	return (CharType*)FPlatformString::Strrchr(String, c);
}

template <typename T> FORCEINLINE
const typename TCString<T>::CharType* TCString<T>::Strrstr( const CharType* String, const CharType* Find )
{
	return Strrstr((CharType*)String, Find);
}

template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strrstr( CharType* String, const CharType* Find )
{
	if (*Find == (CharType)0)
	{
		return String + Strlen(String);
	}

	CharType* Result = nullptr;
	for (;;)
	{
		CharType* Found = Strstr(String, Find);
		if (!Found)
		{
			return Result;
		}

		Result = Found;
		String = Found + 1;
	}
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strspn( const CharType* String, const CharType* Mask )
{
	const TCHAR* StringIt = String;
	while (*StringIt)
	{
		for (const TCHAR* MaskIt = Mask; *MaskIt; ++MaskIt)
		{
			if (*StringIt == *MaskIt)
			{
				goto NextChar;
			}
		}

		return StringIt - String;

	NextChar:
		++StringIt;
	}

	return StringIt - String;
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strcspn( const CharType* String, const CharType* Mask )
{
	const TCHAR* StringIt = String;
	while (*StringIt)
	{
		for (const TCHAR* MaskIt = Mask; *MaskIt; ++MaskIt)
		{
			if (*StringIt == *MaskIt)
			{
				return StringIt - String;
			}
		}

		++StringIt;
	}

	return StringIt - String;
}

template <typename T> FORCEINLINE 
int32 TCString<T>::Atoi( const CharType* String ) 
{
	return FPlatformString::Atoi(String);
}

template <typename T> FORCEINLINE
int64 TCString<T>::Atoi64( const CharType* String )
{ 
	return FPlatformString::Atoi64(String);
}

template <typename T> FORCEINLINE
float TCString<T>::Atof( const CharType* String )
{ 
	return FPlatformString::Atof(String);
}

template <typename T> FORCEINLINE
double TCString<T>::Atod( const CharType* String )
{ 
	return FPlatformString::Atod(String);
}

template <typename T> FORCEINLINE
int32 TCString<T>::Strtoi( const CharType* Start, CharType** End, int32 Base ) 
{ 
	return FPlatformString::Strtoi(Start, End, Base);
}

template <typename T> FORCEINLINE
int64 TCString<T>::Strtoi64( const CharType* Start, CharType** End, int32 Base ) 
{ 
	return FPlatformString::Strtoi64(Start, End, Base);
}

template <typename T> FORCEINLINE
uint64 TCString<T>::Strtoui64( const CharType* Start, CharType** End, int32 Base ) 
{ 
	return FPlatformString::Strtoui64(Start, End, Base);
}


template <typename T> FORCEINLINE
typename TCString<T>::CharType* TCString<T>::Strtok( CharType* TokenString, const CharType* Delim, CharType** Context )
{ 
	return FPlatformString::Strtok(TokenString, Delim, Context);
}

template <typename T> FORCEINLINE
int32 TCString<T>::GetVarArgs( CharType* Dest, SIZE_T DestSize, int32 Count, const CharType*& Fmt, va_list ArgPtr )
{
	return FPlatformString::GetVarArgs(Dest, DestSize, Count, Fmt, ArgPtr);
}


/*-----------------------------------------------------------------------------
	TCString<WIDECHAR> specializations
-----------------------------------------------------------------------------*/
template <> FORCEINLINE
bool TCString<WIDECHAR>::IsPureAnsi(const WIDECHAR* Str)
{
	for( ; *Str; Str++ )
	{
		if( *Str>0x7f )
		{
			return 0;
		}
	}
	return 1;
}


template <> inline
VARARG_BODY( int32, TCString<WIDECHAR>::Sprintf, const CharType*, VARARG_EXTRA(CharType* Dest) )
{
	int32	Result = -1;
	GET_VARARGS_RESULT_WIDE( Dest, MAX_SPRINTF, MAX_SPRINTF-1, Fmt, Fmt, Result );
	return Result;
}

template <> inline
VARARG_BODY( int32, TCString<WIDECHAR>::Snprintf, const CharType*, VARARG_EXTRA(CharType* Dest) VARARG_EXTRA(int32 DestSize) )
{
	int32	Result = -1;
	GET_VARARGS_RESULT_WIDE( Dest, DestSize, DestSize-1, Fmt, Fmt, Result );
	return Result;
}

template <> 
FORCEINLINE bool TCString<TCHAR>::ToBool(const WIDECHAR* Str)
{
	return FToBoolHelper::FromCStringWide(Str);
}

/*-----------------------------------------------------------------------------
	TCString<ANSICHAR> specializations
-----------------------------------------------------------------------------*/
template <> FORCEINLINE bool TCString<ANSICHAR>::IsPureAnsi(const CharType* Str)
{
	return true;
}

template <> inline
VARARG_BODY( int32, TCString<ANSICHAR>::Sprintf, const CharType*, VARARG_EXTRA(CharType* Dest) )
{
	int32	Result = -1;
	GET_VARARGS_RESULT_ANSI( Dest, MAX_SPRINTF, MAX_SPRINTF-1, Fmt, Fmt, Result );
	return Result;
}

template <> inline
VARARG_BODY( int32, TCString<ANSICHAR>::Snprintf, const CharType*, VARARG_EXTRA(CharType* Dest) VARARG_EXTRA(int32 DestSize) )
{
	int32	Result = -1;
	GET_VARARGS_RESULT_ANSI( Dest, DestSize, DestSize-1, Fmt, Fmt, Result );
	return Result;
}

template <> 
FORCEINLINE bool TCString<ANSICHAR>::ToBool(const ANSICHAR* Str)
{
	return FToBoolHelper::FromCStringAnsi(Str);
}
