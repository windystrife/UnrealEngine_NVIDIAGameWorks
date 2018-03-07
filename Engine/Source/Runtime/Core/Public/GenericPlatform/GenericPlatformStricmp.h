// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/EnableIf.h"
#include "Templates/UnrealTypeTraits.h"
#include "Misc/Char.h"


/**
 * This trait tells if given CharTypeA is comparison compatible with CharTypeB, i.e.
 * if CharTypeA contains whole CharTypeB character set on the same positions.
 *
 * This is true e.g. for WIDECHAR and ANSICHAR because first 256 characters of WIDECHAR is
 * basically ANSICHAR table.
 */
template<typename A, typename B>	struct TIsComparisonCompatibleChar							{ enum { Value = false }; };
template<>							struct TIsComparisonCompatibleChar<WIDECHAR, ANSICHAR>		{ enum { Value = true }; };
template<>							struct TIsComparisonCompatibleChar<UTF8CHAR, ANSICHAR>		{ enum { Value = true }; };
template<>							struct TIsComparisonCompatibleChar<UTF16CHAR, ANSICHAR>		{ enum { Value = true }; };
template<>							struct TIsComparisonCompatibleChar<UTF32CHAR, ANSICHAR>		{ enum { Value = true }; };

/**
 * Static struct that implements generic stricmp functionality.
 */
struct FGenericPlatformStricmp
{
private:
	/**
	 * Compares two strings case-insensitive assuming that both char types are
	 * compatible.
	 *
	 * @param String1 First string to compare.
	 * @param String2 Second string to compare.
	 * @returns Zero if both strings are equal. Greater than zero if first
	 *          string is greater than the second one. Less than zero
	 *          otherwise.
	 */
	template <typename CompatibleCharType1, typename CompatibleCharType2>
	static inline int32 CompatibleCharTypesStricmp(const CompatibleCharType1* String1, const CompatibleCharType2* String2)
	{
		// Walk the strings, comparing them case insensitively.
		for (; *String1 || *String2; String1++, String2++)
		{
			if(*String1 != *String2)
			{
				CompatibleCharType1 Char1 = TChar<CompatibleCharType1>::ToLower(*String1);
				CompatibleCharType2 Char2 = TChar<CompatibleCharType2>::ToLower(*String2);

				if (Char1 != Char2)
				{
					return Char1 - Char2;
				}
			}
		}
		return 0;
	}

	/**
	 * Helper function wrapper to verify compatibility. This overload checks if CharType1 == CharType2.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 StricmpImpl(const CharType1* String1, const CharType2* String2, typename TEnableIf<TIsSame<CharType1, CharType2>::Value>::Type* Dummy1 = nullptr)
	{
		return CompatibleCharTypesStricmp(String1, String2);
	}

	/**
	 * Helper function wrapper to verify compatibility. This overload checks if
	 * CharType1 is compatible with CharType2.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 StricmpImpl(const CharType1* String1, const CharType2* String2, typename TEnableIf<TIsComparisonCompatibleChar<CharType1, CharType2>::Value>::Type* Dummy = nullptr)
	{
		return CompatibleCharTypesStricmp(String1, String2);
	}

	/**
	 * Helper function wrapper to verify compatibility. This overload checks if
	 * CharType2 is compatible with CharType1. This is the mirrored version of
	 * above function, so we don't have to declare compatibility trait for both
	 * ways.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 StricmpImpl(const CharType1* String1, const CharType2* String2, typename TEnableIf<TIsComparisonCompatibleChar<CharType2, CharType1>::Value>::Type* Dummy = nullptr)
	{
		return CompatibleCharTypesStricmp(String1, String2);
	}

public:
	/**
	 * Compares two strings case-insensitive.
	 *
	 * @param String1 First string to compare.
	 * @param String2 Second string to compare.
	 * @returns Zero if both strings are equal. Greater than zero if first
	 *          string is greater than the second one. Less than zero
	 *          otherwise.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 Stricmp(const CharType1* String1, const CharType2* String2)
	{
		return FGenericPlatformStricmp::StricmpImpl(String1, String2);
	}
};
