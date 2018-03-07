// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This file contains the classes used when converting strings between
// standards (ANSI, UNICODE, etc.)

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Misc/CString.h"


#define DEFAULT_STRING_CONVERSION_SIZE 128u
#define UNICODE_BOGUS_CHAR_CODEPOINT   '?'


template <typename From, typename To>
class TStringConvert
{
public:
	typedef From FromType;
	typedef To   ToType;

	FORCEINLINE static void Convert(To* Dest, int32 DestLen, const From* Source, int32 SourceLen)
	{
		To* Result = FPlatformString::Convert(Dest, DestLen, Source, SourceLen, (To)UNICODE_BOGUS_CHAR_CODEPOINT);
		check(Result);
	}

	static int32 ConvertedLength(const From* Source, int32 SourceLen)
	{
		return FPlatformString::ConvertedLength<To>(Source, SourceLen);
	}
};

/**
 * This is a basic ANSICHAR* wrapper which swallows all output written through it.
 */
struct FNulPointerIterator
{
	FNulPointerIterator()
		: Ptr_(NULL)
	{
	}

	const FNulPointerIterator& operator* () const {         return *this; }
	const FNulPointerIterator& operator++()       { ++Ptr_; return *this; }
	const FNulPointerIterator& operator++(int)    { ++Ptr_; return *this; }

	ANSICHAR operator=(ANSICHAR Val) const
	{
		return Val;
	}

	friend int32 operator-(FNulPointerIterator Lhs, FNulPointerIterator Rhs)
	{
		return Lhs.Ptr_ - Rhs.Ptr_;
	}

	ANSICHAR* Ptr_;
};

// This should be replaced with Platform stuff when FPlatformString starts to know about UTF-8.
class FTCHARToUTF8_Convert
{
public:
	typedef TCHAR    FromType;
	typedef ANSICHAR ToType;

	// I wrote this function for originally for PhysicsFS. --ryan.
	// !!! FIXME: Maybe this shouldn't be inline...
	template <typename OutputIterator>
	static void utf8fromcodepoint(uint32 cp, OutputIterator* _dst, int32 *_len)
	{
		OutputIterator dst = *_dst;
		int32          len = *_len;

		if (len == 0)
			return;

		if (cp > 0x10FFFF)   // No Unicode codepoints above 10FFFFh, (for now!)
		{
			cp = UNICODE_BOGUS_CHAR_CODEPOINT;
		}
		else if ((cp == 0xFFFE) || (cp == 0xFFFF))  // illegal values.
		{
			cp = UNICODE_BOGUS_CHAR_CODEPOINT;
		}
		else
		{
			// There are seven "UTF-16 surrogates" that are illegal in UTF-8.
			switch (cp)
			{
				case 0xD800:
				case 0xDB7F:
				case 0xDB80:
				case 0xDBFF:
				case 0xDC00:
				case 0xDF80:
				case 0xDFFF:
					cp = UNICODE_BOGUS_CHAR_CODEPOINT;
			}
		}

		// Do the encoding...
		if (cp < 0x80)
		{
			*(dst++) = (char) cp;
			len--;
		}

		else if (cp < 0x800)
		{
			if (len < 2)
			{
				len = 0;
			}
			else
			{
				*(dst++) = (char) ((cp >> 6) | 128 | 64);
				*(dst++) = (char) (cp & 0x3F) | 128;
				len -= 2;
			}
		}

		else if (cp < 0x10000)
		{
			if (len < 3)
			{
				len = 0;
			}
			else
			{
				*(dst++) = (char) ((cp >> 12) | 128 | 64 | 32);
				*(dst++) = (char) ((cp >> 6) & 0x3F) | 128;
				*(dst++) = (char) (cp & 0x3F) | 128;
				len -= 3;
			}
		}

		else
		{
			if (len < 4)
			{
				len = 0;
			}
			else
			{
				*(dst++) = (char) ((cp >> 18) | 128 | 64 | 32 | 16);
				*(dst++) = (char) ((cp >> 12) & 0x3F) | 128;
				*(dst++) = (char) ((cp >> 6) & 0x3F) | 128;
				*(dst++) = (char) (cp & 0x3F) | 128;
				len -= 4;
			}
		}

		*_dst = dst;
		*_len = len;
	}

