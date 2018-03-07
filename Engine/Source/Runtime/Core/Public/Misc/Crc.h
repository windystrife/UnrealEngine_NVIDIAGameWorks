// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Char.h"
#include "Misc/CString.h"


/** 
 * CRC hash generation for different types of input data
 **/
struct CORE_API FCrc
{
	/** lookup table with precalculated CRC values - slicing by 8 implementation */
	static uint32 CRCTablesSB8[8][256];

	/** initializes the CRC lookup table. Must be called before any of the
		CRC functions are used. */
	static void Init();

	/** generates CRC hash of the memory area */
	static uint32 MemCrc32( const void* Data, int32 Length, uint32 CRC=0 );

	/** String CRC. */
	template <typename CharType>
	static typename TEnableIf<sizeof(CharType) != 1, uint32>::Type StrCrc32(const CharType* Data, uint32 CRC = 0)
	{
		// We ensure that we never try to do a StrCrc32 with a CharType of more than 4 bytes.  This is because
		// we always want to treat every CRC as if it was based on 4 byte chars, even if it's less, because we
		// want consistency between equivalent strings with different character types.
		static_assert(sizeof(CharType) <= 4, "StrCrc32 only works with CharType up to 32 bits.");

		CRC = ~CRC;
		while (CharType Ch = *Data++)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
		}
		return ~CRC;
	}

	template <typename CharType>
	static typename TEnableIf<sizeof(CharType) == 1, uint32>::Type StrCrc32(const CharType* Data, uint32 CRC = 0)
	{
		/* Overload for when CharType is a byte, which causes warnings when right-shifting by 8 */
		CRC = ~CRC;
		while (CharType Ch = *Data++)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
		}
		return ~CRC;
	}

	/**
	 * DEPRECATED
	 * These tables and functions are deprecated because they're using tables and implementations
	 * which give values different from what a user of a typical CRC32 algorithm might expect.
	 */

	/** lookup table with precalculated CRC values */
	static uint32 CRCTable_DEPRECATED[256];
	/** lookup table with precalculated CRC values - slicing by 8 implementation */
	static uint32 CRCTablesSB8_DEPRECATED[8][256];

	/** String CRC. */
	template <typename CharType>
	static inline uint32 StrCrc_DEPRECATED(const CharType* Data)
	{
		// make sure table is initialized
		check(CRCTable_DEPRECATED[1] != 0);

		int32 Length = TCString<CharType>::Strlen( Data );
		uint32 CRC = 0xFFFFFFFF;
		for( int32 i=0; i<Length; i++ )
		{
			CharType C = Data[i];
			int32 CL = (C&255);
			CRC = (CRC << 8) ^ CRCTable_DEPRECATED[(CRC >> 24) ^ CL];
			int32 CH = (C>>8)&255;
			CRC = (CRC << 8) ^ CRCTable_DEPRECATED[(CRC >> 24) ^ CH];
		}
		return ~CRC;
	}

	/** Case insensitive string hash function. */
	template <typename CharType> static inline uint32 Strihash_DEPRECATED( const CharType* Data );

	/** generates CRC hash of the memory area */
	static uint32 MemCrc_DEPRECATED( const void* Data, int32 Length, uint32 CRC=0 );
};

template <>
inline uint32 FCrc::Strihash_DEPRECATED(const ANSICHAR* Data)
{
	// make sure table is initialized
	check(CRCTable_DEPRECATED[1] != 0);

	uint32 Hash=0;
	while( *Data )
	{
		ANSICHAR Ch = TChar<ANSICHAR>::ToUpper(*Data++);
		uint8 B  = Ch;
		Hash = ((Hash >> 8) & 0x00FFFFFF) ^ CRCTable_DEPRECATED[(Hash ^ B) & 0x000000FF];
	}
	return Hash;
}

template <>
inline uint32 FCrc::Strihash_DEPRECATED(const WIDECHAR* Data)
{
	// make sure table is initialized
	check(CRCTable_DEPRECATED[1] != 0);

	uint32 Hash=0;
	while( *Data )
	{
		WIDECHAR Ch = TChar<WIDECHAR>::ToUpper(*Data++);
		uint16  B  = Ch;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ CRCTable_DEPRECATED[(Hash ^ B) & 0x000000FF];
		B        = Ch>>8;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ CRCTable_DEPRECATED[(Hash ^ B) & 0x000000FF];
	}
	return Hash;
}