	/**
	 * Converts the string to the desired format.
	 *
	 * @param Dest      The destination buffer of the converted string.
	 * @param DestLen   The length of the destination buffer.
	 * @param Source    The source string to convert.
	 * @param SourceLen The length of the source string.
	 */
	static FORCEINLINE void Convert(ANSICHAR* Dest, int32 DestLen, const TCHAR* Source, int32 SourceLen)
	{
		// Now do the conversion
		// You have to do this even if !UNICODE, since high-ASCII chars
		//  become multibyte. If you aren't using UNICODE and aren't using
		//  a Latin1 charset, you are just screwed, since we don't handle
		//  codepages, etc.
		while (SourceLen--)
		{
			utf8fromcodepoint((uint32)*Source++, &Dest, &DestLen);
		}
	}

	/**
	 * Determines the length of the converted string.
	 *
	 * @return The length of the string in UTF-8 code units.
	 */
	static int32 ConvertedLength(const TCHAR* Source, int32 SourceLen)
	{
		FNulPointerIterator DestStart;
		FNulPointerIterator Dest;
		int32               DestLen = SourceLen * 4;
		while (SourceLen--)
		{
			utf8fromcodepoint((uint32)*Source++, &Dest, &DestLen);
		}
		return Dest - DestStart;
	}
};

// This should be replaced with Platform stuff when FPlatformString starts to know about UTF-8.
// Also, it's dangerous as it may read past the provided memory buffer if passed a malformed UTF-8 string.
class FUTF8ToTCHAR_Convert
{
public:
	typedef ANSICHAR FromType;
	typedef TCHAR    ToType;

	static uint32 utf8codepoint(const ANSICHAR **_str)
	{
		const char *str = *_str;
		uint32 retval = 0;
		uint32 octet = (uint32) ((uint8) *str);
		uint32 octet2, octet3, octet4;

		if (octet < 128)  // one octet char: 0 to 127
		{
			(*_str)++;  // skip to next possible start of codepoint.
			return(octet);
		}
		else if (octet < 192)  // bad (starts with 10xxxxxx).
		{
			// Apparently each of these is supposed to be flagged as a bogus
			//  char, instead of just resyncing to the next valid codepoint.
			(*_str)++;  // skip to next possible start of codepoint.
			return UNICODE_BOGUS_CHAR_CODEPOINT;
		}
		else if (octet < 224)  // two octets
		{
			octet -= (128+64);
			octet2 = (uint32) ((uint8) *(++str));
			if ((octet2 & (128 + 64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			retval = ((octet << 6) | (octet2 - 128));
			if ((retval >= 0x80) && (retval <= 0x7FF))
			{
				*_str += 2;  // skip to next possible start of codepoint.
				return retval;
			}
		}
		else if (octet < 240)  // three octets
		{
			octet -= (128+64+32);
			octet2 = (uint32) ((uint8) *(++str));
			if ((octet2 & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet3 = (uint32) ((uint8) *(++str));
			if ((octet3 & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			retval = ( ((octet << 12)) | ((octet2-128) << 6) | ((octet3-128)) );

			// There are seven "UTF-16 surrogates" that are illegal in UTF-8.
			switch (retval)
			{
				case 0xD800:
				case 0xDB7F:
				case 0xDB80:
				case 0xDBFF:
				case 0xDC00:
				case 0xDF80:
				case 0xDFFF:
					(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
					return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			// 0xFFFE and 0xFFFF are illegal, too, so we check them at the edge.
			if ((retval >= 0x800) && (retval <= 0xFFFD))
			{
				*_str += 3;  // skip to next possible start of codepoint.
				return retval;
			}
		}
		else if (octet < 248)  // four octets
		{
			octet -= (128+64+32+16);
			octet2 = (uint32) ((uint8) *(++str));
			if ((octet2 & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet3 = (uint32) ((uint8) *(++str));
			if ((octet3 & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet4 = (uint32) ((uint8) *(++str));
			if ((octet4 & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			retval = ( ((octet << 18)) | ((octet2 - 128) << 12) |
						((octet3 - 128) << 6) | ((octet4 - 128)) );
			if ((retval >= 0x10000) && (retval <= 0x10FFFF))
			{
				*_str += 4;  // skip to next possible start of codepoint.
				return retval;
			}
		}
		// Five and six octet sequences became illegal in rfc3629.
		//  We throw the codepoint away, but parse them to make sure we move
		//  ahead the right number of bytes and don't overflow the buffer.
		else if (octet < 252)  // five octets
		{
			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			*_str += 5;  // skip to next possible start of codepoint.
			return UNICODE_BOGUS_CHAR_CODEPOINT;
		}

		else  // six octets
		{
			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			octet = (uint32) ((uint8) *(++str));
			if ((octet & (128+64)) != 128)  // Format isn't 10xxxxxx?
			{
				(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
				return UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			*_str += 6;  // skip to next possible start of codepoint.
			return UNICODE_BOGUS_CHAR_CODEPOINT;
		}

		(*_str)++;  // Sequence was not valid UTF-8. Skip the first byte and continue.
		return UNICODE_BOGUS_CHAR_CODEPOINT;  // catch everything else.
	}

	/**
	 * Converts the string to the desired format.
	 *
	 * @param Dest      The destination buffer of the converted string.
	 * @param DestLen   The length of the destination buffer.
	 * @param Source    The source string to convert.
	 * @param SourceLen The length of the source string.
	 */
	static FORCEINLINE void Convert(TCHAR* Dest, int32 DestLen, const ANSICHAR* Source, int32 SourceLen)
	{
		// Now do the conversion
		// You have to do this even if !UNICODE, since high-ASCII chars
		//  become multibyte. If you aren't using UNICODE and aren't using
		//  a Latin1 charset, you are just screwed, since we don't handle
		//  codepages, etc.
		const ANSICHAR* SourceEnd = Source + SourceLen;
		while (Source < SourceEnd)
		{
			uint32 cp = utf8codepoint(&Source);

			// Please note that we're truncating this to a UCS-2 Windows TCHAR.
			//  A UCS-4 Unix wchar_t can hold this, and we're ignoring UTF-16 for now.
			if (cp > 0xFFFF)
			{
				cp = UNICODE_BOGUS_CHAR_CODEPOINT;
			}

			*Dest++ = cp;
		}
	}

	/**
	 * Determines the length of the converted string.
	 *
	 * @return The length of the string in UTF-8 code units.
	 */
	static int32 ConvertedLength(const ANSICHAR* Source, int32 SourceLen)
	{
		int32           DestLen   = 0;
		const ANSICHAR* SourceEnd = Source + SourceLen;
		while (Source < SourceEnd)
		{
			utf8codepoint(&Source);
			++DestLen;
		}
		return DestLen;
	}
};

struct ENullTerminatedString
{
	enum Type
	{
		No  = 0,
		Yes = 1
	};
};

/**
 * Class takes one type of string and converts it to another. The class includes a
 * chunk of presized memory of the destination type. If the presized array is
 * too small, it mallocs the memory needed and frees on the class going out of
 * scope.
 */
template<typename Converter, int32 DefaultConversionSize = DEFAULT_STRING_CONVERSION_SIZE>
class TStringConversion : private Converter, private TInlineAllocator<DefaultConversionSize>::template ForElementType<typename Converter::ToType>
{
	typedef typename TInlineAllocator<DefaultConversionSize>::template ForElementType<typename Converter::ToType> AllocatorType;

	typedef typename Converter::FromType FromType;
	typedef typename Converter::ToType   ToType;

	/**
	 * Converts the data by using the Convert() method on the base class
	 */
	void Init(const FromType* Source, int32 SourceLen, ENullTerminatedString::Type NullTerminated)
	{
		StringLength = Converter::ConvertedLength(Source, SourceLen);

		int32 BufferSize = StringLength + NullTerminated;

		AllocatorType::ResizeAllocation(0, BufferSize, sizeof(ToType));

		Ptr = (ToType*)AllocatorType::GetAllocation();
		Converter::Convert(Ptr, BufferSize, Source, SourceLen + NullTerminated);
	}

public:
	explicit TStringConversion(const FromType* Source)
	{
		if (Source)
		{
			Init(Source, TCString<FromType>::Strlen(Source), ENullTerminatedString::Yes);
		}
		else
		{
			Ptr          = NULL;
			StringLength = 0;
		}
	}

	TStringConversion(const FromType* Source, int32 SourceLen)
	{
		if (Source)
		{
			Init(Source, SourceLen, ENullTerminatedString::No);
		}
		else
		{
			Ptr          = NULL;
			StringLength = 0;
		}
	}

	/**
	 * Move constructor
	 */
	TStringConversion(TStringConversion&& Other)
		: Converter(MoveTemp((Converter&)Other))
	{
		AllocatorType::MoveToEmpty(Other);
	}

	/**
	 * Accessor for the converted string.
	 *
	 * @return A const pointer to the null-terminated converted string.
	 */
	FORCEINLINE const ToType* Get() const
	{
		return Ptr;
	}

	/**
	 * Length of the converted string.
	 *
	 * @return The number of characters in the converted string, excluding any null terminator.
	 */
	FORCEINLINE int32 Length() const
	{
		return StringLength;
	}

private:
	// Non-copyable
	TStringConversion(const TStringConversion&);
	TStringConversion& operator=(const TStringConversion&);

	ToType* Ptr;
	int32   StringLength;
};


/**
 * NOTE: The objects these macros declare have very short lifetimes. They are
 * meant to be used as parameters to functions. You cannot assign a variable
 * to the contents of the converted string as the object will go out of
 * scope and the string released.
 *
 * NOTE: The parameter you pass in MUST be a proper string, as the parameter
 * is typecast to a pointer. If you pass in a char, not char* it will compile
 * and then crash at runtime.
 *
 * Usage:
 *
 *		SomeApi(TCHAR_TO_ANSI(SomeUnicodeString));
 *
 *		const char* SomePointer = TCHAR_TO_ANSI(SomeUnicodeString); <--- Bad!!!
 */

// These should be replaced with StringCasts when FPlatformString starts to know about UTF-8.
typedef TStringConversion<FTCHARToUTF8_Convert> FTCHARToUTF8;
typedef TStringConversion<FUTF8ToTCHAR_Convert> FUTF8ToTCHAR;

// Usage of these should be replaced with StringCasts.
#define TCHAR_TO_ANSI(str) (ANSICHAR*)StringCast<ANSICHAR>(static_cast<const TCHAR*>(str)).Get()
#define ANSI_TO_TCHAR(str) (TCHAR*)StringCast<TCHAR>(static_cast<const ANSICHAR*>(str)).Get()
#define TCHAR_TO_UTF8(str) (ANSICHAR*)FTCHARToUTF8((const TCHAR*)str).Get()
#define UTF8_TO_TCHAR(str) (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)str).Get()

// This seemingly-pointless class is intended to be API-compatible with TStringConversion
// and is returned by StringCast when no string conversion is necessary.
template <typename T>
class TStringPointer
{
public:
	FORCEINLINE explicit TStringPointer(const T* InPtr)
		: Ptr(InPtr)
	{
	}

	FORCEINLINE const T* Get() const
	{
		return Ptr;
	}

	FORCEINLINE int32 Length() const
	{
		return Ptr ? TCString<T>::Strlen(Ptr) : 0;
	}

private:
	const T* Ptr;
};

/**
 * StringCast example usage:
 *
 * void Func(const FString& Str)
 * {
 *     auto Src = StringCast<ANSICHAR>();
 *     const ANSICHAR* Ptr = Src.Get(); // Ptr is a pointer to an ANSICHAR representing the potentially-converted string data.
 * }
 *
 */

/**
 * Creates an object which acts as a source of a given string type.  See example above.
 *
 * @param Str The null-terminated source string to convert.
 */
template <typename To, typename From>
FORCEINLINE typename TEnableIf<FPlatformString::TAreEncodingsCompatible<To, From>::Value, TStringPointer<To>>::Type StringCast(const From* Str)
{
	return TStringPointer<To>((const To*)Str);
}

/**
 * Creates an object which acts as a source of a given string type.  See example above.
 *
 * @param Str The null-terminated source string to convert.
 */
template <typename To, typename From>
FORCEINLINE typename TEnableIf<!FPlatformString::TAreEncodingsCompatible<To, From>::Value, TStringConversion<TStringConvert<From, To>>>::Type StringCast(const From* Str)
{
	return TStringConversion<TStringConvert<From, To>>(Str);
}

/**
 * Creates an object which acts as a source of a given string type.  See example above.
 *
 * @param Str The source string to convert, not necessarily null-terminated.
 * @param Len The number of From elements in Str.
 */
template <typename To, typename From>
FORCEINLINE typename TEnableIf<FPlatformString::TAreEncodingsCompatible<To, From>::Value, TStringPointer<To>>::Type StringCast(const From* Str, int32 Len)
{
	return TStringPointer<To>((const To*)Str);
}

/**
 * Creates an object which acts as a source of a given string type.  See example above.
 *
 * @param Str The source string to convert, not necessarily null-terminated.
 * @param Len The number of From elements in Str.
 */
template <typename To, typename From>
FORCEINLINE typename TEnableIf<!FPlatformString::TAreEncodingsCompatible<To, From>::Value, TStringConversion<TStringConvert<From, To>>>::Type StringCast(const From* Str, int32 Len)
{
	return TStringConversion<TStringConvert<From, To>>(Str, Len);
}


/**
 * Casts one fixed-width char type into another.
 *
 * @param Ch The character to convert.
 * @return The converted character.
 */
template <typename To, typename From>
FORCEINLINE To CharCast(From Ch)
{
	To Result;
	FPlatformString::Convert(&Result, 1, &Ch, 1, (To)UNICODE_BOGUS_CHAR_CODEPOINT);
	return Result;
}

/**
 * This class is returned by StringPassthru and is not intended to be used directly.
 */
template <typename ToType, typename FromType>
class TStringPassthru : private TInlineAllocator<DEFAULT_STRING_CONVERSION_SIZE>::template ForElementType<FromType>
{
	typedef typename TInlineAllocator<DEFAULT_STRING_CONVERSION_SIZE>::template ForElementType<FromType> AllocatorType;

public:
	FORCEINLINE TStringPassthru(ToType* InDest, int32 InDestLen, int32 InSrcLen)
		: Dest   (InDest)
		, DestLen(InDestLen)
		, SrcLen (InSrcLen)
	{
		AllocatorType::ResizeAllocation(0, SrcLen, sizeof(FromType));
	}

	FORCEINLINE TStringPassthru(TStringPassthru&& Other)
	{
		AllocatorType::MoveToEmpty(Other);
	}

	FORCEINLINE void Apply() const
	{
		const FromType* Source = (const FromType*)AllocatorType::GetAllocation();
		check(FPlatformString::ConvertedLength<ToType>(Source, SrcLen) <= DestLen);
		FPlatformString::Convert(Dest, DestLen, Source, SrcLen);
	}

	FORCEINLINE FromType* Get()
	{
		return (FromType*)AllocatorType::GetAllocation();
	}

private:
	// Non-copyable
	TStringPassthru(const TStringPassthru&);
	TStringPassthru& operator=(const TStringPassthru&);

	ToType* Dest;
	int32   DestLen;
	int32   SrcLen;
};

// This seemingly-pointless class is intended to be API-compatible with TStringPassthru
// and is returned by StringPassthru when no string conversion is necessary.
template <typename T>
class TPassthruPointer
{
public:
	FORCEINLINE explicit TPassthruPointer(T* InPtr)
		: Ptr(InPtr)
	{
	}

	FORCEINLINE T* Get() const
	{
		return Ptr;
	}

	FORCEINLINE void Apply() const
	{
	}

private:
	T* Ptr;
};

/**
 * Allows the efficient conversion of strings by means of a temporary memory buffer only when necessary.  Intended to be used
 * when you have an API which populates a buffer with some string representation which is ultimately going to be stored in another
 * representation, but where you don't want to do a conversion or create a temporary buffer for that string if it's not necessary.
 *
 * Intended use:
 *
 * // Populates the buffer Str with StrLen characters.
 * void SomeAPI(APICharType* Str, int32 StrLen);
 *
 * void Func(DestChar* Buffer, int32 BufferSize)
 * {
 *     // Create a passthru.  This takes the buffer (and its size) which will ultimately hold the string, as well as the length of the
 *     // string that's being converted, which must be known in advance.
 *     // An explicit template argument is also passed to indicate the character type of the source string.
 *     // Buffer must be correctly typed for the destination string type.
 *     auto Passthru = StringMemoryPassthru<APICharType>(Buffer, BufferSize, SourceLength);
 *
 *     // Passthru.Get() returns an APICharType buffer pointer which is guaranteed to be SourceLength characters in size.
 *     // It's possible, and in fact intended, for Get() to return the same pointer as Buffer if DestChar and APICharType are
 *     // compatible string types.  If this is the case, SomeAPI will write directly into Buffer.  If the string types are not
 *     // compatible, Get() will return a pointer to some temporary memory which allocated by and owned by the passthru.
 *     SomeAPI(Passthru.Get(), SourceLength);
 *
 *     // If the string types were not compatible, then the passthru used temporary storage, and we need to write that back to Buffer.
 *     // We do that with the Apply call.  If the string types were compatible, then the data was already written to Buffer directly
 *     // and so Apply is a no-op.
 *     Passthru.Apply();
 *
 *     // Now Buffer holds the data output by SomeAPI, already converted if necessary.
 * }
 */
template <typename From, typename To>
FORCEINLINE typename TEnableIf<FPlatformString::TAreEncodingsCompatible<To, From>::Value, TPassthruPointer<From>>::Type StringMemoryPassthru(To* Buffer, int32 BufferSize, int32 SourceLength)
{
	check(SourceLength <= BufferSize);
	return TPassthruPointer<From>((From*)Buffer);
}

template <typename From, typename To>
FORCEINLINE typename TEnableIf<!FPlatformString::TAreEncodingsCompatible<To, From>::Value, TStringPassthru<To, From>>::Type StringMemoryPassthru(To* Buffer, int32 BufferSize, int32 SourceLength)
{
	return TStringPassthru<To, From>(Buffer, BufferSize, SourceLength);
}

template <typename ToType, typename FromType>
FORCEINLINE TArray<ToType> StringToArray(const FromType* Src, int32 SrcLen)
{
	int32 DestLen = FPlatformString::ConvertedLength<TCHAR>(Src, SrcLen);

	TArray<ToType> Result;
	Result.AddUninitialized(DestLen);
	FPlatformString::Convert(Result.GetData(), DestLen, Src, SrcLen);

	return Result;
}

template <typename ToType, typename FromType>
FORCEINLINE TArray<ToType> StringToArray(const FromType* Str)
{
	return ToArray(Str, TCString<FromType>::Strlen(Str) + 1);
}
